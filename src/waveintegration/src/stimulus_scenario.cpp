#include "wave/stimulus_scenario.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

#include <algorithm>
#include <charconv>
#include <exception>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace wave {
namespace {

QString qString(const std::string& value)
{
    return QString::fromUtf8(value);
}

std::string stdString(const QString& value)
{
    return value.toUtf8().toStdString();
}

QString latinString(const std::string_view value)
{
    return QString::fromLatin1(value.data(), static_cast<qsizetype>(value.size()));
}

bool exactKeys(
    const QJsonObject& object,
    const std::initializer_list<QString> keys,
    const QString& context,
    QString& error)
{
    QSet<QString> expected;
    for (const auto& key : keys) {
        expected.insert(key);
        if (!object.contains(key)) {
            error = QStringLiteral("%1.%2 is required").arg(context, key);
            return false;
        }
    }
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
        if (!expected.contains(iterator.key())) {
            error = QStringLiteral("%1 contains unsupported property %2")
                        .arg(context, iterator.key());
            return false;
        }
    }
    return true;
}

bool readString(
    const QJsonObject& object,
    const QString& key,
    const QString& context,
    std::string& output,
    QString& error,
    const bool allowEmpty = false)
{
    const auto value = object.value(key);
    if (!value.isString() || (!allowEmpty && value.toString().isEmpty())) {
        error = QStringLiteral("%1.%2 must be %3 string")
                    .arg(context, key, allowEmpty ? QStringLiteral("a")
                                                  : QStringLiteral("a non-empty"));
        return false;
    }
    output = stdString(value.toString());
    return true;
}

bool readBool(
    const QJsonObject& object,
    const QString& key,
    const QString& context,
    bool& output,
    QString& error)
{
    const auto value = object.value(key);
    if (!value.isBool()) {
        error = QStringLiteral("%1.%2 must be boolean").arg(context, key);
        return false;
    }
    output = value.toBool();
    return true;
}

bool readUnsigned(
    const QJsonObject& object,
    const QString& key,
    const QString& context,
    std::uint64_t& output,
    QString& error,
    const std::uint64_t minimum = 0)
{
    const auto value = object.value(key);
    const auto number = value.toDouble(-1.0);
    if (!value.isDouble() || number < 0.0
        || number > 9'007'199'254'740'991.0
        || number != static_cast<double>(static_cast<std::uint64_t>(number))) {
        error = QStringLiteral("%1.%2 must be a non-negative safe integer")
                    .arg(context, key);
        return false;
    }
    output = static_cast<std::uint64_t>(number);
    if (output < minimum) {
        error = QStringLiteral("%1.%2 must be at least %3")
                    .arg(context, key)
                    .arg(minimum);
        return false;
    }
    return true;
}

bool readSignedDecimalString(
    const QJsonObject& object,
    const QString& key,
    const QString& context,
    Tick& output,
    QString& error,
    const bool positive)
{
    std::string text;
    if (!readString(object, key, context, text, error)) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), output);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()
        || (positive && output <= 0)
        || QString::number(output) != qString(text)) {
        error = QStringLiteral("%1.%2 must be %3 decimal int64 string")
                    .arg(context, key, positive ? QStringLiteral("a positive")
                                                : QStringLiteral("an"));
        return false;
    }
    return true;
}

std::optional<QJsonValue> extensionValue(
    const JsonExtensions& extensions,
    const std::string_view key)
{
    const auto iterator = extensions.find(std::string(key));
    if (iterator == extensions.end()) return std::nullopt;
    QJsonParseError parseError;
    const auto wrapped = QByteArray("[")
        + QByteArray::fromStdString(iterator->second) + QByteArray("]");
    const auto document = QJsonDocument::fromJson(wrapped, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()
        || document.array().size() != 1) {
        return std::nullopt;
    }
    return document.array().at(0);
}

std::optional<std::string> extensionString(
    const JsonExtensions& extensions,
    const std::string_view key)
{
    const auto value = extensionValue(extensions, key);
    if (!value || !value->isString()) return std::nullopt;
    return stdString(value->toString());
}

std::optional<std::uint64_t> extensionUnsigned(
    const JsonExtensions& extensions,
    const std::string_view key)
{
    const auto value = extensionValue(extensions, key);
    if (!value || !value->isDouble()) return std::nullopt;
    const auto number = value->toDouble(-1.0);
    if (number < 0.0 || number > 9'007'199'254'740'991.0
        || number != static_cast<double>(static_cast<std::uint64_t>(number))) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(number);
}

std::optional<bool> extensionBool(
    const JsonExtensions& extensions,
    const std::string_view key)
{
    const auto value = extensionValue(extensions, key);
    if (!value || !value->isBool()) return std::nullopt;
    return value->toBool();
}

std::string jsonStringValue(const std::string& value)
{
    QJsonArray array;
    array.append(qString(value));
    auto encoded = QJsonDocument(array).toJson(QJsonDocument::Compact);
    encoded.remove(0, 1);
    encoded.chop(1);
    return encoded.toStdString();
}

std::string stableDigestId(
    const std::string_view prefix,
    const std::string_view first,
    const std::string_view second)
{
    QByteArray source(first.data(), static_cast<qsizetype>(first.size()));
    source.append('\0');
    source.append(second.data(), static_cast<qsizetype>(second.size()));
    const auto digest = QCryptographicHash::hash(source, QCryptographicHash::Sha256)
                            .toHex()
                            .left(24);
    return std::string(prefix) + "-" + digest.toStdString();
}

std::optional<ModuleManifestTargetMode> targetModeFromString(
    const std::string_view value) noexcept
{
    if (value == "module-definition") return ModuleManifestTargetMode::ModuleDefinition;
    if (value == "instance") return ModuleManifestTargetMode::Instance;
    return std::nullopt;
}

std::string_view targetModeString(const ModuleManifestTargetMode value) noexcept
{
    return value == ModuleManifestTargetMode::Instance
        ? "instance" : "module-definition";
}

std::optional<ModulePortDirection> directionFromString(
    const std::string_view value) noexcept
{
    if (value == "input") return ModulePortDirection::Input;
    if (value == "output") return ModulePortDirection::Output;
    if (value == "inout") return ModulePortDirection::Inout;
    if (value == "ref") return ModulePortDirection::Ref;
    if (value == "interface") return ModulePortDirection::Interface;
    if (value == "unknown") return ModulePortDirection::Unknown;
    return std::nullopt;
}

std::optional<StimulusPortRole> roleFromString(
    const std::string_view value) noexcept
{
    if (value == "stimulus") return StimulusPortRole::Stimulus;
    if (value == "watch") return StimulusPortRole::Watch;
    if (value == "stimulus-watch") return StimulusPortRole::StimulusWatch;
    return std::nullopt;
}

std::optional<StimulusResetActiveLevel> resetActiveLevelFromString(
    const std::string_view value) noexcept
{
    if (value == "unspecified") return StimulusResetActiveLevel::Unspecified;
    if (value == "low") return StimulusResetActiveLevel::Low;
    if (value == "high") return StimulusResetActiveLevel::High;
    return std::nullopt;
}

std::optional<StimulusResetSynchronization> resetSynchronizationFromString(
    const std::string_view value) noexcept
{
    if (value == "unspecified") return StimulusResetSynchronization::Unspecified;
    if (value == "synchronous") return StimulusResetSynchronization::Synchronous;
    if (value == "asynchronous") return StimulusResetSynchronization::Asynchronous;
    return std::nullopt;
}

bool roleMatchesDirection(
    const StimulusPortRole role,
    const ModulePortDirection direction) noexcept
{
    switch (direction) {
    case ModulePortDirection::Input:
        return role == StimulusPortRole::Stimulus;
    case ModulePortDirection::Output:
        return role == StimulusPortRole::Watch;
    case ModulePortDirection::Inout:
    case ModulePortDirection::Ref:
        return role == StimulusPortRole::StimulusWatch;
    case ModulePortDirection::Interface:
    case ModulePortDirection::Unknown:
        return false;
    }
    return false;
}

bool hasStimulus(const StimulusPortRole role) noexcept
{
    return role == StimulusPortRole::Stimulus
        || role == StimulusPortRole::StimulusWatch;
}

QJsonObject bindingToJson(const StimulusPortBinding& binding)
{
    return {
        {QStringLiteral("name"), qString(binding.name)},
        {QStringLiteral("direction"), latinString(toString(binding.direction))},
        {QStringLiteral("canonicalTypeId"), qString(binding.canonicalTypeId)},
        {QStringLiteral("declarationShapeId"), qString(binding.declarationShapeId)},
        {QStringLiteral("width"), static_cast<qint64>(binding.width)},
        {QStringLiteral("signed"), binding.isSigned},
        {QStringLiteral("sourceOrder"), static_cast<qint64>(binding.sourceOrder)},
    };
}

QJsonObject rangeToJson(const StimulusRange& range)
{
    return {
        {QStringLiteral("startTick"), QString::number(range.start)},
        {QStringLiteral("endTick"), QString::number(range.end)},
        {QStringLiteral("value"), qString(range.value)},
    };
}

QJsonObject scenarioToJson(
    const ZeroSlackStimulusScenario& scenario,
    const bool includeIdentity)
{
    QJsonObject target{
        {QStringLiteral("mode"), latinString(targetModeString(scenario.target.mode))},
        {QStringLiteral("module"), qString(scenario.target.module)},
        {QStringLiteral("instancePath"), qString(scenario.target.instancePath)},
    };

    QJsonArray groups;
    for (const auto& group : scenario.groups) {
        groups.append(QJsonObject{
            {QStringLiteral("id"), qString(group.id)},
            {QStringLiteral("name"), qString(group.name)},
            {QStringLiteral("displayOrder"), static_cast<qint64>(group.displayOrder)},
            {QStringLiteral("visible"), group.visible},
        });
    }

    QJsonArray ports;
    for (const auto& port : scenario.ports) {
        QJsonObject enumMap;
        for (const auto& [name, value] : port.enumMap) {
            enumMap.insert(qString(name), qString(value));
        }
        QJsonArray ranges;
        for (const auto& range : port.segments) ranges.append(rangeToJson(range));

        QJsonValue clock = QJsonValue::Null;
        if (port.clock) {
            clock = QJsonObject{
                {QStringLiteral("periodTicks"), QString::number(port.clock->period)},
                {QStringLiteral("phaseTicks"), QString::number(port.clock->phase)},
                {QStringLiteral("dutyNumerator"),
                 QString::number(port.clock->dutyCycle.numerator)},
                {QStringLiteral("dutyDenominator"),
                 QString::number(port.clock->dutyCycle.denominator)},
                {QStringLiteral("activeEdge"), latinString(toString(port.clock->activeEdge))},
                {QStringLiteral("initialValue"),
                 QString(QChar::fromLatin1(port.clock->initialValue))},
            };
        }
        QJsonValue reset = QJsonValue::Null;
        if (port.reset) {
            reset = QJsonObject{
                {QStringLiteral("activeLevel"), latinString(toString(port.reset->activeLevel))},
                {QStringLiteral("synchronization"),
                 latinString(toString(port.reset->synchronization))},
            };
        }
        ports.append(QJsonObject{
            {QStringLiteral("laneId"), qString(port.laneId)},
            {QStringLiteral("displayOrder"), static_cast<qint64>(port.displayOrder)},
            {QStringLiteral("role"), latinString(toString(port.role))},
            {QStringLiteral("kind"), latinString(toString(port.kind))},
            {QStringLiteral("radix"), latinString(toString(port.radix))},
            {QStringLiteral("visible"), port.visible},
            {QStringLiteral("groupId"), qString(port.groupId)},
            {QStringLiteral("binding"), bindingToJson(port.binding)},
            {QStringLiteral("enumMap"), enumMap},
            {QStringLiteral("segments"), ranges},
            {QStringLiteral("clock"), clock},
            {QStringLiteral("reset"), reset},
        });
    }

    QJsonObject scenarioObject{
        {QStringLiteral("id"), qString(scenario.scenarioId)},
        {QStringLiteral("name"), qString(scenario.name)},
        {QStringLiteral("timeBasePicosecondsPerTick"),
         QString::number(scenario.timeBase.picosecondsPerTick)},
        {QStringLiteral("durationTicks"), QString::number(scenario.duration)},
        {QStringLiteral("groups"), groups},
        {QStringLiteral("ports"), ports},
    };
    if (scenario.schemaVersion >= 2) {
        QJsonArray markers;
        for (const auto& marker : scenario.markers) {
            markers.append(QJsonObject{
                {QStringLiteral("id"), qString(marker.id)},
                {QStringLiteral("name"), qString(marker.name)},
                {QStringLiteral("startTick"), QString::number(marker.start)},
                {QStringLiteral("endTick"), QString::number(marker.end)},
                {QStringLiteral("kind"), latinString(toString(marker.kind))},
                {QStringLiteral("note"), qString(marker.note)},
            });
        }
        scenarioObject.insert(QStringLiteral("markers"), markers);
        scenarioObject.insert(
            QStringLiteral("view"),
            QJsonObject{
                {QStringLiteral("selectedPortName"),
                 qString(scenario.view.selectedPortName)},
                {QStringLiteral("cursorTick"),
                 QString::number(scenario.view.cursorTick)},
                {QStringLiteral("visibleSpanTicks"),
                 QString::number(scenario.view.visibleSpanTicks)},
            });
    }
    QJsonObject root{
        {QStringLiteral("schemaVersion"), scenario.schemaVersion},
        {QStringLiteral("manifestSchemaVersion"), scenario.manifestSchemaVersion},
        {QStringLiteral("manifestIdentity"), qString(scenario.manifestIdentity)},
        {QStringLiteral("workspaceId"), qString(scenario.workspaceId)},
        {QStringLiteral("target"), target},
        {QStringLiteral("scenario"), scenarioObject},
    };
    if (includeIdentity) root.insert(QStringLiteral("identity"), qString(scenario.identity));
    return root;
}

std::string computeIdentity(const ZeroSlackStimulusScenario& scenario)
{
    const auto canonical = QJsonDocument(scenarioToJson(scenario, false))
                               .toJson(QJsonDocument::Compact);
    return "sha256:"
        + QCryptographicHash::hash(canonical, QCryptographicHash::Sha256)
              .toHex().toStdString();
}

bool parseBinding(
    const QJsonObject& object,
    const QString& context,
    StimulusPortBinding& binding,
    QString& error)
{
    if (!exactKeys(
            object,
            {QStringLiteral("name"), QStringLiteral("direction"),
             QStringLiteral("canonicalTypeId"), QStringLiteral("declarationShapeId"),
             QStringLiteral("width"), QStringLiteral("signed"),
             QStringLiteral("sourceOrder")},
            context,
            error)) {
        return false;
    }
    std::string directionText;
    std::uint64_t width = 0;
    std::uint64_t sourceOrder = 0;
    if (!readString(object, QStringLiteral("name"), context, binding.name, error)
        || !readString(object, QStringLiteral("direction"), context, directionText, error)
        || !readString(object, QStringLiteral("canonicalTypeId"), context,
                       binding.canonicalTypeId, error, true)
        || !readString(object, QStringLiteral("declarationShapeId"), context,
                       binding.declarationShapeId, error, true)
        || !readUnsigned(object, QStringLiteral("width"), context, width, error, 1)
        || width > std::numeric_limits<std::uint32_t>::max()
        || !readBool(object, QStringLiteral("signed"), context, binding.isSigned, error)
        || !readUnsigned(object, QStringLiteral("sourceOrder"), context,
                         sourceOrder, error)) {
        if (error.isEmpty()) error = QStringLiteral("%1.width is unsupported").arg(context);
        return false;
    }
    const auto direction = directionFromString(directionText);
    if (!direction || *direction == ModulePortDirection::Interface
        || *direction == ModulePortDirection::Unknown) {
        error = QStringLiteral("%1.direction is unsupported").arg(context);
        return false;
    }
    if (binding.canonicalTypeId.empty() && binding.declarationShapeId.empty()) {
        error = QStringLiteral("%1 requires a canonical or declaration type identity")
                    .arg(context);
        return false;
    }
    binding.direction = *direction;
    binding.width = static_cast<std::uint32_t>(width);
    binding.sourceOrder = static_cast<std::size_t>(sourceOrder);
    return true;
}

bool parseRange(
    const QJsonObject& object,
    const QString& context,
    StimulusRange& range,
    QString& error)
{
    return exactKeys(
               object,
               {QStringLiteral("startTick"), QStringLiteral("endTick"),
                QStringLiteral("value")},
               context,
               error)
        && readSignedDecimalString(object, QStringLiteral("startTick"), context,
                                   range.start, error, false)
        && readSignedDecimalString(object, QStringLiteral("endTick"), context,
                                   range.end, error, true)
        && readString(object, QStringLiteral("value"), context, range.value, error);
}

bool validatePortRanges(
    const StimulusScenarioPort& port,
    const Tick duration,
    const QString& context,
    QString& error)
{
    Lane lane;
    lane.id = port.laneId;
    lane.name = port.binding.name;
    lane.kind = port.kind;
    lane.width = port.binding.width;
    lane.isSigned = port.binding.isSigned;
    lane.enumMap = port.enumMap;
    for (std::size_t index = 0; index < port.segments.size(); ++index) {
        const auto& range = port.segments[index];
        if (range.start < 0 || range.end <= range.start || range.end > duration) {
            error = QStringLiteral("%1.segments[%2] is outside the scenario duration")
                        .arg(context).arg(index);
            return false;
        }
        lane.segments.push_back({
            "range-" + std::to_string(index), range.start, range.end, range.value, {}});
    }
    try {
        normalizeSegments(lane);
    } catch (const std::exception& exception) {
        error = QStringLiteral("%1.segments are invalid: %2")
                    .arg(context, QString::fromUtf8(exception.what()));
        return false;
    }
    if (port.kind != LaneKind::Clock && hasStimulus(port.role)) {
        Tick next = 0;
        for (const auto& segment : lane.segments) {
            if (segment.start != next) {
                error = QStringLiteral("%1 stimulus segments must cover the duration without gaps")
                            .arg(context);
                return false;
            }
            next = segment.end;
        }
        if (next != duration) {
            error = QStringLiteral("%1 stimulus segments must cover the duration without gaps")
                        .arg(context);
            return false;
        }
    }
    if (!hasStimulus(port.role) && !port.segments.empty()) {
        error = QStringLiteral("%1 watch-only port cannot contain stimulus segments")
                    .arg(context);
        return false;
    }
    return true;
}

std::optional<StimulusPortBinding> bindingFromLane(
    const Lane& lane,
    const std::size_t fallbackOrder,
    QStringList& diagnostics)
{
    const auto directionText = extensionString(
        lane.extensions, "waveSimulation.direction");
    const auto canonicalTypeId = extensionString(
        lane.extensions, "waveSimulation.canonicalTypeId");
    auto declarationShapeId = extensionString(
        lane.extensions, "waveSimulation.declarationShapeId");
    if (!directionText || !canonicalTypeId) return std::nullopt;
    const auto direction = directionFromString(*directionText);
    if (!direction || *direction == ModulePortDirection::Interface
        || *direction == ModulePortDirection::Unknown) {
        return std::nullopt;
    }
    if (!declarationShapeId || declarationShapeId->empty()) {
        declarationShapeId = canonicalTypeId;
        diagnostics.append(
            QStringLiteral("Port %1 used its canonical type as the legacy declaration shape binding.")
                .arg(qString(lane.name)));
    }
    const auto sourceOrder = extensionUnsigned(
        lane.extensions, "waveSimulation.sourceOrder");
    if (!sourceOrder) {
        diagnostics.append(
            QStringLiteral("Port %1 used its visible lane order as the legacy source order.")
                .arg(qString(lane.name)));
    }
    return StimulusPortBinding{
        lane.name,
        *direction,
        *canonicalTypeId,
        *declarationShapeId,
        lane.width,
        lane.isSigned,
        static_cast<std::size_t>(sourceOrder.value_or(fallbackOrder)),
    };
}

bool compatibleBinding(
    const StimulusScenarioPort& saved,
    const Lane& current)
{
    QStringList ignored;
    const auto binding = bindingFromLane(current, 0, ignored);
    const auto savedKind = saved.kind == LaneKind::Clock ? LaneKind::Bit : saved.kind;
    const auto currentKind = current.kind == LaneKind::Clock ? LaneKind::Bit : current.kind;
    if (!binding || saved.binding.direction != binding->direction
        || saved.binding.width != binding->width
        || saved.binding.isSigned != binding->isSigned
        || savedKind != currentKind
        || (savedKind == LaneKind::Enum && saved.enumMap != current.enumMap)) {
        return false;
    }
    if (!saved.binding.canonicalTypeId.empty() && !binding->canonicalTypeId.empty()) {
        return saved.binding.canonicalTypeId == binding->canonicalTypeId;
    }
    return !saved.binding.declarationShapeId.empty()
        && saved.binding.declarationShapeId == binding->declarationShapeId;
}

bool sameBindingExceptWidth(
    const StimulusScenarioPort& saved,
    const Lane& current)
{
    QStringList ignored;
    const auto binding = bindingFromLane(current, 0, ignored);
    const auto savedKind = saved.kind == LaneKind::Clock ? LaneKind::Bit : saved.kind;
    const auto currentKind = current.kind == LaneKind::Clock ? LaneKind::Bit : current.kind;
    return binding
        && saved.binding.direction == binding->direction
        && saved.binding.isSigned == binding->isSigned
        && saved.binding.width != binding->width
        && savedKind == currentKind
        && (savedKind != LaneKind::Enum || saved.enumMap == current.enumMap);
}

Lane makeGroupLane(const StimulusScenarioGroup& group)
{
    Lane lane;
    lane.id = group.id;
    lane.name = group.name;
    lane.kind = LaneKind::Group;
    lane.color = "#90a4ae";
    lane.height = 40;
    lane.visible = group.visible;
    return lane;
}

} // namespace

QByteArray serializeZeroSlackStimulusScenario(
    const ZeroSlackStimulusScenario& scenario)
{
    auto normalized = scenario;
    normalized.identity = computeIdentity(normalized);
    return QJsonDocument(scenarioToJson(normalized, true))
        .toJson(QJsonDocument::Indented);
}

StimulusScenarioParseResult parseZeroSlackStimulusScenario(
    const QByteArray& document)
{
    StimulusScenarioParseResult result;
    QJsonParseError parseError;
    const auto parsed = QJsonDocument::fromJson(document, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        result.error = QStringLiteral("Invalid ZeroSlack Stimulus Scenario JSON: %1")
                           .arg(parseError.errorString());
        return result;
    }
    const auto root = parsed.object();
    QString error;
    if (!exactKeys(
            root,
            {QStringLiteral("schemaVersion"), QStringLiteral("manifestSchemaVersion"),
             QStringLiteral("identity"), QStringLiteral("manifestIdentity"),
             QStringLiteral("workspaceId"), QStringLiteral("target"),
             QStringLiteral("scenario")},
            QStringLiteral("stimulus"),
            error)) {
        result.error = error;
        return result;
    }

    ZeroSlackStimulusScenario scenario;
    std::uint64_t schemaVersion = 0;
    std::uint64_t manifestSchemaVersion = 0;
    if (!readUnsigned(root, QStringLiteral("schemaVersion"), QStringLiteral("stimulus"),
                      schemaVersion, error, 1)
        || schemaVersion
               < ZeroSlackStimulusScenario::MinimumSupportedSchemaVersion
        || schemaVersion > ZeroSlackStimulusScenario::CurrentSchemaVersion
        || !readUnsigned(root, QStringLiteral("manifestSchemaVersion"),
                         QStringLiteral("stimulus"), manifestSchemaVersion, error, 1)
        || manifestSchemaVersion != ZeroSlackModuleManifest::CurrentSchemaVersion) {
        result.error = error.isEmpty()
            ? QStringLiteral("Unsupported ZeroSlack Stimulus Scenario schema version")
            : error;
        return result;
    }
    scenario.schemaVersion = static_cast<int>(schemaVersion);
    scenario.manifestSchemaVersion = static_cast<int>(manifestSchemaVersion);
    if (!readString(root, QStringLiteral("identity"), QStringLiteral("stimulus"),
                    scenario.identity, error)
        || !readString(root, QStringLiteral("manifestIdentity"),
                       QStringLiteral("stimulus"), scenario.manifestIdentity, error)
        || !readString(root, QStringLiteral("workspaceId"), QStringLiteral("stimulus"),
                       scenario.workspaceId, error)) {
        result.error = error;
        return result;
    }

    if (!root.value(QStringLiteral("target")).isObject()) {
        result.error = QStringLiteral("stimulus.target must be an object");
        return result;
    }
    const auto target = root.value(QStringLiteral("target")).toObject();
    if (!exactKeys(target,
                   {QStringLiteral("mode"), QStringLiteral("module"),
                    QStringLiteral("instancePath")},
                   QStringLiteral("stimulus.target"), error)) {
        result.error = error;
        return result;
    }
    std::string modeText;
    if (!readString(target, QStringLiteral("mode"), QStringLiteral("stimulus.target"),
                    modeText, error)
        || !readString(target, QStringLiteral("module"),
                       QStringLiteral("stimulus.target"), scenario.target.module, error)
        || !readString(target, QStringLiteral("instancePath"),
                       QStringLiteral("stimulus.target"), scenario.target.instancePath,
                       error, true)) {
        result.error = error;
        return result;
    }
    const auto mode = targetModeFromString(modeText);
    if (!mode || (*mode == ModuleManifestTargetMode::Instance
                  && scenario.target.instancePath.empty())
        || (*mode == ModuleManifestTargetMode::ModuleDefinition
            && !scenario.target.instancePath.empty())) {
        result.error = QStringLiteral("stimulus.target mode and instancePath are inconsistent");
        return result;
    }
    scenario.target.mode = *mode;

    if (!root.value(QStringLiteral("scenario")).isObject()) {
        result.error = QStringLiteral("stimulus.scenario must be an object");
        return result;
    }
    const auto scenarioObject = root.value(QStringLiteral("scenario")).toObject();
    const auto scenarioContext = QStringLiteral("stimulus.scenario");
    const auto scenarioKeys = scenario.schemaVersion >= 2
        ? std::initializer_list<QString>{
              QStringLiteral("id"), QStringLiteral("name"),
              QStringLiteral("timeBasePicosecondsPerTick"),
              QStringLiteral("durationTicks"), QStringLiteral("groups"),
              QStringLiteral("ports"), QStringLiteral("markers"),
              QStringLiteral("view")}
        : std::initializer_list<QString>{
              QStringLiteral("id"), QStringLiteral("name"),
              QStringLiteral("timeBasePicosecondsPerTick"),
              QStringLiteral("durationTicks"), QStringLiteral("groups"),
              QStringLiteral("ports")};
    if (!exactKeys(
            scenarioObject,
            scenarioKeys,
            scenarioContext,
            error)
        || !readString(scenarioObject, QStringLiteral("id"), scenarioContext,
                       scenario.scenarioId, error)
        || !readString(scenarioObject, QStringLiteral("name"), scenarioContext,
                       scenario.name, error)
        || !readSignedDecimalString(
            scenarioObject, QStringLiteral("timeBasePicosecondsPerTick"),
            scenarioContext, scenario.timeBase.picosecondsPerTick, error, true)
        || !readSignedDecimalString(
            scenarioObject, QStringLiteral("durationTicks"), scenarioContext,
            scenario.duration, error, true)) {
        result.error = error;
        return result;
    }
    if (!scenarioObject.value(QStringLiteral("groups")).isArray()
        || !scenarioObject.value(QStringLiteral("ports")).isArray()) {
        result.error = QStringLiteral("stimulus.scenario groups and ports must be arrays");
        return result;
    }

    std::set<std::string> groupIds;
    std::set<std::size_t> displayOrders;
    const auto groups = scenarioObject.value(QStringLiteral("groups")).toArray();
    for (qsizetype index = 0; index < groups.size(); ++index) {
        if (!groups.at(index).isObject()) {
            result.error = QStringLiteral("stimulus.scenario.groups[%1] must be an object")
                               .arg(index);
            return result;
        }
        const auto object = groups.at(index).toObject();
        const auto context = QStringLiteral("stimulus.scenario.groups[%1]").arg(index);
        StimulusScenarioGroup group;
        std::uint64_t displayOrder = 0;
        if (!exactKeys(object,
                       {QStringLiteral("id"), QStringLiteral("name"),
                        QStringLiteral("displayOrder"), QStringLiteral("visible")},
                       context, error)
            || !readString(object, QStringLiteral("id"), context, group.id, error)
            || !readString(object, QStringLiteral("name"), context, group.name, error)
            || !readUnsigned(object, QStringLiteral("displayOrder"), context,
                             displayOrder, error)
            || !readBool(object, QStringLiteral("visible"), context,
                         group.visible, error)) {
            result.error = error;
            return result;
        }
        group.displayOrder = static_cast<std::size_t>(displayOrder);
        if (!groupIds.insert(group.id).second
            || !displayOrders.insert(group.displayOrder).second) {
            result.error = QStringLiteral("%1 has a duplicate id or displayOrder")
                               .arg(context);
            return result;
        }
        scenario.groups.push_back(std::move(group));
    }

    std::set<std::string> portNames;
    std::set<std::string> laneIds;
    const auto ports = scenarioObject.value(QStringLiteral("ports")).toArray();
    for (qsizetype index = 0; index < ports.size(); ++index) {
        if (!ports.at(index).isObject()) {
            result.error = QStringLiteral("stimulus.scenario.ports[%1] must be an object")
                               .arg(index);
            return result;
        }
        const auto object = ports.at(index).toObject();
        const auto context = QStringLiteral("stimulus.scenario.ports[%1]").arg(index);
        StimulusScenarioPort port;
        std::uint64_t displayOrder = 0;
        std::string roleText;
        std::string kindText;
        std::string radixText;
        if (!exactKeys(
                object,
                {QStringLiteral("laneId"), QStringLiteral("displayOrder"),
                 QStringLiteral("role"), QStringLiteral("kind"),
                 QStringLiteral("radix"), QStringLiteral("visible"),
                 QStringLiteral("groupId"), QStringLiteral("binding"),
                 QStringLiteral("enumMap"), QStringLiteral("segments"),
                 QStringLiteral("clock"), QStringLiteral("reset")},
                context,
                error)
            || !readString(object, QStringLiteral("laneId"), context, port.laneId, error)
            || !readUnsigned(object, QStringLiteral("displayOrder"), context,
                             displayOrder, error)
            || !readString(object, QStringLiteral("role"), context, roleText, error)
            || !readString(object, QStringLiteral("kind"), context, kindText, error)
            || !readString(object, QStringLiteral("radix"), context, radixText, error)
            || !readBool(object, QStringLiteral("visible"), context, port.visible, error)
            || !readString(object, QStringLiteral("groupId"), context,
                           port.groupId, error, true)
            || !object.value(QStringLiteral("binding")).isObject()
            || !parseBinding(object.value(QStringLiteral("binding")).toObject(),
                            context + QStringLiteral(".binding"), port.binding, error)) {
            result.error = error.isEmpty()
                ? QStringLiteral("%1.binding must be an object").arg(context) : error;
            return result;
        }
        port.displayOrder = static_cast<std::size_t>(displayOrder);
        const auto role = roleFromString(roleText);
        const auto kind = laneKindFromString(kindText);
        const auto radix = radixFromString(radixText);
        if (!role || !kind || !radix
            || (*kind != LaneKind::Clock && *kind != LaneKind::Bit
                && *kind != LaneKind::Bus && *kind != LaneKind::Enum)
            || !roleMatchesDirection(*role, port.binding.direction)) {
            result.error = QStringLiteral("%1 role, kind, radix, or direction is inconsistent")
                               .arg(context);
            return result;
        }
        port.role = *role;
        port.kind = *kind;
        port.radix = *radix;
        if ((port.kind == LaneKind::Clock || port.kind == LaneKind::Bit)
            && port.binding.width != 1) {
            result.error = QStringLiteral("%1 one-bit kind has a non-one-bit binding")
                               .arg(context);
            return result;
        }
        if (!displayOrders.insert(port.displayOrder).second
            || !laneIds.insert(port.laneId).second
            || !portNames.insert(port.binding.name).second
            || groupIds.contains(port.laneId)
            || (!port.groupId.empty() && !groupIds.contains(port.groupId))) {
            result.error = QStringLiteral("%1 has duplicate identity/order or unknown group")
                               .arg(context);
            return result;
        }

        const auto enumValue = object.value(QStringLiteral("enumMap"));
        if (!enumValue.isObject()) {
            result.error = QStringLiteral("%1.enumMap must be an object").arg(context);
            return result;
        }
        const auto enumObject = enumValue.toObject();
        for (auto iterator = enumObject.begin(); iterator != enumObject.end(); ++iterator) {
            if (iterator.key().isEmpty() || !iterator.value().isString()) {
                result.error = QStringLiteral("%1.enumMap entries must be string pairs")
                                   .arg(context);
                return result;
            }
            port.enumMap.emplace(
                stdString(iterator.key()), stdString(iterator.value().toString()));
        }
        if ((port.kind == LaneKind::Enum) != !port.enumMap.empty()) {
            result.error = QStringLiteral("%1 enum kind and enumMap are inconsistent")
                               .arg(context);
            return result;
        }

        const auto segments = object.value(QStringLiteral("segments"));
        if (!segments.isArray()) {
            result.error = QStringLiteral("%1.segments must be an array").arg(context);
            return result;
        }
        for (qsizetype rangeIndex = 0; rangeIndex < segments.toArray().size(); ++rangeIndex) {
            const auto value = segments.toArray().at(rangeIndex);
            if (!value.isObject()) {
                result.error = QStringLiteral("%1.segments[%2] must be an object")
                                   .arg(context).arg(rangeIndex);
                return result;
            }
            StimulusRange range;
            if (!parseRange(value.toObject(),
                            context + QStringLiteral(".segments[%1]").arg(rangeIndex),
                            range, error)) {
                result.error = error;
                return result;
            }
            port.segments.push_back(std::move(range));
        }

        const auto clockValue = object.value(QStringLiteral("clock"));
        if (!clockValue.isNull()) {
            if (!clockValue.isObject()) {
                result.error = QStringLiteral("%1.clock must be an object or null").arg(context);
                return result;
            }
            const auto clockObject = clockValue.toObject();
            const auto clockContext = context + QStringLiteral(".clock");
            StimulusClockConfiguration clock;
            std::string edgeText;
            std::string initialText;
            if (!exactKeys(
                    clockObject,
                    {QStringLiteral("periodTicks"), QStringLiteral("phaseTicks"),
                     QStringLiteral("dutyNumerator"), QStringLiteral("dutyDenominator"),
                     QStringLiteral("activeEdge"), QStringLiteral("initialValue")},
                    clockContext,
                    error)
                || !readSignedDecimalString(clockObject, QStringLiteral("periodTicks"),
                                           clockContext, clock.period, error, true)
                || !readSignedDecimalString(clockObject, QStringLiteral("phaseTicks"),
                                           clockContext, clock.phase, error, false)
                || !readSignedDecimalString(clockObject, QStringLiteral("dutyNumerator"),
                                           clockContext, clock.dutyCycle.numerator, error, true)
                || !readSignedDecimalString(clockObject, QStringLiteral("dutyDenominator"),
                                           clockContext, clock.dutyCycle.denominator, error, true)
                || !readString(clockObject, QStringLiteral("activeEdge"), clockContext,
                              edgeText, error)
                || !readString(clockObject, QStringLiteral("initialValue"), clockContext,
                              initialText, error)) {
                result.error = error;
                return result;
            }
            const auto edge = clockEdgeFromString(edgeText);
            if (!edge || !clock.dutyCycle.isValid()
                || clock.dutyCycle.numerator <= 0
                || clock.dutyCycle.numerator >= clock.dutyCycle.denominator
                || (initialText != "0" && initialText != "1")) {
                result.error = QStringLiteral("%1 contains an invalid clock configuration")
                                   .arg(clockContext);
                return result;
            }
            clock.activeEdge = *edge;
            clock.initialValue = initialText.front();
            ClockDomain validationClock;
            validationClock.id = "contract-clock";
            validationClock.period = clock.period;
            validationClock.phase = clock.phase;
            validationClock.dutyCycle = clock.dutyCycle;
            validationClock.activeEdge = clock.activeEdge;
            Lane validationLane;
            validationLane.kind = LaneKind::Clock;
            if (!validationClock.isValid()
                || clockValueAt(validationClock, validationLane, 0)
                    != clock.initialValue) {
                result.error = QStringLiteral("%1.initialValue conflicts with the clock timing")
                                   .arg(clockContext);
                return result;
            }
            port.clock = clock;
        }
        if ((port.kind == LaneKind::Clock) != port.clock.has_value()) {
            result.error = QStringLiteral("%1 clock kind and configuration are inconsistent")
                               .arg(context);
            return result;
        }

        const auto resetValue = object.value(QStringLiteral("reset"));
        if (!resetValue.isNull()) {
            if (!resetValue.isObject()) {
                result.error = QStringLiteral("%1.reset must be an object or null").arg(context);
                return result;
            }
            const auto resetObject = resetValue.toObject();
            const auto resetContext = context + QStringLiteral(".reset");
            std::string activeText;
            std::string synchronizationText;
            if (!exactKeys(resetObject,
                           {QStringLiteral("activeLevel"),
                            QStringLiteral("synchronization")},
                           resetContext, error)
                || !readString(resetObject, QStringLiteral("activeLevel"), resetContext,
                              activeText, error)
                || !readString(resetObject, QStringLiteral("synchronization"), resetContext,
                              synchronizationText, error)) {
                result.error = error;
                return result;
            }
            const auto active = resetActiveLevelFromString(activeText);
            const auto synchronization =
                resetSynchronizationFromString(synchronizationText);
            if (!active || !synchronization || port.binding.width != 1
                || !hasStimulus(port.role)) {
                result.error = QStringLiteral("%1 contains invalid reset metadata")
                                   .arg(resetContext);
                return result;
            }
            port.reset = StimulusResetConfiguration{*active, *synchronization};
        }
        if (!validatePortRanges(port, scenario.duration, context, error)) {
            result.error = error;
            return result;
        }
        scenario.ports.push_back(std::move(port));
    }
    if (scenario.ports.empty()) {
        result.error = QStringLiteral("stimulus.scenario.ports cannot be empty");
        return result;
    }
    if (scenario.schemaVersion >= 2) {
        const auto markersValue = scenarioObject.value(QStringLiteral("markers"));
        const auto viewValue = scenarioObject.value(QStringLiteral("view"));
        if (!markersValue.isArray() || !viewValue.isObject()) {
            result.error = QStringLiteral(
                "stimulus.scenario markers and view must be an array and object");
            return result;
        }
        std::set<std::string> markerIds;
        const auto markers = markersValue.toArray();
        for (qsizetype index = 0; index < markers.size(); ++index) {
            if (!markers.at(index).isObject()) {
                result.error = QStringLiteral(
                    "stimulus.scenario.markers[%1] must be an object").arg(index);
                return result;
            }
            const auto object = markers.at(index).toObject();
            const auto context = QStringLiteral("stimulus.scenario.markers[%1]")
                                     .arg(index);
            StimulusScenarioMarker marker;
            std::string kindText;
            if (!exactKeys(
                    object,
                    {QStringLiteral("id"), QStringLiteral("name"),
                     QStringLiteral("startTick"), QStringLiteral("endTick"),
                     QStringLiteral("kind"), QStringLiteral("note")},
                    context,
                    error)
                || !readString(object, QStringLiteral("id"), context,
                               marker.id, error)
                || !readString(object, QStringLiteral("name"), context,
                               marker.name, error)
                || !readSignedDecimalString(
                    object, QStringLiteral("startTick"), context,
                    marker.start, error, false)
                || !readSignedDecimalString(
                    object, QStringLiteral("endTick"), context,
                    marker.end, error, false)
                || !readString(object, QStringLiteral("kind"), context,
                               kindText, error)
                || !readString(object, QStringLiteral("note"), context,
                               marker.note, error, true)) {
                result.error = error;
                return result;
            }
            const auto kind = markerKindFromString(kindText);
            if (!kind || marker.start < 0 || marker.end < marker.start
                || marker.end > scenario.duration
                || (*kind == MarkerKind::Point && marker.start != marker.end)
                || !markerIds.insert(marker.id).second) {
                result.error = QStringLiteral(
                    "%1 has invalid or duplicate marker geometry").arg(context);
                return result;
            }
            marker.kind = *kind;
            scenario.markers.push_back(std::move(marker));
        }

        const auto view = viewValue.toObject();
        const auto viewContext = QStringLiteral("stimulus.scenario.view");
        if (!exactKeys(
                view,
                {QStringLiteral("selectedPortName"),
                 QStringLiteral("cursorTick"),
                 QStringLiteral("visibleSpanTicks")},
                viewContext,
                error)
            || !readString(view, QStringLiteral("selectedPortName"),
                           viewContext, scenario.view.selectedPortName,
                           error, true)
            || !readSignedDecimalString(
                view, QStringLiteral("cursorTick"), viewContext,
                scenario.view.cursorTick, error, false)
            || !readSignedDecimalString(
                view, QStringLiteral("visibleSpanTicks"), viewContext,
                scenario.view.visibleSpanTicks, error, false)) {
            result.error = error;
            return result;
        }
        if (scenario.view.cursorTick < 0
            || scenario.view.cursorTick > scenario.duration
            || scenario.view.visibleSpanTicks < 0
            || scenario.view.visibleSpanTicks > scenario.duration
            || (!scenario.view.selectedPortName.empty()
                && !portNames.contains(scenario.view.selectedPortName))) {
            result.error = QStringLiteral(
                "stimulus.scenario.view contains an invalid location");
            return result;
        }
    }
    if (scenario.identity != computeIdentity(scenario)) {
        result.error = QStringLiteral("ZeroSlack Stimulus Scenario identity does not match its content");
        return result;
    }
    result.scenario = std::move(scenario);
    return result;
}

StimulusScenarioExportResult exportZeroSlackStimulusScenario(
    const Project& project,
    const Scenario& sourceScenario,
    const StimulusScenarioViewState& view)
{
    StimulusScenarioExportResult result;
    const auto manifestIdentity = extensionString(
        project.extensions, "waveSimulation.moduleManifestIdentity");
    const auto workspaceId = extensionString(
        project.extensions, "waveSimulation.workspaceId");
    const auto targetMode = extensionString(
        project.extensions, "waveSimulation.targetMode");
    const auto targetModule = extensionString(
        project.extensions, "waveSimulation.targetModule");
    const auto targetInstancePath = extensionString(
        project.extensions, "waveSimulation.targetInstancePath");
    const auto manifestSchemaVersion = extensionUnsigned(
        project.extensions, "waveSimulation.moduleManifestSchemaVersion");
    if (!manifestIdentity || !workspaceId || !targetMode || !targetModule
        || !targetInstancePath || sourceScenario.duration <= 0
        || !project.timeBase.isValid()) {
        result.error = QStringLiteral(
            "Project is not a valid ZeroSlack Module Manifest import or has an invalid scenario");
        return result;
    }
    const auto mode = targetModeFromString(*targetMode);
    if (!mode) {
        result.error = QStringLiteral("Project has an invalid ZeroSlack target mode");
        return result;
    }

    ZeroSlackStimulusScenario scenario;
    scenario.manifestSchemaVersion = static_cast<int>(
        manifestSchemaVersion.value_or(ZeroSlackModuleManifest::CurrentSchemaVersion));
    scenario.manifestIdentity = *manifestIdentity;
    scenario.workspaceId = *workspaceId;
    scenario.target = {*mode, *targetModule, *targetInstancePath};
    scenario.scenarioId = sourceScenario.id;
    scenario.name = sourceScenario.name;
    scenario.timeBase = project.timeBase;
    scenario.duration = sourceScenario.duration;
    scenario.view = view;

    for (std::size_t index = 0; index < sourceScenario.lanes.size(); ++index) {
        const auto& lane = sourceScenario.lanes[index];
        if (lane.kind == LaneKind::Group) {
            scenario.groups.push_back({lane.id, lane.name, index, lane.visible});
            continue;
        }
        const auto laneManifestIdentity = extensionString(
            lane.extensions, "waveSimulation.moduleManifestIdentity");
        if (!laneManifestIdentity || *laneManifestIdentity != *manifestIdentity) {
            result.diagnostics.append(
                QStringLiteral("Lane %1 is not bound to this Module Manifest and was omitted.")
                    .arg(qString(lane.name)));
            continue;
        }
        const auto binding = bindingFromLane(lane, index, result.diagnostics);
        const auto roleText = extensionString(lane.extensions, "waveSimulation.role");
        const auto role = roleText ? roleFromString(*roleText) : std::nullopt;
        if (!binding || !role || !roleMatchesDirection(*role, binding->direction)) {
            result.error = QStringLiteral("Port lane %1 has incomplete binding metadata")
                               .arg(qString(lane.name));
            return result;
        }
        StimulusScenarioPort port;
        port.binding = *binding;
        port.laneId = lane.id;
        port.displayOrder = index;
        port.role = *role;
        port.kind = lane.kind;
        port.radix = lane.radix;
        port.visible = lane.visible;
        port.groupId = lane.groupId;
        port.enumMap = lane.enumMap;
        for (const auto& segment : lane.segments) {
            port.segments.push_back({segment.start, segment.end, segment.value});
        }
        if (lane.kind == LaneKind::Clock) {
            const auto* clock = findClock(project, lane.clockDomainId);
            if (!clock || !clock->isValid()) {
                result.error = QStringLiteral("Clock lane %1 has no valid clock domain")
                                   .arg(qString(lane.name));
                return result;
            }
            const auto initial = clockValueAt(*clock, lane, 0);
            if (initial != '0' && initial != '1') {
                result.error = QStringLiteral("Clock lane %1 has no deterministic initial value")
                                   .arg(qString(lane.name));
                return result;
            }
            port.clock = StimulusClockConfiguration{
                clock->period,
                clock->phase,
                clock->dutyCycle,
                clock->activeEdge,
                initial,
            };
        }
        if (extensionBool(lane.extensions, "waveSimulation.resetCandidate").value_or(false)) {
            auto active = StimulusResetActiveLevel::Unspecified;
            auto synchronization = StimulusResetSynchronization::Unspecified;
            if (const auto value = extensionString(
                    lane.extensions, "waveSimulation.resetActiveLevel")) {
                active = resetActiveLevelFromString(*value).value_or(active);
            }
            if (const auto value = extensionString(
                    lane.extensions, "waveSimulation.resetSynchronization")) {
                synchronization =
                    resetSynchronizationFromString(*value).value_or(synchronization);
            }
            port.reset = StimulusResetConfiguration{active, synchronization};
        }
        scenario.ports.push_back(std::move(port));
    }
    if (scenario.ports.empty()) {
        result.error = QStringLiteral("Scenario has no Module Manifest-bound port lanes");
        return result;
    }
    for (const auto& sourceMarker : sourceScenario.markers) {
        scenario.markers.push_back({
            sourceMarker.id,
            sourceMarker.name,
            sourceMarker.start,
            sourceMarker.end,
            sourceMarker.kind,
            sourceMarker.note,
        });
    }
    scenario.view.cursorTick = std::clamp<Tick>(
        scenario.view.cursorTick, 0, scenario.duration);
    scenario.view.visibleSpanTicks = std::clamp<Tick>(
        scenario.view.visibleSpanTicks, 0, scenario.duration);
    if (!scenario.view.selectedPortName.empty()
        && std::none_of(
            scenario.ports.begin(), scenario.ports.end(),
            [&scenario](const StimulusScenarioPort& port) {
                return port.binding.name == scenario.view.selectedPortName;
            })) {
        scenario.view.selectedPortName.clear();
    }
    scenario.identity = computeIdentity(scenario);
    const auto verified = parseZeroSlackStimulusScenario(
        serializeZeroSlackStimulusScenario(scenario));
    if (!verified.ok()) {
        result.error = QStringLiteral("Cannot export Stimulus Scenario: %1")
                           .arg(verified.error);
        return result;
    }
    result.scenario = *verified.scenario;
    return result;
}

StimulusScenarioRestoreResult restoreZeroSlackStimulusScenario(
    const ZeroSlackModuleManifest& manifest,
    const ZeroSlackStimulusScenario& saved)
{
    StimulusScenarioRestoreResult result;
    if (saved.schemaVersion < ZeroSlackStimulusScenario::MinimumSupportedSchemaVersion
        || saved.schemaVersion > ZeroSlackStimulusScenario::CurrentSchemaVersion
        || saved.manifestSchemaVersion != ZeroSlackModuleManifest::CurrentSchemaVersion
        || saved.identity != computeIdentity(saved)
        || saved.duration <= 0 || !saved.timeBase.isValid()
        || manifest.schemaVersion != saved.manifestSchemaVersion
        || manifest.workspaceId != saved.workspaceId
        || manifest.target.mode != saved.target.mode
        || manifest.target.module != saved.target.module
        || manifest.target.instancePath != saved.target.instancePath) {
        result.error = QStringLiteral("Stimulus Scenario workspace or target does not match the Module Manifest");
        return result;
    }
    result.manifestChanged = manifest.identity != saved.manifestIdentity;
    ModuleManifestImportOptions options;
    options.duration = saved.duration;
    options.defaultClockPeriod = std::min<Tick>(10'000, saved.duration);
    const auto imported = importZeroSlackModuleManifest(manifest, options);
    if (!imported.ok()) {
        result.error = imported.error;
        return result;
    }
    result.diagnostics = imported.diagnostics;
    auto project = *imported.project;
    project.timeBase = saved.timeBase;
    if (project.scenarios.empty()) {
        result.error = QStringLiteral("Module Manifest importer returned no scenario");
        return result;
    }
    auto baseScenario = project.scenarios.front();

    std::map<std::string, Lane> currentPorts;
    std::map<std::string, ClockDomain> currentClocks;
    for (const auto& clock : project.clockDomains) currentClocks.emplace(clock.id, clock);
    for (const auto& lane : baseScenario.lanes) {
        if (lane.kind != LaneKind::Group) currentPorts.emplace(lane.name, lane);
    }
    std::map<std::string, std::string> restoredNameBySavedName;
    std::set<std::string> claimedCurrentNames;
    std::set<std::string> widthChangedSavedNames;
    std::set<std::string> incompatibleSavedNames;

    // Exact names are authoritative. Missing names are migrated only when the
    // current manifest provides one unambiguous structural match.
    for (const auto& port : saved.ports) {
        const auto current = currentPorts.find(port.binding.name);
        if (current == currentPorts.end()) continue;
        restoredNameBySavedName.emplace(port.binding.name, current->first);
        claimedCurrentNames.insert(current->first);
        if (compatibleBinding(port, current->second)) continue;
        ++result.incompatiblePortCount;
        incompatibleSavedNames.insert(port.binding.name);
        if (sameBindingExceptWidth(port, current->second)) {
            ++result.widthChangedPortCount;
            widthChangedSavedNames.insert(port.binding.name);
        }
    }

    for (const auto& port : saved.ports) {
        if (restoredNameBySavedName.contains(port.binding.name)) continue;
        std::vector<std::string> compatibleCandidates;
        std::vector<std::string> widthChangedCandidates;
        for (const auto& [name, lane] : currentPorts) {
            if (claimedCurrentNames.contains(name)) continue;
            if (compatibleBinding(port, lane)) {
                compatibleCandidates.push_back(name);
            } else if (sameBindingExceptWidth(port, lane)) {
                widthChangedCandidates.push_back(name);
            }
        }
        const auto selectUniqueCandidate = [&](const std::vector<std::string>& candidates)
            -> std::optional<std::string> {
            if (candidates.size() == 1) return candidates.front();
            std::vector<std::string> sameOrder;
            for (const auto& candidate : candidates) {
                QStringList ignored;
                const auto binding = bindingFromLane(currentPorts.at(candidate), 0, ignored);
                if (binding && binding->sourceOrder == port.binding.sourceOrder) {
                    sameOrder.push_back(candidate);
                }
            }
            return sameOrder.size() == 1
                ? std::optional<std::string>{sameOrder.front()} : std::nullopt;
        };
        auto candidate = selectUniqueCandidate(compatibleCandidates);
        bool widthChanged = false;
        if (!candidate) {
            candidate = selectUniqueCandidate(widthChangedCandidates);
            widthChanged = candidate.has_value();
        }
        if (!candidate) {
            ++result.missingSavedPortCount;
            continue;
        }
        restoredNameBySavedName.emplace(port.binding.name, *candidate);
        claimedCurrentNames.insert(*candidate);
        ++result.renamedPortCount;
        if (widthChanged) {
            ++result.widthChangedPortCount;
            ++result.incompatiblePortCount;
            widthChangedSavedNames.insert(port.binding.name);
            incompatibleSavedNames.insert(port.binding.name);
        }
        result.diagnostics.append(
            QStringLiteral("Saved port %1 was migrated to renamed port %2.")
                .arg(qString(port.binding.name), qString(*candidate)));
    }
    for (const auto& [name, lane] : currentPorts) {
        static_cast<void>(lane);
        if (!claimedCurrentNames.contains(name)) ++result.newPortCount;
    }
    if (!result.manifestChanged
        && (result.missingSavedPortCount != 0 || result.incompatiblePortCount != 0
            || result.newPortCount != 0
            || currentPorts.size() != saved.ports.size())) {
        result.error = QStringLiteral(
            "Unchanged Module Manifest identity produced a different port contract");
        return result;
    }

    struct OrderedLane {
        std::size_t order{0};
        std::size_t tie{0};
        Lane lane;
    };
    std::vector<OrderedLane> ordered;
    ordered.reserve(saved.groups.size() + currentPorts.size());
    std::set<std::string> validGroups;
    std::size_t tie = 0;
    std::size_t nextOrder = 0;
    for (const auto& group : saved.groups) {
        validGroups.insert(group.id);
        ordered.push_back({group.displayOrder, tie++, makeGroupLane(group)});
        nextOrder = std::max(nextOrder, group.displayOrder + 1);
    }

    std::set<std::string> restoredNames;
    std::vector<ClockDomain> restoredClocks;
    for (const auto& port : saved.ports) {
        nextOrder = std::max(nextOrder, port.displayOrder + 1);
        const auto mappedName = restoredNameBySavedName.find(port.binding.name);
        if (mappedName == restoredNameBySavedName.end()) {
            result.diagnostics.append(
                QStringLiteral("Saved port %1 is missing from the current manifest and was not restored.")
                    .arg(qString(port.binding.name)));
            continue;
        }
        const auto current = currentPorts.find(mappedName->second);
        if (incompatibleSavedNames.contains(port.binding.name)
            && !widthChangedSavedNames.contains(port.binding.name)) {
            result.diagnostics.append(
                QStringLiteral("Saved port %1 changed direction or type; current defaults were retained.")
                    .arg(qString(port.binding.name)));
            continue;
        }
        auto lane = current->second;
        lane.radix = port.radix;
        lane.visible = port.visible;
        lane.groupId = validGroups.contains(port.groupId) ? port.groupId : std::string{};
        if (widthChangedSavedNames.contains(port.binding.name)) {
            restoredNames.insert(current->first);
            if (!lane.clockDomainId.empty()) {
                const auto clock = currentClocks.find(lane.clockDomainId);
                if (clock != currentClocks.end()) restoredClocks.push_back(clock->second);
            }
            ordered.push_back({port.displayOrder, tie++, std::move(lane)});
            result.diagnostics.append(
                QStringLiteral("Saved port %1 changed width; display settings were restored and stimulus reset to safe defaults.")
                    .arg(qString(port.binding.name)));
            continue;
        }
        lane.kind = port.kind;
        lane.enumMap = port.enumMap;
        lane.segments.clear();
        lane.clockDomainId.clear();
        lane.extensions["waveSimulation.stimulusScenarioIdentity"] =
            jsonStringValue(saved.identity);
        if (port.reset) {
            lane.extensions["waveSimulation.resetCandidate"] = "true";
            lane.extensions["waveSimulation.resetActiveLevel"] =
                jsonStringValue(std::string(toString(port.reset->activeLevel)));
            lane.extensions["waveSimulation.resetSynchronization"] =
                jsonStringValue(std::string(toString(port.reset->synchronization)));
        }
        try {
            for (std::size_t rangeIndex = 0; rangeIndex < port.segments.size(); ++rangeIndex) {
                const auto& range = port.segments[rangeIndex];
                setSegmentRange(
                    lane,
                    range.start,
                    range.end,
                    range.value,
                    stableDigestId(
                        "zs-stimulus-segment",
                        saved.identity,
                        port.binding.name + ":" + std::to_string(rangeIndex)));
            }
        } catch (const std::exception& exception) {
            result.error = QStringLiteral("Cannot restore port %1: %2")
                               .arg(qString(port.binding.name),
                                    QString::fromUtf8(exception.what()));
            return result;
        }
        if (port.clock) {
            ClockDomain clock;
            clock.id = stableDigestId(
                "zs-clock", saved.scenarioId, current->first);
            clock.name = current->first;
            clock.period = port.clock->period;
            clock.phase = port.clock->phase;
            clock.dutyCycle = port.clock->dutyCycle;
            clock.activeEdge = port.clock->activeEdge;
            clock.extensions = lane.extensions;
            clock.extensions["waveSimulation.clockInitialValue"] =
                jsonStringValue(std::string(1, port.clock->initialValue));
            lane.clockDomainId = clock.id;
            restoredClocks.push_back(std::move(clock));
        }
        restoredNames.insert(current->first);
        ordered.push_back({port.displayOrder, tie++, std::move(lane)});
        ++result.restoredPortCount;
    }

    struct CurrentDefault {
        std::size_t sourceOrder{0};
        std::string name;
    };
    std::vector<CurrentDefault> defaults;
    for (const auto& [name, lane] : currentPorts) {
        if (!restoredNames.contains(name)) {
            defaults.push_back({
                static_cast<std::size_t>(extensionUnsigned(
                    lane.extensions, "waveSimulation.sourceOrder").value_or(
                        std::numeric_limits<std::uint64_t>::max())),
                name});
        }
    }
    std::stable_sort(defaults.begin(), defaults.end(), [](const CurrentDefault& left,
                                                          const CurrentDefault& right) {
        return left.sourceOrder < right.sourceOrder;
    });
    for (const auto& item : defaults) {
        auto lane = std::move(currentPorts.at(item.name));
        if (!lane.clockDomainId.empty()) {
            const auto clock = currentClocks.find(lane.clockDomainId);
            if (clock != currentClocks.end()) restoredClocks.push_back(clock->second);
        }
        ordered.push_back({nextOrder++, tie++, std::move(lane)});
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const OrderedLane& left,
                                                        const OrderedLane& right) {
        return left.order < right.order
            || (left.order == right.order && left.tie < right.tie);
    });

    Scenario restored;
    restored.id = saved.scenarioId;
    restored.name = saved.name;
    restored.duration = saved.duration;
    restored.markers.reserve(saved.markers.size());
    for (const auto& marker : saved.markers) {
        Marker restoredMarker;
        restoredMarker.id = marker.id;
        restoredMarker.name = marker.name;
        restoredMarker.start = marker.start;
        restoredMarker.end = marker.end;
        restoredMarker.kind = marker.kind;
        restoredMarker.note = marker.note;
        restored.markers.push_back(std::move(restoredMarker));
    }
    restored.extensions = baseScenario.extensions;
    restored.extensions["waveSimulation.stimulusScenarioIdentity"] =
        jsonStringValue(saved.identity);
    restored.lanes.reserve(ordered.size());
    for (auto& item : ordered) restored.lanes.push_back(std::move(item.lane));
    project.clockDomains = std::move(restoredClocks);
    project.scenarios.clear();
    project.scenarios.push_back(std::move(restored));
    project.extensions["waveSimulation.stimulusScenarioIdentity"] =
        jsonStringValue(saved.identity);
    result.view = saved.view;
    if (!result.view.selectedPortName.empty()) {
        const auto renamed = restoredNameBySavedName.find(result.view.selectedPortName);
        if (renamed != restoredNameBySavedName.end()) {
            result.view.selectedPortName = renamed->second;
        } else {
            result.view.selectedPortName.clear();
        }
    }
    if (result.manifestChanged) {
        result.diagnostics.append(
            QStringLiteral("Module Manifest changed: %1 port(s) restored, %2 renamed, %3 width-changed, %4 missing, %5 incompatible, %6 new.")
                .arg(result.restoredPortCount)
                .arg(result.renamedPortCount)
                .arg(result.widthChangedPortCount)
                .arg(result.missingSavedPortCount)
                .arg(result.incompatiblePortCount)
                .arg(result.newPortCount));
    }
    result.project = std::move(project);
    return result;
}

std::string_view toString(const StimulusPortRole role) noexcept
{
    switch (role) {
    case StimulusPortRole::Stimulus: return "stimulus";
    case StimulusPortRole::Watch: return "watch";
    case StimulusPortRole::StimulusWatch: return "stimulus-watch";
    }
    return "stimulus";
}

std::string_view toString(const StimulusResetActiveLevel activeLevel) noexcept
{
    switch (activeLevel) {
    case StimulusResetActiveLevel::Unspecified: return "unspecified";
    case StimulusResetActiveLevel::Low: return "low";
    case StimulusResetActiveLevel::High: return "high";
    }
    return "unspecified";
}

std::string_view toString(
    const StimulusResetSynchronization synchronization) noexcept
{
    switch (synchronization) {
    case StimulusResetSynchronization::Unspecified: return "unspecified";
    case StimulusResetSynchronization::Synchronous: return "synchronous";
    case StimulusResetSynchronization::Asynchronous: return "asynchronous";
    }
    return "unspecified";
}

} // namespace wave
