#include "wave/project_io.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QSet>

#include <cmath>
#include <limits>
#include <utility>

namespace wave {
namespace {

constexpr auto kProjectFileName = "project.wave.json";
constexpr std::int64_t kLargestExactJsonInteger = 9'007'199'254'740'991LL;

QString qString(const std::string& value)
{
    return QString::fromUtf8(value);
}

QString latinString(const std::string_view value)
{
    return QString::fromLatin1(value.data(), static_cast<qsizetype>(value.size()));
}

std::string stdString(const QString& value)
{
    const auto utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

QJsonValue extensionValue(const std::string& encoded)
{
    QJsonParseError error;
    const auto wrapped = QByteArrayLiteral("[") + QByteArray::fromStdString(encoded)
        + QByteArrayLiteral("]");
    const auto document = QJsonDocument::fromJson(wrapped, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()
        || document.array().size() != 1) {
        return qString(encoded);
    }
    return document.array().at(0);
}

std::string encodeExtensionValue(const QJsonValue& value)
{
    const QJsonDocument document(QJsonArray{value});
    auto encoded = document.toJson(QJsonDocument::Compact);
    if (encoded.size() >= 2) {
        encoded.remove(0, 1);
        encoded.chop(1);
    }
    return encoded.toStdString();
}

void applyExtensions(QJsonObject& object, const JsonExtensions& extensions)
{
    for (const auto& [name, encoded] : extensions) {
        object.insert(qString(name), extensionValue(encoded));
    }
}

JsonExtensions captureExtensions(const QJsonObject& object, const QSet<QString>& known)
{
    JsonExtensions extensions;
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
        if (!known.contains(iterator.key())) {
            extensions.emplace(stdString(iterator.key()), encodeExtensionValue(iterator.value()));
        }
    }
    return extensions;
}

QJsonValue integerValue(const std::int64_t value)
{
    return QString::number(value);
}

std::optional<std::int64_t> readInteger(
    const QJsonValue& value,
    const QString& field,
    QString& error,
    const bool required = true)
{
    if (value.isUndefined() || value.isNull()) {
        if (required) {
            error = QStringLiteral("Missing integer field: %1").arg(field);
        }
        return std::nullopt;
    }

    if (value.isString()) {
        bool valid = false;
        const auto parsed = value.toString().toLongLong(&valid);
        if (!valid) {
            error = QStringLiteral("Invalid integer string in %1").arg(field);
            return std::nullopt;
        }
        return parsed;
    }

    if (value.isDouble()) {
        const auto number = value.toDouble();
        if (!std::isfinite(number) || std::floor(number) != number
            || std::abs(number) > static_cast<double>(kLargestExactJsonInteger)) {
            error = QStringLiteral("Unsafe JSON number in %1; integer strings are required")
                        .arg(field);
            return std::nullopt;
        }
        return static_cast<std::int64_t>(number);
    }

    error = QStringLiteral("Expected integer string in %1").arg(field);
    return std::nullopt;
}

std::optional<int> readInt(
    const QJsonValue& value,
    const QString& field,
    QString& error,
    const bool required = true)
{
    const auto parsed = readInteger(value, field, error, required);
    if (!parsed) {
        return std::nullopt;
    }
    if (*parsed < std::numeric_limits<int>::min()
        || *parsed > std::numeric_limits<int>::max()) {
        error = QStringLiteral("Integer out of range in %1").arg(field);
        return std::nullopt;
    }
    return static_cast<int>(*parsed);
}

std::optional<std::uint32_t> readWidth(
    const QJsonValue& value,
    const QString& field,
    QString& error)
{
    const auto parsed = readInteger(value, field, error);
    if (!parsed || *parsed < 1
        || static_cast<std::uint64_t>(*parsed) > std::numeric_limits<std::uint32_t>::max()) {
        if (error.isEmpty()) {
            error = QStringLiteral("Width out of range in %1").arg(field);
        }
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*parsed);
}

std::string readString(
    const QJsonObject& object,
    const QString& name,
    QString& error,
    const bool required = true,
    const std::string& fallback = {})
{
    const auto value = object.value(name);
    if (value.isUndefined() || value.isNull()) {
        if (required) {
            error = QStringLiteral("Missing string field: %1").arg(name);
        }
        return fallback;
    }
    if (!value.isString()) {
        error = QStringLiteral("Expected string field: %1").arg(name);
        return fallback;
    }
    return stdString(value.toString());
}

QJsonObject segmentToJson(const Segment& segment)
{
    QJsonObject object;
    applyExtensions(object, segment.extensions);
    object.insert(QStringLiteral("id"), qString(segment.id));
    object.insert(QStringLiteral("startTick"), integerValue(segment.start));
    object.insert(QStringLiteral("endTick"), integerValue(segment.end));
    object.insert(QStringLiteral("value"), qString(segment.value));
    return object;
}

std::optional<Segment> segmentFromJson(const QJsonValue& value, QString& error)
{
    if (!value.isObject()) {
        error = QStringLiteral("Segment must be an object");
        return std::nullopt;
    }
    const auto object = value.toObject();
    Segment segment;
    segment.id = readString(object, QStringLiteral("id"), error);
    if (!error.isEmpty()) return std::nullopt;
    const auto start = readInteger(object.value(QStringLiteral("startTick")), QStringLiteral("startTick"), error);
    if (!start) return std::nullopt;
    const auto end = readInteger(object.value(QStringLiteral("endTick")), QStringLiteral("endTick"), error);
    if (!end) return std::nullopt;
    segment.start = *start;
    segment.end = *end;
    segment.value = readString(object, QStringLiteral("value"), error);
    if (!error.isEmpty()) return std::nullopt;
    segment.extensions = captureExtensions(
        object,
        {"id", "startTick", "endTick", "value"});
    return segment;
}

QJsonObject laneToJson(const Lane& lane)
{
    QJsonObject object;
    applyExtensions(object, lane.extensions);
    object.insert(QStringLiteral("id"), qString(lane.id));
    object.insert(QStringLiteral("name"), qString(lane.name));
    object.insert(QStringLiteral("kind"), latinString(toString(lane.kind)));
    object.insert(QStringLiteral("width"), static_cast<qint64>(lane.width));
    object.insert(QStringLiteral("signed"), lane.isSigned);
    object.insert(QStringLiteral("radix"), latinString(toString(lane.radix)));
    object.insert(QStringLiteral("clockDomainId"), qString(lane.clockDomainId));
    object.insert(QStringLiteral("color"), qString(lane.color));
    object.insert(QStringLiteral("height"), lane.height);
    object.insert(QStringLiteral("visible"), lane.visible);
    object.insert(QStringLiteral("groupId"), qString(lane.groupId));

    QJsonObject enumMap;
    for (const auto& [name, mappedValue] : lane.enumMap) {
        enumMap.insert(qString(name), qString(mappedValue));
    }
    object.insert(QStringLiteral("enumMap"), enumMap);

    QJsonArray segments;
    for (const auto& segment : lane.segments) {
        segments.append(segmentToJson(segment));
    }
    object.insert(QStringLiteral("segments"), segments);
    return object;
}

std::optional<Lane> laneFromJson(const QJsonValue& value, QString& error)
{
    if (!value.isObject()) {
        error = QStringLiteral("Lane must be an object");
        return std::nullopt;
    }
    const auto object = value.toObject();
    Lane lane;
    lane.id = readString(object, QStringLiteral("id"), error);
    if (!error.isEmpty()) return std::nullopt;
    lane.name = readString(object, QStringLiteral("name"), error);
    if (!error.isEmpty()) return std::nullopt;
    const auto kindText = readString(object, QStringLiteral("kind"), error);
    if (!error.isEmpty()) return std::nullopt;
    const auto kind = laneKindFromString(kindText);
    if (!kind) {
        error = QStringLiteral("Unknown lane kind: %1").arg(qString(kindText));
        return std::nullopt;
    }
    lane.kind = *kind;

    const auto width = readWidth(object.value(QStringLiteral("width")), QStringLiteral("width"), error);
    if (!width) return std::nullopt;
    lane.width = *width;
    lane.isSigned = object.value(QStringLiteral("signed")).toBool(false);

    const auto radixText = readString(
        object,
        QStringLiteral("radix"),
        error,
        false,
        "hexadecimal");
    if (!error.isEmpty()) return std::nullopt;
    const auto radix = radixFromString(radixText);
    if (!radix) {
        error = QStringLiteral("Unknown radix: %1").arg(qString(radixText));
        return std::nullopt;
    }
    lane.radix = *radix;
    lane.clockDomainId = readString(
        object,
        QStringLiteral("clockDomainId"),
        error,
        false);
    if (!error.isEmpty()) return std::nullopt;
    lane.color = readString(
        object,
        QStringLiteral("color"),
        error,
        false,
        "#4fc3f7");
    if (!error.isEmpty()) return std::nullopt;

    const auto height = readInt(
        object.value(QStringLiteral("height")),
        QStringLiteral("height"),
        error,
        false);
    if (!error.isEmpty()) return std::nullopt;
    lane.height = height.value_or(56);
    lane.visible = object.contains(QStringLiteral("visible"))
        ? object.value(QStringLiteral("visible")).toBool(true)
        : true;
    lane.groupId = readString(object, QStringLiteral("groupId"), error, false);
    if (!error.isEmpty()) return std::nullopt;

    const auto enumValue = object.value(QStringLiteral("enumMap"));
    if (!enumValue.isUndefined()) {
        if (!enumValue.isObject()) {
            error = QStringLiteral("enumMap must be an object");
            return std::nullopt;
        }
        const auto enumObject = enumValue.toObject();
        for (auto iterator = enumObject.begin(); iterator != enumObject.end(); ++iterator) {
            if (!iterator.value().isString()) {
                error = QStringLiteral("enumMap values must be strings");
                return std::nullopt;
            }
            lane.enumMap.emplace(stdString(iterator.key()), stdString(iterator.value().toString()));
        }
    }

    const auto segmentValue = object.value(QStringLiteral("segments"));
    if (!segmentValue.isUndefined()) {
        if (!segmentValue.isArray()) {
            error = QStringLiteral("segments must be an array");
            return std::nullopt;
        }
        for (const auto& item : segmentValue.toArray()) {
            auto segment = segmentFromJson(item, error);
            if (!segment) return std::nullopt;
            lane.segments.push_back(std::move(*segment));
        }
    }

    lane.extensions = captureExtensions(
        object,
        {"id", "name", "kind", "width", "signed", "radix", "enumMap",
         "clockDomainId", "color", "height", "visible", "groupId", "segments"});
    try {
        normalizeSegments(lane, true);
    } catch (const std::exception& exception) {
        error = QStringLiteral("Invalid segments in lane %1: %2")
                    .arg(qString(lane.name), QString::fromUtf8(exception.what()));
        return std::nullopt;
    }
    return lane;
}

QJsonObject eventToJson(const Event& event)
{
    QJsonObject object;
    applyExtensions(object, event.extensions);
    object.insert(QStringLiteral("id"), qString(event.id));
    object.insert(QStringLiteral("laneId"), qString(event.laneId));
    object.insert(QStringLiteral("timeTick"), integerValue(event.tick));
    object.insert(QStringLiteral("action"), latinString(toString(event.action)));
    object.insert(QStringLiteral("value"), qString(event.value));
    object.insert(QStringLiteral("expectedResult"), qString(event.expectedResult));
    object.insert(QStringLiteral("clockDomainId"), qString(event.clockDomainId));
    if (event.cycle) {
        object.insert(QStringLiteral("cycle"), integerValue(*event.cycle));
    }
    object.insert(QStringLiteral("description"), qString(event.description));
    object.insert(QStringLiteral("linkedSegmentId"), qString(event.linkedSegmentId));
    object.insert(QStringLiteral("waveformLinked"), event.waveformLinked);
    return object;
}

std::optional<Event> eventFromJson(const QJsonValue& value, QString& error)
{
    if (!value.isObject()) {
        error = QStringLiteral("Event must be an object");
        return std::nullopt;
    }
    const auto object = value.toObject();
    Event event;
    event.id = readString(object, QStringLiteral("id"), error);
    if (!error.isEmpty()) return std::nullopt;
    event.laneId = readString(object, QStringLiteral("laneId"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    const auto tick = readInteger(object.value(QStringLiteral("timeTick")), QStringLiteral("timeTick"), error);
    if (!tick) return std::nullopt;
    event.tick = *tick;
    const auto actionText = readString(object, QStringLiteral("action"), error);
    if (!error.isEmpty()) return std::nullopt;
    const auto action = eventActionFromString(actionText);
    if (!action) {
        error = QStringLiteral("Unknown event action: %1").arg(qString(actionText));
        return std::nullopt;
    }
    event.action = *action;
    event.value = readString(object, QStringLiteral("value"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    event.expectedResult = readString(object, QStringLiteral("expectedResult"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    event.clockDomainId = readString(object, QStringLiteral("clockDomainId"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    if (object.contains(QStringLiteral("cycle"))) {
        const auto cycle = readInteger(object.value(QStringLiteral("cycle")), QStringLiteral("cycle"), error);
        if (!cycle) return std::nullopt;
        event.cycle = *cycle;
    }
    event.description = readString(object, QStringLiteral("description"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    event.linkedSegmentId = readString(
        object,
        QStringLiteral("linkedSegmentId"),
        error,
        false);
    if (!error.isEmpty()) return std::nullopt;
    event.waveformLinked = object.value(QStringLiteral("waveformLinked")).toBool(false);
    event.extensions = captureExtensions(
        object,
        {"id", "laneId", "timeTick", "action", "value", "expectedResult",
         "clockDomainId", "cycle", "description", "linkedSegmentId", "waveformLinked"});
    return event;
}

QJsonObject relationToJson(const Relation& relation)
{
    QJsonObject object;
    applyExtensions(object, relation.extensions);
    object.insert(QStringLiteral("id"), qString(relation.id));
    object.insert(QStringLiteral("sourceEventId"), qString(relation.sourceEventId));
    object.insert(QStringLiteral("targetEventId"), qString(relation.targetEventId));
    object.insert(QStringLiteral("minimumDelayTick"), integerValue(relation.minimumDelay));
    object.insert(QStringLiteral("maximumDelayTick"), integerValue(relation.maximumDelay));
    object.insert(QStringLiteral("clockDomainId"), qString(relation.clockDomainId));
    object.insert(QStringLiteral("condition"), qString(relation.condition));
    object.insert(QStringLiteral("severity"), latinString(toString(relation.severity)));
    object.insert(QStringLiteral("description"), qString(relation.description));
    return object;
}

std::optional<Relation> relationFromJson(const QJsonValue& value, QString& error)
{
    if (!value.isObject()) {
        error = QStringLiteral("Relation must be an object");
        return std::nullopt;
    }
    const auto object = value.toObject();
    Relation relation;
    relation.id = readString(object, QStringLiteral("id"), error);
    if (!error.isEmpty()) return std::nullopt;
    relation.sourceEventId = readString(object, QStringLiteral("sourceEventId"), error);
    if (!error.isEmpty()) return std::nullopt;
    relation.targetEventId = readString(object, QStringLiteral("targetEventId"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    const auto minimum = readInteger(
        object.value(QStringLiteral("minimumDelayTick")),
        QStringLiteral("minimumDelayTick"),
        error);
    if (!minimum) return std::nullopt;
    const auto maximum = readInteger(
        object.value(QStringLiteral("maximumDelayTick")),
        QStringLiteral("maximumDelayTick"),
        error);
    if (!maximum) return std::nullopt;
    relation.minimumDelay = *minimum;
    relation.maximumDelay = *maximum;
    relation.clockDomainId = readString(object, QStringLiteral("clockDomainId"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    relation.condition = readString(object, QStringLiteral("condition"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    const auto severityText = readString(
        object,
        QStringLiteral("severity"),
        error,
        false,
        "error");
    if (!error.isEmpty()) return std::nullopt;
    const auto severity = severityFromString(severityText);
    if (!severity) {
        error = QStringLiteral("Unknown relation severity: %1").arg(qString(severityText));
        return std::nullopt;
    }
    relation.severity = *severity;
    relation.description = readString(object, QStringLiteral("description"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    relation.extensions = captureExtensions(
        object,
        {"id", "sourceEventId", "targetEventId", "minimumDelayTick",
         "maximumDelayTick", "clockDomainId", "condition", "severity", "description"});
    return relation;
}

QJsonObject markerToJson(const Marker& marker)
{
    QJsonObject object;
    applyExtensions(object, marker.extensions);
    object.insert(QStringLiteral("id"), qString(marker.id));
    object.insert(QStringLiteral("name"), qString(marker.name));
    object.insert(QStringLiteral("startTick"), integerValue(marker.start));
    object.insert(QStringLiteral("endTick"), integerValue(marker.end));
    object.insert(QStringLiteral("kind"), latinString(toString(marker.kind)));
    object.insert(QStringLiteral("note"), qString(marker.note));
    return object;
}

std::optional<Marker> markerFromJson(const QJsonValue& value, QString& error)
{
    if (!value.isObject()) {
        error = QStringLiteral("Marker must be an object");
        return std::nullopt;
    }
    const auto object = value.toObject();
    Marker marker;
    marker.id = readString(object, QStringLiteral("id"), error);
    if (!error.isEmpty()) return std::nullopt;
    marker.name = readString(object, QStringLiteral("name"), error);
    if (!error.isEmpty()) return std::nullopt;
    const auto start = readInteger(object.value(QStringLiteral("startTick")), QStringLiteral("startTick"), error);
    if (!start) return std::nullopt;
    const auto end = readInteger(object.value(QStringLiteral("endTick")), QStringLiteral("endTick"), error);
    if (!end) return std::nullopt;
    marker.start = *start;
    marker.end = *end;
    const auto kindText = readString(object, QStringLiteral("kind"), error, false, "point");
    if (!error.isEmpty()) return std::nullopt;
    const auto kind = markerKindFromString(kindText);
    if (!kind) {
        error = QStringLiteral("Unknown marker kind: %1").arg(qString(kindText));
        return std::nullopt;
    }
    marker.kind = *kind;
    marker.note = readString(object, QStringLiteral("note"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    marker.extensions = captureExtensions(
        object,
        {"id", "name", "startTick", "endTick", "kind", "note"});
    return marker;
}

QJsonObject scenarioToJson(const Scenario& scenario)
{
    QJsonObject object;
    applyExtensions(object, scenario.extensions);
    object.insert(QStringLiteral("id"), qString(scenario.id));
    object.insert(QStringLiteral("name"), qString(scenario.name));
    object.insert(QStringLiteral("durationTick"), integerValue(scenario.duration));

    QJsonArray lanes;
    for (const auto& lane : scenario.lanes) lanes.append(laneToJson(lane));
    object.insert(QStringLiteral("lanes"), lanes);
    QJsonArray events;
    for (const auto& event : scenario.events) events.append(eventToJson(event));
    object.insert(QStringLiteral("events"), events);
    QJsonArray relations;
    for (const auto& relation : scenario.relations) relations.append(relationToJson(relation));
    object.insert(QStringLiteral("relations"), relations);
    QJsonArray markers;
    for (const auto& marker : scenario.markers) markers.append(markerToJson(marker));
    object.insert(QStringLiteral("markers"), markers);
    return object;
}

std::optional<Scenario> scenarioFromJson(const QJsonValue& value, QString& error)
{
    if (!value.isObject()) {
        error = QStringLiteral("Scenario must be an object");
        return std::nullopt;
    }
    const auto object = value.toObject();
    Scenario scenario;
    scenario.id = readString(object, QStringLiteral("id"), error);
    if (!error.isEmpty()) return std::nullopt;
    scenario.name = readString(object, QStringLiteral("name"), error);
    if (!error.isEmpty()) return std::nullopt;
    const auto duration = readInteger(
        object.value(QStringLiteral("durationTick")),
        QStringLiteral("durationTick"),
        error);
    if (!duration) return std::nullopt;
    scenario.duration = *duration;

    const auto readArray = [&object, &error](
                               const QString& name,
                               auto parser,
                               auto& destination) -> bool {
        const auto value = object.value(name);
        if (value.isUndefined()) {
            return true;
        }
        if (!value.isArray()) {
            error = QStringLiteral("%1 must be an array").arg(name);
            return false;
        }
        for (const auto& item : value.toArray()) {
            auto parsed = parser(item, error);
            if (!parsed) return false;
            destination.push_back(std::move(*parsed));
        }
        return true;
    };

    if (!readArray(QStringLiteral("lanes"), laneFromJson, scenario.lanes)
        || !readArray(QStringLiteral("events"), eventFromJson, scenario.events)
        || !readArray(QStringLiteral("relations"), relationFromJson, scenario.relations)
        || !readArray(QStringLiteral("markers"), markerFromJson, scenario.markers)) {
        return std::nullopt;
    }
    scenario.extensions = captureExtensions(
        object,
        {"id", "name", "durationTick", "lanes", "events", "relations", "markers"});
    return scenario;
}

QJsonObject clockToJson(const ClockDomain& clock)
{
    QJsonObject object;
    applyExtensions(object, clock.extensions);
    object.insert(QStringLiteral("id"), qString(clock.id));
    object.insert(QStringLiteral("name"), qString(clock.name));
    object.insert(QStringLiteral("periodTick"), integerValue(clock.period));
    object.insert(QStringLiteral("phaseTick"), integerValue(clock.phase));
    object.insert(QStringLiteral("dutyNumerator"), integerValue(clock.dutyCycle.numerator));
    object.insert(QStringLiteral("dutyDenominator"), integerValue(clock.dutyCycle.denominator));
    object.insert(QStringLiteral("activeEdge"), latinString(toString(clock.activeEdge)));
    object.insert(QStringLiteral("resetRelation"), qString(clock.resetRelation));
    return object;
}

std::optional<ClockDomain> clockFromJson(const QJsonValue& value, QString& error)
{
    if (!value.isObject()) {
        error = QStringLiteral("Clock domain must be an object");
        return std::nullopt;
    }
    const auto object = value.toObject();
    ClockDomain clock;
    clock.id = readString(object, QStringLiteral("id"), error);
    if (!error.isEmpty()) return std::nullopt;
    clock.name = readString(object, QStringLiteral("name"), error);
    if (!error.isEmpty()) return std::nullopt;
    const auto period = readInteger(object.value(QStringLiteral("periodTick")), QStringLiteral("periodTick"), error);
    if (!period) return std::nullopt;
    const auto phase = readInteger(object.value(QStringLiteral("phaseTick")), QStringLiteral("phaseTick"), error);
    if (!phase) return std::nullopt;
    const auto numerator = readInteger(
        object.value(QStringLiteral("dutyNumerator")),
        QStringLiteral("dutyNumerator"),
        error);
    if (!numerator) return std::nullopt;
    const auto denominator = readInteger(
        object.value(QStringLiteral("dutyDenominator")),
        QStringLiteral("dutyDenominator"),
        error);
    if (!denominator) return std::nullopt;
    clock.period = *period;
    clock.phase = *phase;
    clock.dutyCycle = {*numerator, *denominator};
    clock.dutyCycle.normalize();
    const auto edgeText = readString(object, QStringLiteral("activeEdge"), error, false, "rising");
    if (!error.isEmpty()) return std::nullopt;
    const auto edge = clockEdgeFromString(edgeText);
    if (!edge) {
        error = QStringLiteral("Unknown clock edge: %1").arg(qString(edgeText));
        return std::nullopt;
    }
    clock.activeEdge = *edge;
    clock.resetRelation = readString(object, QStringLiteral("resetRelation"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    clock.extensions = captureExtensions(
        object,
        {"id", "name", "periodTick", "phaseTick", "dutyNumerator",
         "dutyDenominator", "activeEdge", "resetRelation"});
    if (!clock.isValid()) {
        error = QStringLiteral("Clock domain %1 is invalid").arg(qString(clock.name));
        return std::nullopt;
    }
    return clock;
}

QJsonObject traceToJson(const ImportedTrace& trace)
{
    QJsonObject object;
    applyExtensions(object, trace.extensions);
    object.insert(QStringLiteral("id"), qString(trace.id));
    object.insert(QStringLiteral("path"), qString(trace.path));
    object.insert(QStringLiteral("format"), qString(trace.format));
    object.insert(QStringLiteral("offsetTick"), integerValue(trace.offset));
    QJsonObject mapping;
    for (const auto& [source, target] : trace.signalMapping) {
        mapping.insert(qString(source), qString(target));
    }
    object.insert(QStringLiteral("signalMapping"), mapping);
    return object;
}

std::optional<ImportedTrace> traceFromJson(const QJsonValue& value, QString& error)
{
    if (!value.isObject()) {
        error = QStringLiteral("Imported trace must be an object");
        return std::nullopt;
    }
    const auto object = value.toObject();
    ImportedTrace trace;
    trace.id = readString(object, QStringLiteral("id"), error);
    if (!error.isEmpty()) return std::nullopt;
    trace.path = readString(object, QStringLiteral("path"), error);
    if (!error.isEmpty()) return std::nullopt;
    trace.format = readString(object, QStringLiteral("format"), error);
    if (!error.isEmpty()) return std::nullopt;
    const auto offset = readInteger(
        object.value(QStringLiteral("offsetTick")),
        QStringLiteral("offsetTick"),
        error,
        false);
    if (!error.isEmpty()) return std::nullopt;
    trace.offset = offset.value_or(0);
    const auto mappingValue = object.value(QStringLiteral("signalMapping"));
    if (!mappingValue.isUndefined()) {
        if (!mappingValue.isObject()) {
            error = QStringLiteral("signalMapping must be an object");
            return std::nullopt;
        }
        const auto mappingObject = mappingValue.toObject();
        for (auto iterator = mappingObject.begin();
             iterator != mappingObject.end();
             ++iterator) {
            if (!iterator.value().isString()) {
                error = QStringLiteral("signalMapping values must be strings");
                return std::nullopt;
            }
            trace.signalMapping.emplace(
                stdString(iterator.key()),
                stdString(iterator.value().toString()));
        }
    }
    trace.extensions = captureExtensions(
        object,
        {"id", "path", "format", "offsetTick", "signalMapping"});
    return trace;
}

QJsonObject resourceToJson(const LinkedResource& resource)
{
    QJsonObject object;
    applyExtensions(object, resource.extensions);
    object.insert(QStringLiteral("kind"), qString(resource.kind));
    object.insert(QStringLiteral("path"), qString(resource.path));
    object.insert(QStringLiteral("stableId"), qString(resource.stableId));
    object.insert(QStringLiteral("contentHash"), qString(resource.contentHash));
    object.insert(QStringLiteral("summary"), qString(resource.summary));
    return object;
}

std::optional<LinkedResource> resourceFromJson(const QJsonValue& value, QString& error)
{
    if (!value.isObject()) {
        error = QStringLiteral("Linked resource must be an object");
        return std::nullopt;
    }
    const auto object = value.toObject();
    LinkedResource resource;
    resource.kind = readString(object, QStringLiteral("kind"), error);
    if (!error.isEmpty()) return std::nullopt;
    resource.path = readString(object, QStringLiteral("path"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    resource.stableId = readString(object, QStringLiteral("stableId"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    resource.contentHash = readString(object, QStringLiteral("contentHash"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    resource.summary = readString(object, QStringLiteral("summary"), error, false);
    if (!error.isEmpty()) return std::nullopt;
    resource.extensions = captureExtensions(
        object,
        {"kind", "path", "stableId", "contentHash", "summary"});
    return resource;
}

QJsonObject extensionsToObject(const JsonExtensions& extensions)
{
    QJsonObject object;
    applyExtensions(object, extensions);
    return object;
}

JsonExtensions objectToExtensions(const QJsonValue& value, QString& error)
{
    if (value.isUndefined()) {
        return {};
    }
    if (!value.isObject()) {
        error = QStringLiteral("exportSettings must be an object");
        return {};
    }
    return captureExtensions(value.toObject(), {});
}

void ensureStableIds(QJsonObject& root)
{
    if (!root.value(QStringLiteral("projectId")).isString()
        || root.value(QStringLiteral("projectId")).toString().isEmpty()) {
        root.insert(QStringLiteral("projectId"), qString(makeStableId("project")));
    }
    auto scenarios = root.value(QStringLiteral("scenarios")).toArray();
    for (auto scenarioIndex = 0; scenarioIndex < scenarios.size(); ++scenarioIndex) {
        auto scenario = scenarios.at(scenarioIndex).toObject();
        if (!scenario.value(QStringLiteral("id")).isString()
            || scenario.value(QStringLiteral("id")).toString().isEmpty()) {
            scenario.insert(QStringLiteral("id"), qString(makeStableId("scenario")));
        }
        if (scenario.contains(QStringLiteral("duration"))
            && !scenario.contains(QStringLiteral("durationTick"))) {
            scenario.insert(QStringLiteral("durationTick"), scenario.take(QStringLiteral("duration")));
        }
        auto lanes = scenario.value(QStringLiteral("lanes")).toArray();
        for (auto laneIndex = 0; laneIndex < lanes.size(); ++laneIndex) {
            auto lane = lanes.at(laneIndex).toObject();
            if (!lane.value(QStringLiteral("id")).isString()
                || lane.value(QStringLiteral("id")).toString().isEmpty()) {
                lane.insert(QStringLiteral("id"), qString(makeStableId("lane")));
            }
            if (lane.contains(QStringLiteral("displayName"))
                && !lane.contains(QStringLiteral("name"))) {
                lane.insert(QStringLiteral("name"), lane.take(QStringLiteral("displayName")));
            }
            if (!lane.contains(QStringLiteral("width"))) lane.insert(QStringLiteral("width"), 1);
            auto segments = lane.value(QStringLiteral("segments")).toArray();
            for (auto segmentIndex = 0; segmentIndex < segments.size(); ++segmentIndex) {
                auto segment = segments.at(segmentIndex).toObject();
                if (!segment.value(QStringLiteral("id")).isString()
                    || segment.value(QStringLiteral("id")).toString().isEmpty()) {
                    segment.insert(QStringLiteral("id"), qString(makeStableId("segment")));
                }
                if (segment.contains(QStringLiteral("start"))
                    && !segment.contains(QStringLiteral("startTick"))) {
                    segment.insert(QStringLiteral("startTick"), segment.take(QStringLiteral("start")));
                }
                if (segment.contains(QStringLiteral("end"))
                    && !segment.contains(QStringLiteral("endTick"))) {
                    segment.insert(QStringLiteral("endTick"), segment.take(QStringLiteral("end")));
                }
                segments.replace(segmentIndex, segment);
            }
            lane.insert(QStringLiteral("segments"), segments);
            lanes.replace(laneIndex, lane);
        }
        scenario.insert(QStringLiteral("lanes"), lanes);
        scenarios.replace(scenarioIndex, scenario);
    }
    root.insert(QStringLiteral("scenarios"), scenarios);
}

bool migrateRoot(QJsonObject& root, const int version, QStringList& warnings)
{
    if (version == Project::CurrentSchemaVersion) {
        return false;
    }
    if (version != 0) {
        return false;
    }

    if (root.contains(QStringLiteral("id")) && !root.contains(QStringLiteral("projectId"))) {
        root.insert(QStringLiteral("projectId"), root.take(QStringLiteral("id")));
    }
    auto timebase = root.value(QStringLiteral("timebase")).toObject();
    if (timebase.contains(QStringLiteral("baseUnitPs"))
        && !timebase.contains(QStringLiteral("picosecondsPerTick"))) {
        timebase.insert(
            QStringLiteral("picosecondsPerTick"),
            timebase.take(QStringLiteral("baseUnitPs")));
    }
    if (!timebase.contains(QStringLiteral("picosecondsPerTick"))) {
        timebase.insert(QStringLiteral("picosecondsPerTick"), QStringLiteral("1"));
    }
    root.insert(QStringLiteral("timebase"), timebase);
    ensureStableIds(root);
    root.insert(QStringLiteral("schemaVersion"), Project::CurrentSchemaVersion);
    warnings.append(QStringLiteral("Migrated project schema from version 0 to version 1"));
    return true;
}

QJsonObject projectToJson(const Project& project)
{
    QJsonObject root;
    applyExtensions(root, project.extensions);
    root.insert(QStringLiteral("schemaVersion"), project.schemaVersion);
    root.insert(QStringLiteral("projectId"), qString(project.id));
    root.insert(QStringLiteral("name"), qString(project.name));
    root.insert(
        QStringLiteral("timebase"),
        QJsonObject{{QStringLiteral("picosecondsPerTick"),
                     integerValue(project.timeBase.picosecondsPerTick)}});

    QJsonArray clocks;
    for (const auto& clock : project.clockDomains) clocks.append(clockToJson(clock));
    root.insert(QStringLiteral("clockDomains"), clocks);
    QJsonArray scenarios;
    for (const auto& scenario : project.scenarios) scenarios.append(scenarioToJson(scenario));
    root.insert(QStringLiteral("scenarios"), scenarios);
    QJsonArray traces;
    for (const auto& trace : project.importedTraces) traces.append(traceToJson(trace));
    root.insert(QStringLiteral("importedTraces"), traces);
    QJsonArray resources;
    for (const auto& resource : project.linkedResources) resources.append(resourceToJson(resource));
    root.insert(QStringLiteral("linkedResources"), resources);
    root.insert(QStringLiteral("exportSettings"), extensionsToObject(project.exportSettings));
    return root;
}

std::optional<Project> projectFromJson(const QJsonObject& root, QString& error)
{
    Project project;
    const auto version = readInt(
        root.value(QStringLiteral("schemaVersion")),
        QStringLiteral("schemaVersion"),
        error);
    if (!version) return std::nullopt;
    project.schemaVersion = *version;
    project.id = readString(root, QStringLiteral("projectId"), error);
    if (!error.isEmpty()) return std::nullopt;
    project.name = readString(root, QStringLiteral("name"), error);
    if (!error.isEmpty()) return std::nullopt;

    const auto timebaseValue = root.value(QStringLiteral("timebase"));
    if (!timebaseValue.isObject()) {
        error = QStringLiteral("timebase must be an object");
        return std::nullopt;
    }
    const auto picoseconds = readInteger(
        timebaseValue.toObject().value(QStringLiteral("picosecondsPerTick")),
        QStringLiteral("timebase.picosecondsPerTick"),
        error);
    if (!picoseconds) return std::nullopt;
    project.timeBase.picosecondsPerTick = *picoseconds;
    if (!project.timeBase.isValid()) {
        error = QStringLiteral("timebase.picosecondsPerTick must be positive");
        return std::nullopt;
    }

    const auto readArray = [&root, &error](
                               const QString& name,
                               auto parser,
                               auto& destination) -> bool {
        const auto value = root.value(name);
        if (value.isUndefined()) return true;
        if (!value.isArray()) {
            error = QStringLiteral("%1 must be an array").arg(name);
            return false;
        }
        for (const auto& item : value.toArray()) {
            auto parsed = parser(item, error);
            if (!parsed) return false;
            destination.push_back(std::move(*parsed));
        }
        return true;
    };
    if (!readArray(QStringLiteral("clockDomains"), clockFromJson, project.clockDomains)
        || !readArray(QStringLiteral("scenarios"), scenarioFromJson, project.scenarios)
        || !readArray(QStringLiteral("importedTraces"), traceFromJson, project.importedTraces)
        || !readArray(QStringLiteral("linkedResources"), resourceFromJson, project.linkedResources)) {
        return std::nullopt;
    }

    project.exportSettings = objectToExtensions(
        root.value(QStringLiteral("exportSettings")),
        error);
    if (!error.isEmpty()) return std::nullopt;
    project.extensions = captureExtensions(
        root,
        {"schemaVersion", "projectId", "name", "timebase", "clockDomains",
         "scenarios", "importedTraces", "linkedResources", "exportSettings"});
    return project;
}

} // namespace

QByteArray serializeProject(const Project& project)
{
    return QJsonDocument(projectToJson(project)).toJson(QJsonDocument::Indented);
}

ProjectLoadResult deserializeProject(const QByteArray& data)
{
    ProjectLoadResult result;
    QJsonParseError parseError;
    auto document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.error = QStringLiteral("JSON parse error at offset %1: %2")
                           .arg(parseError.offset)
                           .arg(parseError.errorString());
        return result;
    }
    if (!document.isObject()) {
        result.error = QStringLiteral("Project root must be a JSON object");
        return result;
    }

    auto root = document.object();
    QString versionError;
    auto version = readInt(
        root.value(QStringLiteral("schemaVersion")),
        QStringLiteral("schemaVersion"),
        versionError,
        false);
    if (!version && !versionError.isEmpty()) {
        result.error = versionError;
        return result;
    }
    const auto sourceVersion = version.value_or(0);
    if (sourceVersion > Project::CurrentSchemaVersion) {
        result.error = QStringLiteral("Project schema version %1 is newer than supported version %2")
                           .arg(sourceVersion)
                           .arg(Project::CurrentSchemaVersion);
        return result;
    }
    if (sourceVersion < Project::CurrentSchemaVersion) {
        result.migrated = migrateRoot(root, sourceVersion, result.warnings);
        if (!result.migrated) {
            result.error = QStringLiteral("No migration path exists from schema version %1")
                               .arg(sourceVersion);
            return result;
        }
    }

    QString modelError;
    auto project = projectFromJson(root, modelError);
    if (!project) {
        result.error = modelError;
        return result;
    }
    result.project = std::move(project);
    return result;
}

ProjectLoadResult loadProjectFile(const QString& filePath)
{
    ProjectLoadResult result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Cannot open %1: %2").arg(filePath, file.errorString());
        return result;
    }
    result = deserializeProject(file.readAll());
    if (!result.ok() && !result.error.isEmpty()) {
        result.error = QStringLiteral("%1: %2").arg(filePath, result.error);
    }
    return result;
}

bool saveProjectFileAtomic(
    const Project& project,
    const QString& filePath,
    QString* error)
{
    QSaveFile file(filePath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open %1 for writing: %2")
                         .arg(filePath, file.errorString());
        }
        return false;
    }
    const auto data = serializeProject(project);
    if (file.write(data) != data.size()) {
        if (error) {
            *error = QStringLiteral("Cannot write %1: %2").arg(filePath, file.errorString());
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error) {
            *error = QStringLiteral("Cannot atomically replace %1: %2")
                         .arg(filePath, file.errorString());
        }
        return false;
    }
    return true;
}

ProjectLoadResult loadProjectDirectory(const QString& directoryPath)
{
    return loadProjectFile(projectFilePath(directoryPath));
}

bool saveProjectDirectory(
    const Project& project,
    const QString& directoryPath,
    QString* error)
{
    QDir directory;
    if (!directory.mkpath(directoryPath)) {
        if (error) {
            *error = QStringLiteral("Cannot create project directory: %1").arg(directoryPath);
        }
        return false;
    }
    const QDir projectDirectory(directoryPath);
    for (const auto* child : {"traces", "generated", "exports"}) {
        if (!projectDirectory.mkpath(QString::fromLatin1(child))) {
            if (error) {
                *error = QStringLiteral("Cannot create project subdirectory: %1")
                             .arg(projectDirectory.filePath(QString::fromLatin1(child)));
            }
            return false;
        }
    }
    return saveProjectFileAtomic(project, projectFilePath(directoryPath), error);
}

QString projectFilePath(const QString& directoryPath)
{
    return QDir(directoryPath).filePath(QString::fromLatin1(kProjectFileName));
}

} // namespace wave
