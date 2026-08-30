#include "wave/automation.h"

#include "wave/commands.h"
#include "wave/time.h"
#include "wave/validation.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wave {
namespace {

QJsonValue integerValue(const std::int64_t value)
{
    return QString::number(value);
}

QJsonValue unsignedIntegerValue(const std::uint64_t value)
{
    return QString::number(value);
}

QJsonArray stringArray(const std::vector<std::string>& values)
{
    QJsonArray result;
    for (const auto& value : values) {
        result.append(QString::fromStdString(value));
    }
    return result;
}

QJsonObject extensionsObject(const JsonExtensions& extensions)
{
    QJsonObject result;
    for (const auto& [key, encoded] : extensions) {
        QJsonParseError parseError;
        const auto wrapped =
            QByteArrayLiteral("[") + QByteArray::fromStdString(encoded)
            + QByteArrayLiteral("]");
        const auto document = QJsonDocument::fromJson(wrapped, &parseError);
        const auto value =
            parseError.error == QJsonParseError::NoError
                && document.isArray()
                && document.array().size() == 1
            ? document.array().at(0)
            : QJsonValue{QString::fromStdString(encoded)};
        result.insert(QString::fromStdString(key), value);
    }
    return result;
}

QJsonObject clockObject(const ClockDomain& clock)
{
    return {
        {QStringLiteral("id"), QString::fromStdString(clock.id)},
        {QStringLiteral("name"), QString::fromStdString(clock.name)},
        {QStringLiteral("periodTick"), integerValue(clock.period)},
        {QStringLiteral("phaseTick"), integerValue(clock.phase)},
        {QStringLiteral("dutyNumerator"), integerValue(clock.dutyCycle.numerator)},
        {QStringLiteral("dutyDenominator"), integerValue(clock.dutyCycle.denominator)},
        {QStringLiteral("activeEdge"),
         QString::fromLatin1(toString(clock.activeEdge).data())},
        {QStringLiteral("resetRelation"),
         QString::fromStdString(clock.resetRelation)},
        {QStringLiteral("extensions"), extensionsObject(clock.extensions)},
    };
}

QJsonObject segmentObject(const Segment& segment)
{
    return {
        {QStringLiteral("id"), QString::fromStdString(segment.id)},
        {QStringLiteral("startTick"), integerValue(segment.start)},
        {QStringLiteral("endTick"), integerValue(segment.end)},
        {QStringLiteral("value"), QString::fromStdString(segment.value)},
        {QStringLiteral("extensions"), extensionsObject(segment.extensions)},
    };
}

QJsonObject enumMapObject(
    const std::map<std::string, std::string>& enumMap)
{
    QJsonObject result;
    for (const auto& [name, value] : enumMap) {
        result.insert(
            QString::fromStdString(name),
            QString::fromStdString(value));
    }
    return result;
}

QJsonObject laneObject(const Lane& lane)
{
    QJsonArray segments;
    for (const auto& segment : lane.segments) {
        segments.append(segmentObject(segment));
    }
    return {
        {QStringLiteral("id"), QString::fromStdString(lane.id)},
        {QStringLiteral("name"), QString::fromStdString(lane.name)},
        {QStringLiteral("kind"), QString::fromLatin1(toString(lane.kind).data())},
        {QStringLiteral("width"), static_cast<qint64>(lane.width)},
        {QStringLiteral("signed"), lane.isSigned},
        {QStringLiteral("radix"), QString::fromLatin1(toString(lane.radix).data())},
        {QStringLiteral("enumMap"), enumMapObject(lane.enumMap)},
        {QStringLiteral("clockDomainId"),
         QString::fromStdString(lane.clockDomainId)},
        {QStringLiteral("color"), QString::fromStdString(lane.color)},
        {QStringLiteral("height"), lane.height},
        {QStringLiteral("visible"), lane.visible},
        {QStringLiteral("groupId"), QString::fromStdString(lane.groupId)},
        {QStringLiteral("segmentCount"),
         static_cast<qint64>(lane.segments.size())},
        {QStringLiteral("segments"), segments},
        {QStringLiteral("extensions"), extensionsObject(lane.extensions)},
    };
}

QJsonObject eventObject(const Event& event)
{
    QJsonObject result{
        {QStringLiteral("id"), QString::fromStdString(event.id)},
        {QStringLiteral("laneId"), QString::fromStdString(event.laneId)},
        {QStringLiteral("timeTick"), integerValue(event.tick)},
        {QStringLiteral("action"), QString::fromLatin1(toString(event.action).data())},
        {QStringLiteral("value"), QString::fromStdString(event.value)},
        {QStringLiteral("expectedResult"),
         QString::fromStdString(event.expectedResult)},
        {QStringLiteral("clockDomainId"),
         QString::fromStdString(event.clockDomainId)},
        {QStringLiteral("description"),
         QString::fromStdString(event.description)},
        {QStringLiteral("linkedSegmentId"),
         QString::fromStdString(event.linkedSegmentId)},
        {QStringLiteral("waveformLinked"), event.waveformLinked},
        {QStringLiteral("extensions"), extensionsObject(event.extensions)},
    };
    result.insert(
        QStringLiteral("cycle"),
        event.cycle ? integerValue(*event.cycle) : QJsonValue{QJsonValue::Null});
    return result;
}

QJsonObject relationObject(const Relation& relation)
{
    return {
        {QStringLiteral("id"), QString::fromStdString(relation.id)},
        {QStringLiteral("sourceEventId"),
         QString::fromStdString(relation.sourceEventId)},
        {QStringLiteral("targetEventId"),
         QString::fromStdString(relation.targetEventId)},
        {QStringLiteral("minimumDelayTick"),
         integerValue(relation.minimumDelay)},
        {QStringLiteral("maximumDelayTick"),
         integerValue(relation.maximumDelay)},
        {QStringLiteral("clockDomainId"),
         QString::fromStdString(relation.clockDomainId)},
        {QStringLiteral("condition"), QString::fromStdString(relation.condition)},
        {QStringLiteral("severity"),
         QString::fromLatin1(toString(relation.severity).data())},
        {QStringLiteral("description"),
         QString::fromStdString(relation.description)},
        {QStringLiteral("extensions"), extensionsObject(relation.extensions)},
    };
}

QJsonObject markerObject(const Marker& marker)
{
    return {
        {QStringLiteral("id"), QString::fromStdString(marker.id)},
        {QStringLiteral("name"), QString::fromStdString(marker.name)},
        {QStringLiteral("startTick"), integerValue(marker.start)},
        {QStringLiteral("endTick"), integerValue(marker.end)},
        {QStringLiteral("kind"), QString::fromLatin1(toString(marker.kind).data())},
        {QStringLiteral("note"), QString::fromStdString(marker.note)},
        {QStringLiteral("extensions"), extensionsObject(marker.extensions)},
    };
}

QJsonObject scenarioObject(const Scenario& scenario, const TimeBase& timeBase)
{
    QJsonArray lanes;
    for (const auto& lane : scenario.lanes) lanes.append(laneObject(lane));
    QJsonArray events;
    for (const auto& event : scenario.events) events.append(eventObject(event));
    QJsonArray relations;
    for (const auto& relation : scenario.relations) {
        relations.append(relationObject(relation));
    }
    QJsonArray markers;
    for (const auto& marker : scenario.markers) markers.append(markerObject(marker));
    return {
        {QStringLiteral("id"), QString::fromStdString(scenario.id)},
        {QStringLiteral("name"), QString::fromStdString(scenario.name)},
        {QStringLiteral("durationTick"), integerValue(scenario.duration)},
        {QStringLiteral("duration"),
         QString::fromStdString(formatTick(scenario.duration, timeBase))},
        {QStringLiteral("laneCount"),
         static_cast<qint64>(scenario.lanes.size())},
        {QStringLiteral("eventCount"),
         static_cast<qint64>(scenario.events.size())},
        {QStringLiteral("relationCount"),
         static_cast<qint64>(scenario.relations.size())},
        {QStringLiteral("markerCount"),
         static_cast<qint64>(scenario.markers.size())},
        {QStringLiteral("lanes"), lanes},
        {QStringLiteral("events"), events},
        {QStringLiteral("relations"), relations},
        {QStringLiteral("markers"), markers},
        {QStringLiteral("extensions"), extensionsObject(scenario.extensions)},
    };
}

QJsonObject scenarioSummaryObject(
    const Scenario& scenario,
    const TimeBase& timeBase)
{
    auto result = scenarioObject(scenario, timeBase);
    auto lanes = result.value(QStringLiteral("lanes")).toArray();
    for (qsizetype index = 0; index < lanes.size(); ++index) {
        auto lane = lanes.at(index).toObject();
        lane.remove(QStringLiteral("segments"));
        lane.remove(QStringLiteral("extensions"));
        lanes.replace(index, lane);
    }
    result.insert(QStringLiteral("lanes"), lanes);
    result.remove(QStringLiteral("events"));
    result.remove(QStringLiteral("relations"));
    result.remove(QStringLiteral("markers"));
    result.remove(QStringLiteral("extensions"));
    return result;
}

const Segment* segmentAt(const Lane& lane, const Tick tick)
{
    const auto iterator = std::upper_bound(
        lane.segments.begin(),
        lane.segments.end(),
        tick,
        [](const Tick value, const Segment& segment) {
            return value < segment.start;
        });
    if (iterator == lane.segments.begin()) return nullptr;
    const auto& candidate = *std::prev(iterator);
    return candidate.start <= tick && tick < candidate.end
        ? &candidate
        : nullptr;
}

QJsonObject sampleObject(
    const Project& project,
    const Lane& lane,
    const Tick tick)
{
    const auto* segment = segmentAt(lane, tick);
    QString value;
    auto defined = segment != nullptr;
    if (lane.kind == LaneKind::Clock) {
        const auto* clock = findClock(project, lane.clockDomainId);
        if (clock && clock->isValid()) {
            value = QString(QChar::fromLatin1(clockValueAt(*clock, lane, tick)));
            defined = true;
        } else {
            value = QStringLiteral("X");
            defined = false;
        }
    } else if (segment) {
        value = QString::fromStdString(segment->value);
    } else if (lane.kind == LaneKind::Bit) {
        value = QStringLiteral("0");
    } else if (lane.kind == LaneKind::Bus || lane.kind == LaneKind::Enum) {
        value = QStringLiteral("X");
    } else {
        value = QStringLiteral("?");
    }

    QJsonObject result{
        {QStringLiteral("laneId"), QString::fromStdString(lane.id)},
        {QStringLiteral("name"), QString::fromStdString(lane.name)},
        {QStringLiteral("kind"), QString::fromLatin1(toString(lane.kind).data())},
        {QStringLiteral("width"), static_cast<qint64>(lane.width)},
        {QStringLiteral("value"), value},
        {QStringLiteral("defined"), defined},
        {QStringLiteral("implicit"), !segment && lane.kind != LaneKind::Clock},
    };
    if (segment) {
        result.insert(
            QStringLiteral("segmentId"),
            QString::fromStdString(segment->id));
        result.insert(
            QStringLiteral("segmentStartTick"),
            integerValue(segment->start));
        result.insert(
            QStringLiteral("segmentEndTick"),
            integerValue(segment->end));
    } else {
        result.insert(
            QStringLiteral("segmentId"),
            QJsonValue{QJsonValue::Null});
        result.insert(
            QStringLiteral("segmentStartTick"),
            QJsonValue{QJsonValue::Null});
        result.insert(
            QStringLiteral("segmentEndTick"),
            QJsonValue{QJsonValue::Null});
    }
    return result;
}

QString automationEdgeKindName(const AutomationEdgeKind kind)
{
    switch (kind) {
    case AutomationEdgeKind::Initial: return QStringLiteral("initial");
    case AutomationEdgeKind::Rising: return QStringLiteral("rising");
    case AutomationEdgeKind::Falling: return QStringLiteral("falling");
    case AutomationEdgeKind::Change: return QStringLiteral("change");
    }
    return QStringLiteral("change");
}

AutomationEdgeKind classifyAutomationEdge(
    const Lane& lane,
    const Tick tick,
    const QJsonValue& previousValue,
    const QJsonValue& value)
{
    if (tick == 0) return AutomationEdgeKind::Initial;
    if (lane.kind == LaneKind::Bit) {
        const auto previous = previousValue.toString();
        const auto current = value.toString();
        if (previous == QStringLiteral("0")
            && current == QStringLiteral("1")) {
            return AutomationEdgeKind::Rising;
        }
        if (previous == QStringLiteral("1")
            && current == QStringLiteral("0")) {
            return AutomationEdgeKind::Falling;
        }
    }
    return AutomationEdgeKind::Change;
}

struct RelationEndpointResolution {
    const Event* event{nullptr};
    std::size_t eventIdCount{0};
};

RelationEndpointResolution resolveRelationEndpoint(
    const Scenario& scenario,
    const std::string_view eventId)
{
    RelationEndpointResolution result;
    for (const auto& event : scenario.events) {
        if (event.id != eventId) continue;
        ++result.eventIdCount;
        if (result.eventIdCount == 1) result.event = &event;
    }
    if (result.eventIdCount != 1) result.event = nullptr;
    return result;
}

QJsonObject relationEndpointQueryObject(
    const Project& project,
    const Scenario& scenario,
    const RelationEndpointResolution& resolution,
    bool& ready)
{
    ready = false;
    QJsonObject result{
        {QStringLiteral("resolved"), false},
        {QStringLiteral("eventIdCount"),
         static_cast<qint64>(resolution.eventIdCount)},
        {QStringLiteral("relationEndpoint"), false},
    };
    if (!resolution.event) {
        result.insert(
            QStringLiteral("issue"),
            resolution.eventIdCount == 0
                ? QStringLiteral("missing-event")
                : QStringLiteral("ambiguous-event-id"));
        return result;
    }

    const auto& event = *resolution.event;
    result.insert(
        QStringLiteral("laneId"),
        QString::fromStdString(event.laneId));
    result.insert(QStringLiteral("timeTick"), integerValue(event.tick));
    result.insert(
        QStringLiteral("time"),
        QString::fromStdString(formatTick(event.tick, project.timeBase)));
    result.insert(
        QStringLiteral("action"),
        QString::fromLatin1(toString(event.action).data()));
    result.insert(
        QStringLiteral("description"),
        QString::fromStdString(event.description));
    result.insert(QStringLiteral("waveformLinked"), event.waveformLinked);

    const auto* lane = findLane(scenario, event.laneId);
    if (!lane) {
        result.insert(QStringLiteral("issue"), QStringLiteral("missing-lane"));
        return result;
    }
    result.insert(QStringLiteral("resolved"), true);
    result.insert(
        QStringLiteral("name"),
        QString::fromStdString(lane->name));
    result.insert(
        QStringLiteral("kind"),
        QString::fromLatin1(toString(lane->kind).data()));
    result.insert(
        QStringLiteral("width"),
        static_cast<qint64>(lane->width));
    result.insert(
        QStringLiteral("clockDomainId"),
        QString::fromStdString(
            !event.clockDomainId.empty()
                ? event.clockDomainId
                : lane->clockDomainId));

    const auto inScenario =
        event.tick >= 0 && event.tick < scenario.duration;
    result.insert(QStringLiteral("inScenario"), inScenario);
    std::size_t candidateCount = 0;
    for (const auto& candidate : scenario.events) {
        if (candidate.waveformLinked
            && candidate.laneId == event.laneId
            && candidate.tick == event.tick) {
            ++candidateCount;
        }
    }
    result.insert(
        QStringLiteral("candidateCount"),
        static_cast<qint64>(candidateCount));

    if (inScenario && lane->kind != LaneKind::Group) {
        const auto value =
            sampleObject(project, *lane, event.tick)
                .value(QStringLiteral("value"));
        const auto previousValue = event.tick == 0
            ? QJsonValue{QJsonValue::Null}
            : sampleObject(project, *lane, event.tick - 1)
                  .value(QStringLiteral("value"));
        result.insert(QStringLiteral("previousValue"), previousValue);
        result.insert(QStringLiteral("value"), value);
        result.insert(
            QStringLiteral("edge"),
            automationEdgeKindName(
                classifyAutomationEdge(
                    *lane, event.tick, previousValue, value)));
    } else {
        result.insert(
            QStringLiteral("previousValue"),
            QJsonValue{QJsonValue::Null});
        result.insert(
            QStringLiteral("value"),
            QJsonValue{QJsonValue::Null});
        result.insert(
            QStringLiteral("edge"),
            QJsonValue{QJsonValue::Null});
    }

    QString issue;
    if (event.id.empty()) {
        issue = QStringLiteral("missing-event-id");
    } else if (!inScenario) {
        issue = QStringLiteral("time-out-of-range");
    } else if (lane->kind == LaneKind::Clock
               || lane->kind == LaneKind::Group) {
        issue = QStringLiteral("unsupported-lane");
    } else if (!event.waveformLinked) {
        issue = QStringLiteral("detached-event");
    } else if (candidateCount == 0) {
        issue = QStringLiteral("missing-waveform-edge");
    } else if (candidateCount > 1) {
        issue = QStringLiteral("ambiguous-waveform-edge");
    }
    if (!issue.isEmpty()) {
        result.insert(QStringLiteral("issue"), issue);
        return result;
    }

    ready = true;
    result.insert(QStringLiteral("relationEndpoint"), true);
    result.insert(
        QStringLiteral("issue"),
        QJsonValue{QJsonValue::Null});
    return result;
}

QJsonObject windowSegmentObject(
    const Segment& segment,
    const Tick start,
    const Tick end)
{
    auto result = segmentObject(segment);
    const auto visibleStart = std::max(start, segment.start);
    const auto visibleEnd = std::min(end, segment.end);
    result.insert(
        QStringLiteral("visibleStartTick"),
        integerValue(visibleStart));
    result.insert(
        QStringLiteral("visibleEndTick"),
        integerValue(visibleEnd));
    result.insert(
        QStringLiteral("clipped"),
        visibleStart != segment.start || visibleEnd != segment.end);
    return result;
}

QJsonObject windowLaneObject(
    const Project& project,
    const Lane& lane,
    const Tick start,
    const Tick end)
{
    auto result = laneObject(lane);
    result.remove(QStringLiteral("segments"));
    result.remove(QStringLiteral("extensions"));
    QJsonArray segments;
    auto iterator = std::lower_bound(
        lane.segments.begin(),
        lane.segments.end(),
        start,
        [](const Segment& segment, const Tick tick) {
            return segment.end <= tick;
        });
    for (; iterator != lane.segments.end() && iterator->start < end; ++iterator) {
        segments.append(windowSegmentObject(*iterator, start, end));
    }
    const auto startSample = sampleObject(project, lane, start);
    const auto endSample = sampleObject(project, lane, end - 1);
    result.insert(QStringLiteral("segmentCount"), segments.size());
    result.insert(QStringLiteral("segments"), segments);
    result.insert(
        QStringLiteral("startValue"),
        startSample.value(QStringLiteral("value")));
    result.insert(
        QStringLiteral("startImplicit"),
        startSample.value(QStringLiteral("implicit")));
    result.insert(
        QStringLiteral("endValue"),
        endSample.value(QStringLiteral("value")));
    result.insert(
        QStringLiteral("endImplicit"),
        endSample.value(QStringLiteral("implicit")));
    return result;
}

const Scenario* selectedScenario(
    const Project& project,
    const std::optional<std::string>& scenarioId)
{
    if (!scenarioId) return nullptr;
    const auto iterator = std::find_if(
        project.scenarios.begin(),
        project.scenarios.end(),
        [&scenarioId](const Scenario& scenario) {
            return scenario.id == *scenarioId;
        });
    return iterator == project.scenarios.end() ? nullptr : &*iterator;
}

template<typename Item>
std::optional<std::string> resolveNamedSelector(
    const std::vector<Item>& items,
    const std::string& selector,
    const QString& kind,
    QString& error)
{
    if (selector.empty()) {
        error = QStringLiteral("%1 selector must not be empty.").arg(kind);
        return std::nullopt;
    }
    const Item* idMatch = nullptr;
    for (const auto& item : items) {
        if (item.id != selector) continue;
        if (idMatch) {
            error = QStringLiteral(
                "%1 stable ID '%2' is ambiguous.")
                        .arg(kind, QString::fromStdString(selector));
            return std::nullopt;
        }
        idMatch = &item;
    }
    if (idMatch) return idMatch->id;

    const auto name = QString::fromStdString(selector);
    const Item* matched = nullptr;
    for (const auto& item : items) {
        if (QString::compare(
                QString::fromStdString(item.name),
                name,
                Qt::CaseInsensitive)
            != 0) {
            continue;
        }
        if (matched) {
            error = QStringLiteral(
                "%1 name '%2' is ambiguous; use a stable ID.")
                        .arg(kind, name);
            return std::nullopt;
        }
        matched = &item;
    }
    if (!matched) {
        error = QStringLiteral("%1 '%2' does not exist.").arg(kind, name);
        return std::nullopt;
    }
    const auto duplicateResolvedId = std::count_if(
        items.begin(),
        items.end(),
        [matched](const Item& item) {
            return item.id == matched->id;
        });
    if (duplicateResolvedId != 1) {
        error = QStringLiteral(
            "%1 name '%2' resolves to ambiguous stable ID '%3'.")
                    .arg(
                        kind,
                        name,
                        QString::fromStdString(matched->id));
        return std::nullopt;
    }
    return matched->id;
}

std::optional<std::string> resolveScenarioSelector(
    const Project& project,
    const std::string& selector,
    QString& error)
{
    return resolveNamedSelector(
        project.scenarios,
        selector,
        QStringLiteral("Scenario"),
        error);
}

std::optional<std::string> resolveLaneSelector(
    const Scenario& scenario,
    const std::string& selector,
    QString& error)
{
    return resolveNamedSelector(
        scenario.lanes,
        selector,
        QStringLiteral("Lane"),
        error);
}

std::optional<std::string> resolveMarkerSelector(
    const Scenario& scenario,
    const std::string& selector,
    QString& error)
{
    return resolveNamedSelector(
        scenario.markers,
        selector,
        QStringLiteral("Marker"),
        error);
}

std::optional<std::string> resolveClockSelector(
    const Project& project,
    const std::string& selector,
    QString& error)
{
    return resolveNamedSelector(
        project.clockDomains,
        selector,
        QStringLiteral("Clock domain"),
        error);
}

const Scenario* automationScenario(
    const Project& project,
    const std::optional<std::string>& selector,
    QString& error)
{
    if (selector) {
        const auto id = resolveScenarioSelector(project, *selector, error);
        return id ? selectedScenario(project, id) : nullptr;
    }
    if (project.scenarios.size() == 1) return &project.scenarios.front();
    error = project.scenarios.empty()
        ? QStringLiteral("Project contains no scenario.")
        : QStringLiteral(
              "Project contains multiple scenarios; provide a Scenario ID or unique name.");
    return nullptr;
}

QJsonObject issueObject(const ValidationIssue& issue, const std::string& scenarioId)
{
    return {
        {QStringLiteral("scenarioId"), QString::fromStdString(scenarioId)},
        {QStringLiteral("code"), QString::fromLatin1(toString(issue.code).data())},
        {QStringLiteral("severity"),
         QString::fromLatin1(toString(issue.severity).data())},
        {QStringLiteral("message"), QString::fromStdString(issue.message)},
        {QStringLiteral("laneId"), QString::fromStdString(issue.laneId)},
        {QStringLiteral("tick"), integerValue(issue.tick)},
        {QStringLiteral("eventId"), QString::fromStdString(issue.eventId)},
        {QStringLiteral("relationId"),
         QString::fromStdString(issue.relationId)},
    };
}

struct StableIdentityCandidate {
    std::string scope;
    QString objectKind;
    QString displayKind;
    QString path;
    std::string stableId;
    std::string scenarioId;
    std::string laneId;
    std::optional<std::size_t> scenarioIndex;
    std::optional<std::size_t> laneIndex;
    std::optional<std::size_t> objectIndex;
};

struct StableIdentityAudit {
    QJsonArray issues;
    std::size_t affectedObjectCount{0};
    std::size_t missingStableIdCount{0};
    std::size_t duplicateStableIdCount{0};
};

StableIdentityAudit auditStableIdentities(
    const Project& project,
    const std::optional<std::string>& selectedScenarioId)
{
    std::vector<StableIdentityCandidate> candidates;
    const auto addCandidate =
        [&candidates](
            std::string scope,
            QString objectKind,
            QString displayKind,
            QString path,
            std::string stableId,
            std::string scenarioId = {},
            std::string laneId = {},
            std::optional<std::size_t> scenarioIndex = std::nullopt,
            std::optional<std::size_t> laneIndex = std::nullopt,
            std::optional<std::size_t> objectIndex = std::nullopt) {
            candidates.push_back({
                std::move(scope),
                std::move(objectKind),
                std::move(displayKind),
                std::move(path),
                std::move(stableId),
                std::move(scenarioId),
                std::move(laneId),
                scenarioIndex,
                laneIndex,
                objectIndex,
            });
        };

    addCandidate(
        "project",
        QStringLiteral("project"),
        QStringLiteral("Project"),
        QStringLiteral("projectId"),
        project.id);
    for (std::size_t clockIndex = 0;
         clockIndex < project.clockDomains.size();
         ++clockIndex) {
        addCandidate(
            "project/clock-domain",
            QStringLiteral("clock-domain"),
            QStringLiteral("Clock domain"),
            QStringLiteral("clockDomains[%1].id")
                .arg(static_cast<qulonglong>(clockIndex)),
            project.clockDomains.at(clockIndex).id,
            {},
            {},
            std::nullopt,
            std::nullopt,
            clockIndex);
    }
    for (std::size_t traceIndex = 0;
         traceIndex < project.importedTraces.size();
         ++traceIndex) {
        addCandidate(
            "project/imported-trace",
            QStringLiteral("imported-trace"),
            QStringLiteral("Imported trace"),
            QStringLiteral("importedTraces[%1].id")
                .arg(static_cast<qulonglong>(traceIndex)),
            project.importedTraces.at(traceIndex).id,
            {},
            {},
            std::nullopt,
            std::nullopt,
            traceIndex);
    }
    for (std::size_t scenarioIndex = 0;
         scenarioIndex < project.scenarios.size();
         ++scenarioIndex) {
        const auto& scenario =
            project.scenarios.at(scenarioIndex);
        addCandidate(
            "project/scenario",
            QStringLiteral("scenario"),
            QStringLiteral("Scenario"),
            QStringLiteral("scenarios[%1].id")
                .arg(static_cast<qulonglong>(scenarioIndex)),
            scenario.id,
            scenario.id,
            {},
            scenarioIndex,
            std::nullopt,
            scenarioIndex);
        if (selectedScenarioId
            && scenario.id != *selectedScenarioId) {
            continue;
        }
        const auto scenarioScope =
            "scenario:" + std::to_string(scenarioIndex);
        for (std::size_t laneIndex = 0;
             laneIndex < scenario.lanes.size();
             ++laneIndex) {
            const auto& lane = scenario.lanes.at(laneIndex);
            addCandidate(
                scenarioScope + "/lane",
                QStringLiteral("lane"),
                QStringLiteral("Lane"),
                QStringLiteral("scenarios[%1].lanes[%2].id")
                    .arg(static_cast<qulonglong>(scenarioIndex))
                    .arg(static_cast<qulonglong>(laneIndex)),
                lane.id,
                scenario.id,
                lane.id,
                scenarioIndex,
                laneIndex,
                laneIndex);
            for (std::size_t segmentIndex = 0;
                 segmentIndex < lane.segments.size();
                 ++segmentIndex) {
                addCandidate(
                    scenarioScope + "/segment",
                    QStringLiteral("segment"),
                    QStringLiteral("Segment"),
                    QStringLiteral(
                        "scenarios[%1].lanes[%2].segments[%3].id")
                        .arg(static_cast<qulonglong>(scenarioIndex))
                        .arg(static_cast<qulonglong>(laneIndex))
                        .arg(static_cast<qulonglong>(segmentIndex)),
                    lane.segments.at(segmentIndex).id,
                    scenario.id,
                    lane.id,
                    scenarioIndex,
                    laneIndex,
                    segmentIndex);
            }
        }
        for (std::size_t eventIndex = 0;
             eventIndex < scenario.events.size();
             ++eventIndex) {
            addCandidate(
                scenarioScope + "/event",
                QStringLiteral("event"),
                QStringLiteral("Event"),
                QStringLiteral("scenarios[%1].events[%2].id")
                    .arg(static_cast<qulonglong>(scenarioIndex))
                    .arg(static_cast<qulonglong>(eventIndex)),
                scenario.events.at(eventIndex).id,
                scenario.id,
                scenario.events.at(eventIndex).laneId,
                scenarioIndex,
                std::nullopt,
                eventIndex);
        }
        for (std::size_t relationIndex = 0;
             relationIndex < scenario.relations.size();
             ++relationIndex) {
            addCandidate(
                scenarioScope + "/relation",
                QStringLiteral("relation"),
                QStringLiteral("Relation"),
                QStringLiteral("scenarios[%1].relations[%2].id")
                    .arg(static_cast<qulonglong>(scenarioIndex))
                    .arg(static_cast<qulonglong>(relationIndex)),
                scenario.relations.at(relationIndex).id,
                scenario.id,
                {},
                scenarioIndex,
                std::nullopt,
                relationIndex);
        }
        for (std::size_t markerIndex = 0;
             markerIndex < scenario.markers.size();
             ++markerIndex) {
            addCandidate(
                scenarioScope + "/marker",
                QStringLiteral("marker"),
                QStringLiteral("Marker"),
                QStringLiteral("scenarios[%1].markers[%2].id")
                    .arg(static_cast<qulonglong>(scenarioIndex))
                    .arg(static_cast<qulonglong>(markerIndex)),
                scenario.markers.at(markerIndex).id,
                scenario.id,
                {},
                scenarioIndex,
                std::nullopt,
                markerIndex);
        }
    }

    std::map<
        std::pair<std::string, std::string>,
        std::size_t> counts;
    for (const auto& candidate : candidates) {
        ++counts[{candidate.scope, candidate.stableId}];
    }

    StableIdentityAudit audit;
    const auto appendIssue =
        [&audit](
            const StableIdentityCandidate& candidate,
            const QString& code,
            const QString& message,
            const std::size_t idCount) {
            QJsonObject issue{
                {QStringLiteral("scenarioId"),
                 QString::fromStdString(candidate.scenarioId)},
                {QStringLiteral("code"), code},
                {QStringLiteral("severity"),
                 QStringLiteral("error")},
                {QStringLiteral("message"), message},
                {QStringLiteral("objectKind"),
                 candidate.objectKind},
                {QStringLiteral("path"), candidate.path},
                {QStringLiteral("stableId"),
                 QString::fromStdString(candidate.stableId)},
                {QStringLiteral("idCount"),
                 static_cast<qint64>(idCount)},
                {QStringLiteral("laneId"),
                 QString::fromStdString(candidate.laneId)},
            };
            if (candidate.scenarioIndex) {
                issue.insert(
                    QStringLiteral("scenarioIndex"),
                    static_cast<qint64>(
                        *candidate.scenarioIndex));
            }
            if (candidate.laneIndex) {
                issue.insert(
                    QStringLiteral("laneIndex"),
                    static_cast<qint64>(*candidate.laneIndex));
            }
            if (candidate.objectIndex) {
                issue.insert(
                    QStringLiteral("objectIndex"),
                    static_cast<qint64>(*candidate.objectIndex));
            }
            audit.issues.append(issue);
        };
    for (const auto& candidate : candidates) {
        const auto idCount = counts.at(
            {candidate.scope, candidate.stableId});
        const auto missing = candidate.stableId.empty();
        const auto duplicate = idCount > 1;
        if (!missing && !duplicate) continue;
        ++audit.affectedObjectCount;
        if (missing) {
            ++audit.missingStableIdCount;
            appendIssue(
                candidate,
                QStringLiteral("missing-stable-id"),
                QStringLiteral("%1 at %2 has an empty stable ID.")
                    .arg(
                        candidate.displayKind,
                        candidate.path),
                idCount);
        }
        if (duplicate) {
            ++audit.duplicateStableIdCount;
            appendIssue(
                candidate,
                QStringLiteral("duplicate-stable-id"),
                QStringLiteral(
                    "%1 at %2 shares stable ID '%3' with %4 objects in the same scope.")
                    .arg(
                        candidate.displayKind,
                        candidate.path,
                        QString::fromStdString(
                            candidate.stableId))
                    .arg(
                        static_cast<qulonglong>(idCount)),
                idCount);
        }
    }
    return audit;
}

bool containsOnly(
    const QJsonObject& object,
    const std::initializer_list<QString>& allowed,
    QString& error)
{
    const std::set<QString> names(allowed.begin(), allowed.end());
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
        if (!names.contains(iterator.key())) {
            error = QStringLiteral("Unknown field '%1'.").arg(iterator.key());
            return false;
        }
    }
    return true;
}

std::optional<std::int64_t> integerField(
    const QJsonObject& object,
    const QString& name,
    const bool required,
    QString& error)
{
    if (!object.contains(name)) {
        if (required) error = QStringLiteral("Missing field '%1'.").arg(name);
        return std::nullopt;
    }
    const auto value = object.value(name);
    if (value.isString()) {
        bool valid = false;
        const auto parsed = value.toString().toLongLong(&valid);
        if (valid) return parsed;
    } else if (value.isDouble()) {
        const auto number = value.toDouble();
        constexpr auto safeJsonInteger = 9'007'199'254'740'991.0;
        if (std::isfinite(number)
            && std::trunc(number) == number
            && std::abs(number) <= safeJsonInteger
            && number >= static_cast<double>(std::numeric_limits<std::int64_t>::min())
            && number <= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
            return static_cast<std::int64_t>(number);
        }
    }
    error = QStringLiteral(
        "Field '%1' must be an integer string or a safe JSON integer.")
                .arg(name);
    return std::nullopt;
}

std::optional<std::string> requiredString(
    const QJsonObject& object,
    const QString& name,
    QString& error)
{
    const auto value = object.value(name);
    if (!value.isString() || value.toString().trimmed().isEmpty()) {
        error = QStringLiteral("Field '%1' must be a non-empty string.").arg(name);
        return std::nullopt;
    }
    return value.toString().toStdString();
}

bool enumMapField(
    const QJsonObject& object,
    const QString& name,
    const std::uint32_t width,
    const bool isSigned,
    std::map<std::string, std::string>& result,
    QString& error)
{
    const auto value = object.value(name);
    if (!value.isObject() || value.toObject().isEmpty()) {
        error = QStringLiteral(
            "Field '%1' must be a non-empty object of symbol-to-value strings.")
                    .arg(name);
        return false;
    }

    Lane encodedLane;
    encodedLane.kind = LaneKind::Bus;
    encodedLane.width = width;
    encodedLane.isSigned = isSigned;
    result.clear();
    const auto entries = value.toObject();
    for (auto iterator = entries.begin(); iterator != entries.end(); ++iterator) {
        const auto symbol = iterator.key();
        if (symbol.trimmed().isEmpty() || symbol.trimmed() != symbol) {
            error = QStringLiteral(
                "Enum symbols must be non-empty and cannot have surrounding whitespace.");
            return false;
        }
        if (!iterator.value().isString()
            || iterator.value().toString().trimmed().isEmpty()) {
            error = QStringLiteral(
                "Enum value for symbol '%1' must be a non-empty string.")
                        .arg(symbol);
            return false;
        }
        const auto mappedValue =
            iterator.value().toString().trimmed().toStdString();
        const auto validation = validateLaneValue(encodedLane, mappedValue);
        if (!validation.valid) {
            error = QStringLiteral(
                "Enum value for symbol '%1' is invalid for width %2: %3")
                        .arg(symbol)
                        .arg(width)
                        .arg(QString::fromStdString(validation.error));
            return false;
        }
        result.emplace(
            symbol.toStdString(),
            validation.normalizedValue);
    }
    return true;
}

template<typename Resolver>
bool canonicalizeSelectorField(
    QJsonObject& object,
    const QString& field,
    Resolver&& resolver,
    const bool allowEmpty,
    QString& error)
{
    if (!object.contains(field)) return true;
    const auto value = object.value(field);
    if (!value.isString()) {
        error = QStringLiteral("Field '%1' must be a string.").arg(field);
        return false;
    }
    const auto selector = value.toString().trimmed();
    if (selector.isEmpty()) {
        if (allowEmpty) {
            object.insert(field, QString{});
            return true;
        }
        error = QStringLiteral("Field '%1' must be a non-empty string.")
                    .arg(field);
        return false;
    }
    const auto id = resolver(selector.toStdString(), error);
    if (!id) return false;
    object.insert(field, QString::fromStdString(*id));
    return true;
}

bool canonicalizeLaneSelectorField(
    QJsonObject& object,
    const QString& field,
    const Scenario& scenario,
    const bool allowEmpty,
    QString& error)
{
    return canonicalizeSelectorField(
        object,
        field,
        [&scenario](
            const std::string& selector,
            QString& selectorError) {
            return resolveLaneSelector(scenario, selector, selectorError);
        },
        allowEmpty,
        error);
}

bool canonicalizeClockSelectorField(
    QJsonObject& object,
    const QString& field,
    const Project& project,
    const bool allowEmpty,
    QString& error)
{
    return canonicalizeSelectorField(
        object,
        field,
        [&project](
            const std::string& selector,
            QString& selectorError) {
            return resolveClockSelector(project, selector, selectorError);
        },
        allowEmpty,
        error);
}

bool canonicalizeMarkerSelectorField(
    QJsonObject& object,
    const QString& field,
    const Scenario& scenario,
    QString& error)
{
    return canonicalizeSelectorField(
        object,
        field,
        [&scenario](
            const std::string& selector,
            QString& selectorError) {
            return resolveMarkerSelector(
                scenario, selector, selectorError);
        },
        false,
        error);
}

bool canonicalizeLaneSelectorArray(
    QJsonObject& object,
    const QString& field,
    const Scenario& scenario,
    QString& error)
{
    if (!object.contains(field)) return true;
    const auto value = object.value(field);
    if (!value.isArray()) {
        error = QStringLiteral("Field '%1' must be an array.").arg(field);
        return false;
    }
    QJsonArray resolved;
    for (const auto& entry : value.toArray()) {
        if (!entry.isString() || entry.toString().trimmed().isEmpty()) {
            error = QStringLiteral(
                "Every '%1' entry must be a non-empty string.")
                        .arg(field);
            return false;
        }
        const auto id = resolveLaneSelector(
            scenario, entry.toString().trimmed().toStdString(), error);
        if (!id) return false;
        resolved.append(QString::fromStdString(*id));
    }
    object.insert(field, resolved);
    return true;
}

bool canonicalizeNestedLaneSelectors(
    QJsonObject& object,
    const QString& arrayField,
    const std::initializer_list<QString>& selectorFields,
    const Scenario& scenario,
    QString& error)
{
    if (!object.contains(arrayField)) return true;
    const auto value = object.value(arrayField);
    if (!value.isArray()) {
        error = QStringLiteral("Field '%1' must be an array.")
                    .arg(arrayField);
        return false;
    }
    QJsonArray resolved;
    for (const auto& entry : value.toArray()) {
        if (!entry.isObject()) {
            error = QStringLiteral("Every '%1' entry must be an object.")
                        .arg(arrayField);
            return false;
        }
        auto item = entry.toObject();
        for (const auto& selectorField : selectorFields) {
            if (!canonicalizeLaneSelectorField(
                    item,
                    selectorField,
                    scenario,
                    false,
                    error)) {
                return false;
            }
        }
        resolved.append(item);
    }
    object.insert(arrayField, resolved);
    return true;
}

bool canonicalizeOperationSelectors(
    const Project& project,
    const Scenario& scenario,
    const QString& operationName,
    QJsonObject& operation,
    QString& error)
{
    const auto hasSingleLane =
        operationName == QStringLiteral("set-range")
        || operationName == QStringLiteral("set-sequence")
        || operationName == QStringLiteral("assert-value")
        || operationName == QStringLiteral("clear-range")
        || operationName == QStringLiteral("transfer-range")
        || operationName == QStringLiteral("rename-lane")
        || operationName == QStringLiteral("delete-signal")
        || operationName == QStringLiteral("move-signal")
        || operationName == QStringLiteral("update-signal")
        || operationName == QStringLiteral("duplicate-signal");
    if (hasSingleLane
        && !canonicalizeLaneSelectorField(
            operation,
            QStringLiteral("laneId"),
            scenario,
            false,
            error)) {
        return false;
    }
    if ((operationName == QStringLiteral("clear-range")
         || operationName == QStringLiteral("transfer-range"))
        && !canonicalizeLaneSelectorArray(
            operation,
            QStringLiteral("laneIds"),
            scenario,
            error)) {
        return false;
    }
    if (operationName == QStringLiteral("set-range")
        && !canonicalizeNestedLaneSelectors(
            operation,
            QStringLiteral("assignments"),
            {QStringLiteral("laneId")},
            scenario,
            error)) {
        return false;
    }
    if (operationName == QStringLiteral("set-sequence")
        && !canonicalizeNestedLaneSelectors(
            operation,
            QStringLiteral("sequences"),
            {QStringLiteral("laneId")},
            scenario,
            error)) {
        return false;
    }
    if (operationName == QStringLiteral("transfer-range")
        && !canonicalizeNestedLaneSelectors(
            operation,
            QStringLiteral("mappings"),
            {QStringLiteral("sourceLaneId"),
             QStringLiteral("targetLaneId")},
            scenario,
            error)) {
        return false;
    }
    if (operationName == QStringLiteral("move-signal")
        || operationName == QStringLiteral("duplicate-signal")) {
        if (!canonicalizeLaneSelectorField(
                operation,
                QStringLiteral("beforeLaneId"),
                scenario,
                false,
                error)
            || !canonicalizeLaneSelectorField(
                operation,
                QStringLiteral("afterLaneId"),
                scenario,
                false,
                error)) {
            return false;
        }
    }
    if (operationName == QStringLiteral("update-group")
        || operationName == QStringLiteral("move-group")
        || operationName == QStringLiteral("delete-group")) {
        if (!canonicalizeLaneSelectorField(
                operation,
                QStringLiteral("groupId"),
                scenario,
                false,
                error)) {
            return false;
        }
    }
    if (operationName == QStringLiteral("move-group")) {
        if (!canonicalizeLaneSelectorField(
                operation,
                QStringLiteral("beforeLaneId"),
                scenario,
                false,
                error)
            || !canonicalizeLaneSelectorField(
                operation,
                QStringLiteral("afterLaneId"),
                scenario,
                false,
                error)) {
            return false;
        }
    }
    if ((operationName == QStringLiteral("update-signal")
         || operationName == QStringLiteral("duplicate-signal"))
        && !canonicalizeLaneSelectorField(
                operation,
                QStringLiteral("groupId"),
                scenario,
                true,
                error)) {
        return false;
    }
    if (operationName == QStringLiteral("update-signal")) {
        if (!canonicalizeClockSelectorField(
                operation,
                QStringLiteral("clockDomainId"),
                project,
                true,
                error)) {
            return false;
        }
    }
    if (operationName == QStringLiteral("add-relation")
        || operationName == QStringLiteral("update-relation")) {
        if (!canonicalizeLaneSelectorField(
                operation,
                QStringLiteral("sourceLaneId"),
                scenario,
                false,
                error)
            || !canonicalizeLaneSelectorField(
                operation,
                QStringLiteral("targetLaneId"),
                scenario,
                false,
                error)) {
            return false;
        }
    }
    if ((operationName == QStringLiteral("update-marker")
         || operationName == QStringLiteral("delete-marker"))
        && !canonicalizeMarkerSelectorField(
            operation,
            QStringLiteral("markerId"),
            scenario,
            error)) {
        return false;
    }
    if (operationName == QStringLiteral("add-signal")
        && !canonicalizeLaneSelectorField(
            operation,
            QStringLiteral("groupId"),
            scenario,
            false,
            error)) {
        return false;
    }
    if (operationName == QStringLiteral("add-signal")
        && operation.value(QStringLiteral("kind"))
                   .toString()
                   .trimmed()
                   .compare(QStringLiteral("clock"), Qt::CaseInsensitive)
            != 0
        && !canonicalizeClockSelectorField(
            operation,
            QStringLiteral("clockDomainId"),
            project,
            false,
            error)) {
        return false;
    }
    const auto hasClockSelector =
        operationName == QStringLiteral("set-range")
        || operationName == QStringLiteral("set-sequence")
        || operationName == QStringLiteral("assert-value")
        || operationName == QStringLiteral("clear-range")
        || operationName == QStringLiteral("transfer-range")
        || operationName == QStringLiteral("set-duration")
        || operationName == QStringLiteral("update-clock")
        || operationName == QStringLiteral("add-relation")
        || operationName == QStringLiteral("update-relation")
        || operationName == QStringLiteral("add-marker")
        || operationName == QStringLiteral("update-marker");
    if (hasClockSelector
        && !canonicalizeClockSelectorField(
            operation,
            QStringLiteral("clockId"),
            project,
            false,
            error)) {
        return false;
    }
    return true;
}

bool resolveClockContext(
    const Project& project,
    const Scenario& scenario,
    const QJsonObject& object,
    const std::vector<std::string>& laneIds,
    std::optional<std::string>& clockId,
    QString& error)
{
    if (object.contains(QStringLiteral("clockId"))) {
        const auto explicitId = requiredString(
            object, QStringLiteral("clockId"), error);
        if (!explicitId) return false;
        if (!findClock(project, *explicitId)) {
            error = QStringLiteral("Clock domain '%1' does not exist.")
                        .arg(QString::fromStdString(*explicitId));
            return false;
        }
        clockId = *explicitId;
        return true;
    }

    std::set<std::string> inferred;
    for (const auto& laneId : laneIds) {
        const auto* lane = findLane(scenario, laneId);
        if (lane && !lane->clockDomainId.empty()) {
            inferred.insert(lane->clockDomainId);
        }
    }
    if (inferred.size() == 1) {
        clockId = *inferred.begin();
    } else if (inferred.empty() && project.clockDomains.size() == 1) {
        clockId = project.clockDomains.front().id;
    }
    return true;
}

std::optional<Tick> automationTickField(
    const Project& project,
    const QJsonObject& object,
    const QString& tickName,
    const QString& timeName,
    const std::optional<std::string>& clockId,
    QString& error,
    const bool allowCycle = true)
{
    const auto hasTick = object.contains(tickName);
    const auto hasTime = object.contains(timeName);
    if (hasTick == hasTime) {
        error = QStringLiteral("Use exactly one of '%1' or '%2'.")
                    .arg(tickName, timeName);
        return std::nullopt;
    }
    if (hasTick) return integerField(object, tickName, true, error);
    const auto value = object.value(timeName);
    if (!value.isString() || value.toString().trimmed().isEmpty()) {
        error = QStringLiteral("Field '%1' must be a non-empty time string.")
                    .arg(timeName);
        return std::nullopt;
    }
    if (!allowCycle
        && value.toString()
               .trimmed()
               .startsWith(QStringLiteral("cycle"), Qt::CaseInsensitive)) {
        error = QStringLiteral(
            "Field '%1' does not accept cycle-based time.")
                    .arg(timeName);
        return std::nullopt;
    }
    const auto parsed = parseAutomationTime(project, value.toString(), clockId);
    if (!parsed.ok()) {
        error = QStringLiteral("Field '%1': %2").arg(timeName, parsed.error);
        return std::nullopt;
    }
    return parsed.tick;
}

bool uniqueLaneName(
    const Scenario& scenario,
    const std::string& name,
    const std::string_view ignoredLaneId = {})
{
    const auto candidate = QString::fromStdString(name);
    return std::none_of(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [&candidate, ignoredLaneId](const Lane& lane) {
            return lane.id != ignoredLaneId
                && QString::compare(
                       QString::fromStdString(lane.name),
                       candidate,
                       Qt::CaseInsensitive)
                    == 0;
        });
}

std::string availableLaneName(
    const Scenario& scenario,
    const std::string_view base)
{
    auto candidate = std::string(base);
    if (uniqueLaneName(scenario, candidate)) return candidate;
    for (std::size_t suffix = 2;; ++suffix) {
        candidate =
            std::string(base) + "_" + std::to_string(suffix);
        if (uniqueLaneName(scenario, candidate)) return candidate;
    }
}

bool uniqueMarkerName(
    const Scenario& scenario,
    const std::string& name,
    const std::string_view ignoredMarkerId = {})
{
    const auto candidate = QString::fromStdString(name);
    return std::none_of(
        scenario.markers.begin(),
        scenario.markers.end(),
        [&candidate, ignoredMarkerId](const Marker& marker) {
            return marker.id != ignoredMarkerId
                && QString::compare(
                       QString::fromStdString(marker.name),
                       candidate,
                       Qt::CaseInsensitive)
                    == 0;
        });
}

std::uint64_t stableHash(const std::string_view value)
{
    auto hash = std::uint64_t{14'695'981'039'346'656'037ULL};
    for (const auto byte : value) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1'099'511'628'211ULL;
    }
    return hash;
}

std::string hexadecimalToken(std::uint64_t value)
{
    static constexpr std::string_view digits{"0123456789abcdef"};
    std::string result(16, '0');
    for (auto index = result.size(); index > 0; --index) {
        result[index - 1] = digits.at(value & 0x0fU);
        value >>= 4U;
    }
    return result;
}

template<typename IsTaken>
std::string deterministicStableId(
    const std::string_view prefix,
    const std::string_view seed,
    IsTaken&& isTaken)
{
    const auto base =
        std::string(prefix) + "-auto-" + hexadecimalToken(stableHash(seed));
    if (!isTaken(base)) return base;
    for (std::size_t suffix = 2;; ++suffix) {
        const auto candidate = base + "-" + std::to_string(suffix);
        if (!isTaken(candidate)) return candidate;
    }
}

std::string deterministicReadableColor(
    const Scenario& scenario,
    const std::string_view seed,
    const std::string_view excludedColor = {})
{
    static constexpr std::array<std::string_view, 12> palette{
        "#4fc3f7",
        "#81c784",
        "#ffb74d",
        "#ba68c8",
        "#e57373",
        "#64b5f6",
        "#ffd54f",
        "#4db6ac",
        "#f06292",
        "#a1887f",
        "#90a4ae",
        "#7986cb",
    };
    const auto excluded = QString::fromLatin1(
        excludedColor.data(),
        static_cast<qsizetype>(excludedColor.size()));
    std::vector<std::size_t> candidates;
    std::vector<std::size_t> unused;
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const auto color = QString::fromLatin1(palette.at(index).data());
        if (!excluded.isEmpty()
            && QString::compare(
                   color, excluded, Qt::CaseInsensitive)
                == 0) {
            continue;
        }
        candidates.push_back(index);
        const auto used = std::any_of(
            scenario.lanes.begin(),
            scenario.lanes.end(),
            [&color](const Lane& lane) {
                return QString::compare(
                           color,
                           QString::fromStdString(lane.color),
                           Qt::CaseInsensitive)
                    == 0;
            });
        if (!used) unused.push_back(index);
    }
    if (candidates.empty()) return std::string(palette.front());
    const auto& pool = unused.empty() ? candidates : unused;
    const auto colorIndex =
        pool.at(stableHash(seed) % pool.size());
    return std::string(palette.at(colorIndex));
}

void appendIdentityField(std::string& seed, const std::string_view value)
{
    seed += std::to_string(value.size());
    seed.push_back(':');
    seed.append(value);
    seed.push_back(';');
}

void appendIdentityExtensions(
    std::string& seed,
    const JsonExtensions& extensions)
{
    for (const auto& [name, encoded] : extensions) {
        appendIdentityField(seed, name);
        appendIdentityField(seed, encoded);
    }
}

void canonicalizeGeneratedIdentities(
    const Project& source,
    Project& candidate)
{
    std::set<std::string> sourceSegmentIds;
    std::set<std::string> sourceEventIds;
    for (const auto& scenario : source.scenarios) {
        for (const auto& lane : scenario.lanes) {
            for (const auto& segment : lane.segments) {
                sourceSegmentIds.insert(segment.id);
            }
        }
        for (const auto& event : scenario.events) {
            sourceEventIds.insert(event.id);
        }
    }

    auto usedSegmentIds = sourceSegmentIds;
    std::map<std::string, std::string> segmentIdMap;
    for (auto& scenario : candidate.scenarios) {
        for (auto& lane : scenario.lanes) {
            for (auto& segment : lane.segments) {
                if (sourceSegmentIds.contains(segment.id)) {
                    usedSegmentIds.insert(segment.id);
                    continue;
                }
                std::string seed;
                appendIdentityField(seed, scenario.id);
                appendIdentityField(seed, lane.id);
                appendIdentityField(seed, std::to_string(segment.start));
                appendIdentityField(seed, std::to_string(segment.end));
                appendIdentityField(seed, segment.value);
                appendIdentityExtensions(seed, segment.extensions);
                const auto oldId = segment.id;
                segment.id = deterministicStableId(
                    "segment",
                    seed,
                    [&usedSegmentIds](const std::string& id) {
                        return usedSegmentIds.contains(id);
                    });
                usedSegmentIds.insert(segment.id);
                segmentIdMap.emplace(oldId, segment.id);
            }
        }
    }
    for (auto& scenario : candidate.scenarios) {
        for (auto& event : scenario.events) {
            const auto mapped = segmentIdMap.find(event.linkedSegmentId);
            if (mapped != segmentIdMap.end()) {
                event.linkedSegmentId = mapped->second;
            }
        }
    }

    auto usedEventIds = sourceEventIds;
    std::map<std::string, std::string> eventIdMap;
    for (auto& scenario : candidate.scenarios) {
        for (auto& event : scenario.events) {
            if (sourceEventIds.contains(event.id)) {
                usedEventIds.insert(event.id);
                continue;
            }
            std::string seed;
            appendIdentityField(seed, scenario.id);
            appendIdentityField(seed, event.laneId);
            appendIdentityField(seed, std::to_string(event.tick));
            appendIdentityField(seed, event.value);
            appendIdentityField(seed, toString(event.action));
            appendIdentityField(seed, event.linkedSegmentId);
            appendIdentityExtensions(seed, event.extensions);
            const auto oldId = event.id;
            event.id = deterministicStableId(
                "event",
                seed,
                [&usedEventIds](const std::string& id) {
                    return usedEventIds.contains(id);
                });
            usedEventIds.insert(event.id);
            eventIdMap.emplace(oldId, event.id);
        }
    }
    for (auto& scenario : candidate.scenarios) {
        std::stable_sort(
            scenario.events.begin(),
            scenario.events.end(),
            [](const Event& left, const Event& right) {
                return left.tick < right.tick
                    || (left.tick == right.tick && left.id < right.id);
            });
        for (auto& relation : scenario.relations) {
            const auto sourceEvent = eventIdMap.find(relation.sourceEventId);
            if (sourceEvent != eventIdMap.end()) {
                relation.sourceEventId = sourceEvent->second;
            }
            const auto targetEvent = eventIdMap.find(relation.targetEventId);
            if (targetEvent != eventIdMap.end()) {
                relation.targetEventId = targetEvent->second;
            }
        }
    }
}

std::size_t segmentCount(const Scenario& scenario)
{
    std::size_t result = 0;
    for (const auto& lane : scenario.lanes) {
        result += lane.segments.size();
    }
    return result;
}

QJsonObject scenarioChangeObject(
    const Scenario* before,
    const Scenario* after)
{
    std::vector<std::string> addedLaneIds;
    std::vector<std::string> removedLaneIds;
    std::vector<std::string> changedLaneIds;
    std::vector<std::string> movedLaneIds;
    if (before && after) {
        for (std::size_t beforeIndex = 0;
             beforeIndex < before->lanes.size();
             ++beforeIndex) {
            const auto& lane = before->lanes.at(beforeIndex);
            const auto iterator = std::find_if(
                after->lanes.begin(),
                after->lanes.end(),
                [&lane](const Lane& candidate) {
                    return candidate.id == lane.id;
                });
            if (iterator == after->lanes.end()) {
                removedLaneIds.push_back(lane.id);
                continue;
            }
            if (*iterator != lane) changedLaneIds.push_back(lane.id);
            const auto afterIndex = static_cast<std::size_t>(
                std::distance(after->lanes.begin(), iterator));
            if (afterIndex != beforeIndex) movedLaneIds.push_back(lane.id);
        }
        for (const auto& lane : after->lanes) {
            if (!findLane(*before, lane.id)) addedLaneIds.push_back(lane.id);
        }
    } else if (before) {
        for (const auto& lane : before->lanes) {
            removedLaneIds.push_back(lane.id);
        }
    } else if (after) {
        for (const auto& lane : after->lanes) {
            addedLaneIds.push_back(lane.id);
        }
    }

    return {
        {QStringLiteral("scenarioId"),
         QString::fromStdString(after ? after->id : before->id)},
        {QStringLiteral("added"), before == nullptr},
        {QStringLiteral("removed"), after == nullptr},
        {QStringLiteral("beforeDurationTick"),
         before ? integerValue(before->duration)
                : QJsonValue{QJsonValue::Null}},
        {QStringLiteral("afterDurationTick"),
         after ? integerValue(after->duration)
               : QJsonValue{QJsonValue::Null}},
        {QStringLiteral("beforeLaneCount"),
         static_cast<qint64>(before ? before->lanes.size() : 0)},
        {QStringLiteral("afterLaneCount"),
         static_cast<qint64>(after ? after->lanes.size() : 0)},
        {QStringLiteral("beforeSegmentCount"),
         static_cast<qint64>(before ? segmentCount(*before) : 0)},
        {QStringLiteral("afterSegmentCount"),
         static_cast<qint64>(after ? segmentCount(*after) : 0)},
        {QStringLiteral("beforeEventCount"),
         static_cast<qint64>(before ? before->events.size() : 0)},
        {QStringLiteral("afterEventCount"),
         static_cast<qint64>(after ? after->events.size() : 0)},
        {QStringLiteral("beforeRelationCount"),
         static_cast<qint64>(before ? before->relations.size() : 0)},
        {QStringLiteral("afterRelationCount"),
         static_cast<qint64>(after ? after->relations.size() : 0)},
        {QStringLiteral("beforeMarkerCount"),
         static_cast<qint64>(before ? before->markers.size() : 0)},
        {QStringLiteral("afterMarkerCount"),
         static_cast<qint64>(after ? after->markers.size() : 0)},
        {QStringLiteral("addedLaneIds"), stringArray(addedLaneIds)},
        {QStringLiteral("removedLaneIds"), stringArray(removedLaneIds)},
        {QStringLiteral("changedLaneIds"), stringArray(changedLaneIds)},
        {QStringLiteral("movedLaneIds"), stringArray(movedLaneIds)},
    };
}

QJsonObject projectChangeSummary(
    const Project& before,
    const Project& after)
{
    std::vector<std::string> addedClockIds;
    std::vector<std::string> removedClockIds;
    std::vector<std::string> changedClockIds;
    for (const auto& clock : before.clockDomains) {
        const auto* updated = findClock(after, clock.id);
        if (!updated) {
            removedClockIds.push_back(clock.id);
        } else if (*updated != clock) {
            changedClockIds.push_back(clock.id);
        }
    }
    for (const auto& clock : after.clockDomains) {
        if (!findClock(before, clock.id)) addedClockIds.push_back(clock.id);
    }

    std::set<std::string> scenarioIds;
    for (const auto& scenario : before.scenarios) {
        scenarioIds.insert(scenario.id);
    }
    for (const auto& scenario : after.scenarios) {
        scenarioIds.insert(scenario.id);
    }
    QJsonArray scenarios;
    for (const auto& scenarioId : scenarioIds) {
        const auto beforeIterator = std::find_if(
            before.scenarios.begin(),
            before.scenarios.end(),
            [&scenarioId](const Scenario& scenario) {
                return scenario.id == scenarioId;
            });
        const auto afterIterator = std::find_if(
            after.scenarios.begin(),
            after.scenarios.end(),
            [&scenarioId](const Scenario& scenario) {
                return scenario.id == scenarioId;
            });
        const auto* beforeScenario = beforeIterator == before.scenarios.end()
            ? nullptr
            : &*beforeIterator;
        const auto* afterScenario = afterIterator == after.scenarios.end()
            ? nullptr
            : &*afterIterator;
        if (beforeScenario && afterScenario
            && *beforeScenario == *afterScenario) {
            continue;
        }
        scenarios.append(
            scenarioChangeObject(beforeScenario, afterScenario));
    }

    return {
        {QStringLiteral("projectChanged"), before != after},
        {QStringLiteral("beforeClockCount"),
         static_cast<qint64>(before.clockDomains.size())},
        {QStringLiteral("afterClockCount"),
         static_cast<qint64>(after.clockDomains.size())},
        {QStringLiteral("addedClockIds"), stringArray(addedClockIds)},
        {QStringLiteral("removedClockIds"), stringArray(removedClockIds)},
        {QStringLiteral("changedClockIds"), stringArray(changedClockIds)},
        {QStringLiteral("importedTracesChanged"),
         before.importedTraces != after.importedTraces},
        {QStringLiteral("linkedResourcesChanged"),
         before.linkedResources != after.linkedResources},
        {QStringLiteral("scenarioChangeCount"), scenarios.size()},
        {QStringLiteral("scenarios"), scenarios},
    };
}

std::optional<std::string> scenarioIdFromBatch(
    const Project& project,
    const QJsonObject& batch,
    const std::optional<std::string>& overrideId,
    QString& error)
{
    std::optional<std::string> documentId;
    if (batch.contains(QStringLiteral("scenarioId"))) {
        const auto value = batch.value(QStringLiteral("scenarioId"));
        if (!value.isString() || value.toString().trimmed().isEmpty()) {
            error = QStringLiteral("Field 'scenarioId' must be a non-empty string.");
            return std::nullopt;
        }
        documentId = resolveScenarioSelector(
            project,
            value.toString().trimmed().toStdString(),
            error);
        if (!documentId) return std::nullopt;
    }
    std::optional<std::string> resolvedOverride;
    if (overrideId) {
        resolvedOverride =
            resolveScenarioSelector(project, *overrideId, error);
        if (!resolvedOverride) return std::nullopt;
    }
    if (resolvedOverride && documentId
        && *resolvedOverride != *documentId) {
        error = QStringLiteral(
            "CLI scenario override does not match operations scenarioId.");
        return std::nullopt;
    }
    if (resolvedOverride) return resolvedOverride;
    if (documentId) return documentId;
    if (project.scenarios.size() == 1) return project.scenarios.front().id;
    error = project.scenarios.empty()
        ? QStringLiteral("Project contains no scenario.")
        : QStringLiteral(
              "Project contains multiple scenarios; provide scenarioId.");
    return std::nullopt;
}

QJsonObject operationResult(
    const int index,
    const QString& operation,
    const bool changed)
{
    return {
        {QStringLiteral("index"), index},
        {QStringLiteral("op"), operation},
        {QStringLiteral("changed"), changed},
    };
}

bool applySetRange(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("laneId"),
             QStringLiteral("assignments"),
             QStringLiteral("startTick"),
             QStringLiteral("start"),
             QStringLiteral("endTick"),
             QStringLiteral("end"),
             QStringLiteral("clockId"),
             QStringLiteral("value")},
            error)) {
        return false;
    }

    std::vector<LaneRangeAssignment> assignments;
    std::vector<std::string> laneIds;
    if (operation.contains(QStringLiteral("assignments"))) {
        if (operation.contains(QStringLiteral("laneId"))
            || operation.contains(QStringLiteral("value"))) {
            error = QStringLiteral(
                "Use either 'assignments' or 'laneId' with 'value', not both.");
            return false;
        }
        const auto values = operation.value(QStringLiteral("assignments"));
        if (!values.isArray() || values.toArray().isEmpty()) {
            error = QStringLiteral(
                "Field 'assignments' must be a non-empty array.");
            return false;
        }
        for (const auto& entry : values.toArray()) {
            if (!entry.isObject()) {
                error = QStringLiteral(
                    "Every assignments entry must be an object.");
                return false;
            }
            const auto object = entry.toObject();
            if (!containsOnly(
                    object,
                    {QStringLiteral("laneId"), QStringLiteral("value")},
                    error)) {
                return false;
            }
            const auto laneId = requiredString(
                object, QStringLiteral("laneId"), error);
            const auto value = requiredString(
                object, QStringLiteral("value"), error);
            if (!laneId || !value) return false;
            if (std::find(laneIds.begin(), laneIds.end(), *laneId)
                != laneIds.end()) {
                error = QStringLiteral(
                    "Field 'assignments' contains duplicate Lane IDs.");
                return false;
            }
            const auto* lane = findLane(scenario, *laneId);
            if (!lane || lane->kind == LaneKind::Group) {
                error = QStringLiteral("Lane '%1' does not exist.")
                            .arg(QString::fromStdString(*laneId));
                return false;
            }
            laneIds.push_back(*laneId);
            assignments.push_back({*laneId, *value, {}});
        }
    } else {
        const auto laneId = requiredString(
            operation, QStringLiteral("laneId"), error);
        const auto value = requiredString(
            operation, QStringLiteral("value"), error);
        if (!laneId || !value) return false;
        const auto* lane = findLane(scenario, *laneId);
        if (!lane || lane->kind == LaneKind::Group) {
            error = QStringLiteral("Lane '%1' does not exist.")
                        .arg(QString::fromStdString(*laneId));
            return false;
        }
        laneIds.push_back(*laneId);
        assignments.push_back({*laneId, *value, {}});
    }
    std::optional<std::string> clockId;
    if (!resolveClockContext(
            project, scenario, operation, laneIds, clockId, error)) {
        return false;
    }
    const auto start = automationTickField(
        project,
        operation,
        QStringLiteral("startTick"),
        QStringLiteral("start"),
        clockId,
        error);
    if (!start) return false;
    const auto end = automationTickField(
        project,
        operation,
        QStringLiteral("endTick"),
        QStringLiteral("end"),
        clockId,
        error);
    if (!end) return false;

    QJsonArray assignmentReport;
    for (const auto& assignment : assignments) {
        assignmentReport.append(QJsonObject{
            {QStringLiteral("laneId"),
             QString::fromStdString(assignment.laneId)},
            {QStringLiteral("value"),
             QString::fromStdString(assignment.value)},
        });
    }
    const auto changed = stack.execute(std::make_unique<SetLaneRangesCommand>(
        scenario,
        *start,
        *end,
        std::move(assignments)));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(QStringLiteral("laneIds"), stringArray(laneIds));
    result.insert(QStringLiteral("startTick"), integerValue(*start));
    result.insert(QStringLiteral("endTick"), integerValue(*end));
    result.insert(QStringLiteral("assignments"), assignmentReport);
    if (assignmentReport.size() == 1) {
        result.insert(
            QStringLiteral("value"),
            assignmentReport.at(0)
                .toObject()
                .value(QStringLiteral("value")));
    }
    return true;
}

bool applySetSequence(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("laneId"),
             QStringLiteral("values"),
             QStringLiteral("sequences"),
             QStringLiteral("startTick"),
             QStringLiteral("start"),
             QStringLiteral("stepTick"),
             QStringLiteral("step"),
             QStringLiteral("stepCycles"),
             QStringLiteral("clockId"),
             QStringLiteral("repeat")},
            error)) {
        return false;
    }

    struct LaneSequence {
        std::string laneId;
        std::vector<std::string> values;
    };
    std::vector<LaneSequence> sequences;
    const auto hasSingle =
        operation.contains(QStringLiteral("laneId"))
        || operation.contains(QStringLiteral("values"));
    const auto hasMultiple = operation.contains(QStringLiteral("sequences"));
    if (hasSingle == hasMultiple) {
        error = QStringLiteral(
            "Use either 'laneId' with 'values' or 'sequences'.");
        return false;
    }
    const auto appendSequence =
        [&scenario, &sequences, &error](
            const QJsonObject& object) -> bool {
        if (!containsOnly(
                object,
                {QStringLiteral("laneId"), QStringLiteral("values")},
                error)) {
            return false;
        }
        const auto laneId = requiredString(
            object, QStringLiteral("laneId"), error);
        if (!laneId) return false;
        if (std::any_of(
                sequences.begin(),
                sequences.end(),
                [&laneId](const LaneSequence& sequence) {
                    return sequence.laneId == *laneId;
                })) {
            error = QStringLiteral(
                "Sequence contains a duplicate Lane ID.");
            return false;
        }
        const auto* lane = findLane(scenario, *laneId);
        if (!lane || lane->kind == LaneKind::Group) {
            error = QStringLiteral("Lane '%1' does not exist.")
                        .arg(QString::fromStdString(*laneId));
            return false;
        }
        const auto values = object.value(QStringLiteral("values"));
        if (!values.isArray() || values.toArray().isEmpty()) {
            error = QStringLiteral(
                "Sequence 'values' must be a non-empty array.");
            return false;
        }
        LaneSequence sequence;
        sequence.laneId = *laneId;
        sequence.values.reserve(
            static_cast<std::size_t>(values.toArray().size()));
        for (const auto& value : values.toArray()) {
            if (!value.isString()) {
                error = QStringLiteral(
                    "Every sequence value must be a string.");
                return false;
            }
            const auto validation = validateLaneValue(
                *lane, value.toString().toStdString());
            if (!validation.valid) {
                error = QStringLiteral(
                    "Sequence value for Lane '%1' is invalid: %2")
                            .arg(
                                QString::fromStdString(*laneId),
                                QString::fromStdString(validation.error));
                return false;
            }
            sequence.values.push_back(validation.normalizedValue);
        }
        sequences.push_back(std::move(sequence));
        return true;
    };

    if (hasMultiple) {
        const auto values = operation.value(QStringLiteral("sequences"));
        if (!values.isArray() || values.toArray().isEmpty()) {
            error = QStringLiteral(
                "Field 'sequences' must be a non-empty array.");
            return false;
        }
        for (const auto& value : values.toArray()) {
            if (!value.isObject() || !appendSequence(value.toObject())) {
                if (error.isEmpty()) {
                    error = QStringLiteral(
                        "Every sequences entry must be an object.");
                }
                return false;
            }
        }
    } else {
        QJsonObject single{
            {QStringLiteral("laneId"),
             operation.value(QStringLiteral("laneId"))},
            {QStringLiteral("values"),
             operation.value(QStringLiteral("values"))},
        };
        if (!appendSequence(single)) return false;
    }

    auto repeat = std::int64_t{1};
    if (operation.contains(QStringLiteral("repeat"))) {
        const auto parsed = integerField(
            operation, QStringLiteral("repeat"), true, error);
        if (!parsed) return false;
        repeat = *parsed;
    }
    if (repeat <= 0 || repeat > 1'000'000) {
        error = QStringLiteral("Field 'repeat' must be from 1 to 1000000.");
        return false;
    }

    std::vector<std::string> laneIds;
    laneIds.reserve(sequences.size());
    std::uint64_t cellCount = 0;
    std::uint64_t maximumSteps = 0;
    for (const auto& sequence : sequences) {
        laneIds.push_back(sequence.laneId);
        const auto valueCount =
            static_cast<std::uint64_t>(sequence.values.size());
        const auto repeatedCount =
            valueCount * static_cast<std::uint64_t>(repeat);
        if (repeatedCount > 1'000'000
            || cellCount > 1'000'000 - repeatedCount) {
            error = QStringLiteral(
                "Sequence expands beyond 1000000 value cells.");
            return false;
        }
        cellCount += repeatedCount;
        maximumSteps = std::max(maximumSteps, repeatedCount);
    }

    std::optional<std::string> clockId;
    if (!resolveClockContext(
            project, scenario, operation, laneIds, clockId, error)) {
        return false;
    }
    const auto start = automationTickField(
        project,
        operation,
        QStringLiteral("startTick"),
        QStringLiteral("start"),
        clockId,
        error);
    if (!start) return false;
    if (*start < 0) {
        error = QStringLiteral("Sequence start must not be negative.");
        return false;
    }

    const auto stepModeCount =
        (operation.contains(QStringLiteral("stepTick")) ? 1 : 0)
        + (operation.contains(QStringLiteral("step")) ? 1 : 0)
        + (operation.contains(QStringLiteral("stepCycles")) ? 1 : 0);
    if (stepModeCount != 1) {
        error = QStringLiteral(
            "Use exactly one of 'stepTick', 'step', or 'stepCycles'.");
        return false;
    }
    Tick step = 0;
    if (operation.contains(QStringLiteral("stepCycles"))) {
        const auto cycles = integerField(
            operation, QStringLiteral("stepCycles"), true, error);
        if (!cycles) return false;
        if (*cycles <= 0) {
            error = QStringLiteral("Field 'stepCycles' must be positive.");
            return false;
        }
        if (!clockId) {
            error = QStringLiteral(
                "stepCycles requires one unambiguous clock domain.");
            return false;
        }
        const auto* clock = findClock(project, *clockId);
        if (!clock || clock->period <= 0
            || *cycles > std::numeric_limits<Tick>::max() / clock->period) {
            error = QStringLiteral(
                "Sequence cycle step is outside the integer tick range.");
            return false;
        }
        step = clock->period * *cycles;
    } else {
        const auto parsed = automationTickField(
            project,
            operation,
            QStringLiteral("stepTick"),
            QStringLiteral("step"),
            clockId,
            error,
            false);
        if (!parsed) return false;
        step = *parsed;
    }
    if (step <= 0) {
        error = QStringLiteral("Sequence step must be positive.");
        return false;
    }
    if (maximumSteps
        > static_cast<std::uint64_t>(
            std::numeric_limits<Tick>::max() / step)) {
        error = QStringLiteral("Sequence duration overflows tick range.");
        return false;
    }
    const auto sequenceDuration =
        step * static_cast<Tick>(maximumSteps);
    if (*start > std::numeric_limits<Tick>::max() - sequenceDuration) {
        error = QStringLiteral("Sequence end overflows tick range.");
        return false;
    }
    const auto end = *start + sequenceDuration;

    const auto beforeDuration = scenario.duration;
    auto changed = false;
    if (end > scenario.duration) {
        changed = stack.execute(
            std::make_unique<ChangeScenarioDurationCommand>(scenario, end))
            || changed;
    }
    for (std::uint64_t index = 0; index < maximumSteps; ++index) {
        std::vector<LaneRangeAssignment> assignments;
        for (const auto& sequence : sequences) {
            const auto expandedCount =
                static_cast<std::uint64_t>(sequence.values.size())
                * static_cast<std::uint64_t>(repeat);
            if (index >= expandedCount) continue;
            const auto valueIndex = static_cast<std::size_t>(
                index % sequence.values.size());
            assignments.push_back({
                sequence.laneId,
                sequence.values.at(valueIndex),
                {},
            });
        }
        if (assignments.empty()) continue;
        const auto beatStart = *start + step * static_cast<Tick>(index);
        const auto beatEnd = beatStart + step;
        changed = stack.execute(std::make_unique<SetLaneRangesCommand>(
                      scenario,
                      beatStart,
                      beatEnd,
                      std::move(assignments)))
            || changed;
    }

    result.insert(QStringLiteral("changed"), changed);
    result.insert(QStringLiteral("laneIds"), stringArray(laneIds));
    result.insert(QStringLiteral("startTick"), integerValue(*start));
    result.insert(QStringLiteral("stepTick"), integerValue(step));
    result.insert(QStringLiteral("endTick"), integerValue(end));
    result.insert(QStringLiteral("repeat"), integerValue(repeat));
    result.insert(QStringLiteral("valueCellCount"), unsignedIntegerValue(cellCount));
    result.insert(QStringLiteral("beforeDurationTick"), integerValue(beforeDuration));
    result.insert(QStringLiteral("durationAfterTick"), integerValue(scenario.duration));
    return true;
}

bool applyClearRange(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("laneId"),
             QStringLiteral("laneIds"),
             QStringLiteral("startTick"),
             QStringLiteral("start"),
             QStringLiteral("endTick"),
             QStringLiteral("end"),
             QStringLiteral("clockId")},
            error)) {
        return false;
    }
    std::vector<std::string> laneIds;
    if (operation.contains(QStringLiteral("laneId"))) {
        const auto laneId = requiredString(
            operation, QStringLiteral("laneId"), error);
        if (!laneId) return false;
        laneIds.push_back(*laneId);
    }
    if (operation.contains(QStringLiteral("laneIds"))) {
        if (!laneIds.empty()) {
            error = QStringLiteral("Use either 'laneId' or 'laneIds', not both.");
            return false;
        }
        const auto values = operation.value(QStringLiteral("laneIds"));
        if (!values.isArray() || values.toArray().isEmpty()) {
            error = QStringLiteral("Field 'laneIds' must be a non-empty array.");
            return false;
        }
        for (const auto& value : values.toArray()) {
            if (!value.isString() || value.toString().trimmed().isEmpty()) {
                error = QStringLiteral(
                    "Every laneIds entry must be a non-empty string.");
                return false;
            }
            laneIds.push_back(value.toString().toStdString());
        }
    }
    if (laneIds.empty()) {
        error = QStringLiteral("Missing field 'laneId' or 'laneIds'.");
        return false;
    }
    std::optional<std::string> clockId;
    if (!resolveClockContext(
            project, scenario, operation, laneIds, clockId, error)) {
        return false;
    }
    const auto start = automationTickField(
        project,
        operation,
        QStringLiteral("startTick"),
        QStringLiteral("start"),
        clockId,
        error);
    if (!start) return false;
    const auto end = automationTickField(
        project,
        operation,
        QStringLiteral("endTick"),
        QStringLiteral("end"),
        clockId,
        error);
    if (!end) return false;
    const auto changed = stack.execute(std::make_unique<ClearLaneRangesCommand>(
        scenario,
        *start,
        *end,
        laneIds));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(QStringLiteral("laneIds"), stringArray(laneIds));
    result.insert(QStringLiteral("startTick"), integerValue(*start));
    result.insert(QStringLiteral("endTick"), integerValue(*end));
    return true;
}

bool applyTransferRange(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("mode"),
             QStringLiteral("laneId"),
             QStringLiteral("laneIds"),
             QStringLiteral("mappings"),
             QStringLiteral("sourceStartTick"),
             QStringLiteral("sourceStart"),
             QStringLiteral("sourceEndTick"),
             QStringLiteral("sourceEnd"),
             QStringLiteral("destinationTick"),
             QStringLiteral("destination"),
             QStringLiteral("clockId"),
             QStringLiteral("overwrite")},
            error)) {
        return false;
    }
    const auto modeText = requiredString(
        operation, QStringLiteral("mode"), error);
    if (!modeText) return false;
    RangeTransferMode mode;
    if (*modeText == "copy") {
        mode = RangeTransferMode::Copy;
    } else if (*modeText == "move") {
        mode = RangeTransferMode::Move;
    } else {
        error = QStringLiteral("Field 'mode' must be 'copy' or 'move'.");
        return false;
    }

    struct LaneMapping {
        std::string source;
        std::string target;
    };
    std::vector<LaneMapping> mappings;
    const auto laneModeCount =
        (operation.contains(QStringLiteral("laneId")) ? 1 : 0)
        + (operation.contains(QStringLiteral("laneIds")) ? 1 : 0)
        + (operation.contains(QStringLiteral("mappings")) ? 1 : 0);
    if (laneModeCount != 1) {
        error = QStringLiteral(
            "Use exactly one of 'laneId', 'laneIds', or 'mappings'.");
        return false;
    }
    if (operation.contains(QStringLiteral("laneId"))) {
        const auto laneId = requiredString(
            operation, QStringLiteral("laneId"), error);
        if (!laneId) return false;
        mappings.push_back({*laneId, *laneId});
    } else if (operation.contains(QStringLiteral("laneIds"))) {
        const auto values = operation.value(QStringLiteral("laneIds"));
        if (!values.isArray() || values.toArray().isEmpty()) {
            error = QStringLiteral("Field 'laneIds' must be a non-empty array.");
            return false;
        }
        for (const auto& value : values.toArray()) {
            if (!value.isString() || value.toString().trimmed().isEmpty()) {
                error = QStringLiteral(
                    "Every laneIds entry must be a non-empty string.");
                return false;
            }
            const auto laneId = value.toString().trimmed().toStdString();
            mappings.push_back({laneId, laneId});
        }
    } else {
        const auto values = operation.value(QStringLiteral("mappings"));
        if (!values.isArray() || values.toArray().isEmpty()) {
            error = QStringLiteral(
                "Field 'mappings' must be a non-empty array.");
            return false;
        }
        for (const auto& value : values.toArray()) {
            if (!value.isObject()) {
                error = QStringLiteral(
                    "Every mappings entry must be an object.");
                return false;
            }
            const auto object = value.toObject();
            if (!containsOnly(
                    object,
                    {QStringLiteral("sourceLaneId"),
                     QStringLiteral("targetLaneId")},
                    error)) {
                return false;
            }
            const auto source = requiredString(
                object, QStringLiteral("sourceLaneId"), error);
            const auto target = requiredString(
                object, QStringLiteral("targetLaneId"), error);
            if (!source || !target) return false;
            mappings.push_back({*source, *target});
        }
    }

    std::set<std::string> sourceIds;
    std::set<std::string> targetIds;
    std::vector<std::string> clockLaneIds;
    clockLaneIds.reserve(mappings.size() * 2);
    for (const auto& mapping : mappings) {
        if (!sourceIds.insert(mapping.source).second
            || !targetIds.insert(mapping.target).second) {
            error = QStringLiteral(
                "Range transfer source and target Lane IDs must each be unique.");
            return false;
        }
        const auto* sourceLane = findLane(scenario, mapping.source);
        const auto* targetLane = findLane(scenario, mapping.target);
        if (!sourceLane || !targetLane
            || sourceLane->kind == LaneKind::Group
            || targetLane->kind == LaneKind::Group) {
            error = QStringLiteral(
                "Range transfer references a missing or Group lane.");
            return false;
        }
        if (sourceLane->kind != targetLane->kind
            || ((sourceLane->kind == LaneKind::Bus
                 || sourceLane->kind == LaneKind::Enum)
                && sourceLane->width != targetLane->width)) {
            error = QStringLiteral(
                "Range transfer source and target lanes are incompatible.");
            return false;
        }
        clockLaneIds.push_back(mapping.source);
        if (mapping.target != mapping.source) {
            clockLaneIds.push_back(mapping.target);
        }
    }

    auto overwrite = false;
    if (operation.contains(QStringLiteral("overwrite"))) {
        const auto value = operation.value(QStringLiteral("overwrite"));
        if (!value.isBool()) {
            error = QStringLiteral("Field 'overwrite' must be boolean.");
            return false;
        }
        overwrite = value.toBool();
    }

    std::optional<std::string> clockId;
    if (!resolveClockContext(
            project,
            scenario,
            operation,
            clockLaneIds,
            clockId,
            error)) {
        return false;
    }
    const auto sourceStart = automationTickField(
        project,
        operation,
        QStringLiteral("sourceStartTick"),
        QStringLiteral("sourceStart"),
        clockId,
        error);
    if (!sourceStart) return false;
    const auto sourceEnd = automationTickField(
        project,
        operation,
        QStringLiteral("sourceEndTick"),
        QStringLiteral("sourceEnd"),
        clockId,
        error);
    if (!sourceEnd) return false;
    const auto destination = automationTickField(
        project,
        operation,
        QStringLiteral("destinationTick"),
        QStringLiteral("destination"),
        clockId,
        error);
    if (!destination) return false;
    if (*sourceStart < 0
        || *sourceEnd <= *sourceStart
        || *sourceEnd > scenario.duration
        || *destination < 0
        || *destination > scenario.duration) {
        error = QStringLiteral(
            "Range transfer source or destination is outside the Scenario.");
        return false;
    }
    const auto duration = *sourceEnd - *sourceStart;
    if (duration > std::numeric_limits<Tick>::max() - *destination) {
        error = QStringLiteral("Range transfer destination overflows tick range.");
        return false;
    }
    const auto destinationEnd = *destination + duration;

    auto overwroteExplicit = false;
    for (const auto& mapping : mappings) {
        const auto* targetLane = findLane(scenario, mapping.target);
        if (!targetLane) {
            error = QStringLiteral("Range transfer target Lane disappeared.");
            return false;
        }
        for (const auto& segment : targetLane->segments) {
            const auto overlapStart = std::max(segment.start, *destination);
            const auto overlapEnd = std::min(segment.end, destinationEnd);
            if (overlapEnd <= overlapStart) continue;
            const auto clearedBySameLaneMove =
                mode == RangeTransferMode::Move
                && mapping.source == mapping.target
                && overlapStart >= *sourceStart
                && overlapEnd <= *sourceEnd;
            if (clearedBySameLaneMove) continue;
            overwroteExplicit = true;
            if (!overwrite) {
                error = QStringLiteral(
                    "Range transfer would replace explicit target content on Lane '%1'; set 'overwrite' to true to confirm.")
                            .arg(QString::fromStdString(mapping.target));
                return false;
            }
        }
    }

    std::vector<CopiedLaneRange> copied;
    copied.reserve(mappings.size());
    QJsonArray mappingReport;
    for (const auto& mapping : mappings) {
        const auto* sourceLane = findLane(scenario, mapping.source);
        if (!sourceLane) {
            error = QStringLiteral("Range transfer source Lane disappeared.");
            return false;
        }
        CopiedLaneRange lane;
        lane.laneId = mapping.target;
        lane.sourceLaneId = mapping.source;
        auto iterator = std::lower_bound(
            sourceLane->segments.begin(),
            sourceLane->segments.end(),
            *sourceStart,
            [](const Segment& segment, const Tick tick) {
                return segment.end <= tick;
            });
        for (;
             iterator != sourceLane->segments.end()
             && iterator->start < *sourceEnd;
             ++iterator) {
            const auto clippedStart =
                std::max(*sourceStart, iterator->start);
            const auto clippedEnd =
                std::min(*sourceEnd, iterator->end);
            if (clippedEnd <= clippedStart) continue;
            auto relative = *iterator;
            const auto preserveIdentity =
                mode == RangeTransferMode::Move
                && mapping.source == mapping.target
                && iterator->start >= *sourceStart
                && iterator->end <= *sourceEnd;
            if (!preserveIdentity) relative.id.clear();
            relative.start = clippedStart - *sourceStart;
            relative.end = clippedEnd - *sourceStart;
            lane.relativeSegments.push_back(std::move(relative));
        }
        copied.push_back(std::move(lane));
        mappingReport.append(QJsonObject{
            {QStringLiteral("sourceLaneId"),
             QString::fromStdString(mapping.source)},
            {QStringLiteral("targetLaneId"),
             QString::fromStdString(mapping.target)},
        });
    }

    const auto beforeDuration = scenario.duration;
    const auto changed = stack.execute(std::make_unique<TransferRangeCommand>(
        scenario,
        std::move(copied),
        *sourceStart,
        *destination,
        duration,
        mode));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("mode"),
        mode == RangeTransferMode::Copy
            ? QStringLiteral("copy")
            : QStringLiteral("move"));
    result.insert(QStringLiteral("sourceStartTick"), integerValue(*sourceStart));
    result.insert(QStringLiteral("sourceEndTick"), integerValue(*sourceEnd));
    result.insert(QStringLiteral("destinationTick"), integerValue(*destination));
    result.insert(QStringLiteral("destinationEndTick"), integerValue(destinationEnd));
    result.insert(QStringLiteral("durationTick"), integerValue(duration));
    result.insert(QStringLiteral("overwrite"), overwrite);
    result.insert(QStringLiteral("overwroteExplicit"), overwroteExplicit);
    result.insert(QStringLiteral("mappings"), mappingReport);
    result.insert(QStringLiteral("beforeDurationTick"), integerValue(beforeDuration));
    result.insert(QStringLiteral("durationAfterTick"), integerValue(scenario.duration));
    return true;
}

bool applyRenameLane(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("laneId"),
             QStringLiteral("name")},
            error)) {
        return false;
    }
    const auto laneId = requiredString(operation, QStringLiteral("laneId"), error);
    if (!laneId) return false;
    const auto name = requiredString(operation, QStringLiteral("name"), error);
    if (!name) return false;
    const auto* lane = findLane(scenario, *laneId);
    if (!lane) {
        error = QStringLiteral("Lane '%1' does not exist.")
                    .arg(QString::fromStdString(*laneId));
        return false;
    }
    if (!uniqueLaneName(scenario, *name, *laneId)) {
        error = QStringLiteral("Lane name '%1' already exists.")
                    .arg(QString::fromStdString(*name));
        return false;
    }
    auto replacement = *lane;
    replacement.name = *name;
    const auto changed = stack.execute(std::make_unique<ChangeLaneCommand>(
        project,
        scenario,
        *laneId,
        std::move(replacement)));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(QStringLiteral("laneId"), QString::fromStdString(*laneId));
    result.insert(QStringLiteral("name"), QString::fromStdString(*name));
    return true;
}

bool applySetDuration(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("durationTick"),
             QStringLiteral("duration"),
             QStringLiteral("clockId"),
             QStringLiteral("truncate")},
            error)) {
        return false;
    }
    auto truncate = false;
    if (operation.contains(QStringLiteral("truncate"))) {
        const auto value = operation.value(QStringLiteral("truncate"));
        if (!value.isBool()) {
            error = QStringLiteral("Field 'truncate' must be boolean.");
            return false;
        }
        truncate = value.toBool();
    }
    std::optional<std::string> clockId;
    if (!resolveClockContext(
            project, scenario, operation, {}, clockId, error)) {
        return false;
    }
    const auto duration = automationTickField(
        project,
        operation,
        QStringLiteral("durationTick"),
        QStringLiteral("duration"),
        clockId,
        error);
    if (!duration) return false;
    if (*duration <= 0) {
        error = QStringLiteral("Scenario duration must be positive.");
        return false;
    }
    const auto before = scenario.duration;
    auto changed = false;
    auto contentTruncated = false;
    ScenarioTruncationSummary truncation;
    if (*duration < before) {
        auto truncateCommand =
            std::make_unique<TruncateScenarioDurationCommand>(
                scenario, *duration);
        truncation = truncateCommand->summary();
        contentTruncated = truncation.changesContent();
        if (contentTruncated && !truncate) {
            error = QStringLiteral(
                "Shortening End to %1 would clip %2 Segment(s) and %3 Marker(s), "
                "and remove %4 Segment(s), %5 Event(s), %6 Relation(s), and "
                "%7 Marker(s); set 'truncate' to true to confirm content loss.")
                        .arg(*duration)
                        .arg(truncation.clippedSegmentCount)
                        .arg(truncation.clippedMarkerCount)
                        .arg(truncation.removedSegmentCount)
                        .arg(truncation.removedEventCount)
                        .arg(truncation.removedRelationCount)
                        .arg(truncation.removedMarkerCount);
            return false;
        }
        changed = truncate
            ? stack.execute(std::move(truncateCommand))
            : stack.execute(
                  std::make_unique<ChangeScenarioDurationCommand>(
                      scenario, *duration));
    } else if (*duration > before) {
        changed = stack.execute(
            std::make_unique<ChangeScenarioDurationCommand>(
                scenario, *duration));
    }
    result.insert(QStringLiteral("changed"), changed);
    result.insert(QStringLiteral("beforeTick"), integerValue(before));
    result.insert(QStringLiteral("durationTick"), integerValue(*duration));
    result.insert(
        QStringLiteral("direction"),
        *duration < before
            ? QStringLiteral("shrink")
            : (*duration > before
                   ? QStringLiteral("extend")
                   : QStringLiteral("unchanged")));
    result.insert(QStringLiteral("truncateRequested"), truncate);
    result.insert(QStringLiteral("contentTruncated"), contentTruncated);
    result.insert(
        QStringLiteral("clippedSegmentCount"),
        static_cast<qint64>(truncation.clippedSegmentCount));
    result.insert(
        QStringLiteral("removedSegmentCount"),
        static_cast<qint64>(truncation.removedSegmentCount));
    result.insert(
        QStringLiteral("removedEventCount"),
        static_cast<qint64>(truncation.removedEventCount));
    result.insert(
        QStringLiteral("removedRelationCount"),
        static_cast<qint64>(truncation.removedRelationCount));
    result.insert(
        QStringLiteral("clippedMarkerCount"),
        static_cast<qint64>(truncation.clippedMarkerCount));
    result.insert(
        QStringLiteral("removedMarkerCount"),
        static_cast<qint64>(truncation.removedMarkerCount));
    return true;
}

bool applyDeleteSignal(
    Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"), QStringLiteral("laneId")},
            error)) {
        return false;
    }
    const auto laneId = requiredString(operation, QStringLiteral("laneId"), error);
    if (!laneId) return false;
    const auto* lane = findLane(scenario, *laneId);
    if (!lane) {
        error = QStringLiteral("Lane '%1' does not exist.")
                    .arg(QString::fromStdString(*laneId));
        return false;
    }
    if (lane->kind == LaneKind::Group) {
        error = QStringLiteral(
            "delete-signal does not remove Group lanes.");
        return false;
    }
    std::set<std::string> eventIds;
    for (const auto& event : scenario.events) {
        if (event.laneId == *laneId) eventIds.insert(event.id);
    }
    const auto relationCount = std::count_if(
        scenario.relations.begin(),
        scenario.relations.end(),
        [&eventIds](const Relation& relation) {
            return eventIds.contains(relation.sourceEventId)
                || eventIds.contains(relation.targetEventId);
        });
    std::size_t mappingCount = 0;
    for (const auto& trace : project.importedTraces) {
        if (trace.signalMapping.contains(*laneId)) ++mappingCount;
    }
    const auto laneName = lane->name;
    const auto laneKind = lane->kind;
    const auto changed = stack.execute(std::make_unique<RemoveLaneCommand>(
        project, scenario, *laneId));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(QStringLiteral("laneId"), QString::fromStdString(*laneId));
    result.insert(QStringLiteral("name"), QString::fromStdString(laneName));
    result.insert(
        QStringLiteral("kind"),
        QString::fromLatin1(toString(laneKind).data()));
    result.insert(
        QStringLiteral("removedEvents"),
        static_cast<qint64>(eventIds.size()));
    result.insert(
        QStringLiteral("removedRelations"),
        static_cast<qint64>(relationCount));
    result.insert(
        QStringLiteral("removedTraceMappings"),
        static_cast<qint64>(mappingCount));
    return true;
}

bool applyMoveSignal(
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("laneId"),
             QStringLiteral("destinationIndex"),
             QStringLiteral("beforeLaneId"),
             QStringLiteral("afterLaneId")},
            error)) {
        return false;
    }
    const auto laneId = requiredString(operation, QStringLiteral("laneId"), error);
    if (!laneId) return false;
    const auto source = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [&laneId](const Lane& lane) {
            return lane.id == *laneId;
        });
    if (source == scenario.lanes.end()) {
        error = QStringLiteral("Lane '%1' does not exist.")
                    .arg(QString::fromStdString(*laneId));
        return false;
    }
    if (source->kind == LaneKind::Group) {
        error = QStringLiteral("move-signal does not move Group lanes.");
        return false;
    }
    const auto modeCount =
        (operation.contains(QStringLiteral("destinationIndex")) ? 1 : 0)
        + (operation.contains(QStringLiteral("beforeLaneId")) ? 1 : 0)
        + (operation.contains(QStringLiteral("afterLaneId")) ? 1 : 0);
    if (modeCount != 1) {
        error = QStringLiteral(
            "Use exactly one of 'destinationIndex', 'beforeLaneId', or 'afterLaneId'.");
        return false;
    }
    const auto sourceIndex = static_cast<std::size_t>(
        std::distance(scenario.lanes.begin(), source));
    std::size_t destinationIndex = sourceIndex;
    if (operation.contains(QStringLiteral("destinationIndex"))) {
        const auto index = integerField(
            operation, QStringLiteral("destinationIndex"), true, error);
        if (!index) return false;
        if (*index < 0
            || static_cast<std::uint64_t>(*index) >= scenario.lanes.size()) {
            error = QStringLiteral(
                "Field 'destinationIndex' is outside the lane list.");
            return false;
        }
        destinationIndex = static_cast<std::size_t>(*index);
    } else {
        const auto field = operation.contains(QStringLiteral("beforeLaneId"))
            ? QStringLiteral("beforeLaneId")
            : QStringLiteral("afterLaneId");
        const auto targetId = requiredString(operation, field, error);
        if (!targetId) return false;
        if (*targetId == *laneId) {
            error = QStringLiteral("A lane cannot be moved relative to itself.");
            return false;
        }
        const auto target = std::find_if(
            scenario.lanes.begin(),
            scenario.lanes.end(),
            [&targetId](const Lane& lane) {
                return lane.id == *targetId;
            });
        if (target == scenario.lanes.end()) {
            error = QStringLiteral("Reference lane '%1' does not exist.")
                        .arg(QString::fromStdString(*targetId));
            return false;
        }
        const auto targetIndex = static_cast<std::size_t>(
            std::distance(scenario.lanes.begin(), target));
        const auto reducedTarget =
            targetIndex - (sourceIndex < targetIndex ? 1U : 0U);
        destinationIndex =
            reducedTarget
            + (field == QStringLiteral("afterLaneId") ? 1U : 0U);
    }
    const auto changed = destinationIndex == sourceIndex
        ? false
        : stack.execute(std::make_unique<MoveLaneCommand>(
              scenario, *laneId, destinationIndex));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(QStringLiteral("laneId"), QString::fromStdString(*laneId));
    result.insert(
        QStringLiteral("beforeIndex"),
        static_cast<qint64>(sourceIndex));
    result.insert(
        QStringLiteral("destinationIndex"),
        static_cast<qint64>(destinationIndex));
    return true;
}

bool applyUpdateSignal(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("laneId"),
             QStringLiteral("name"),
             QStringLiteral("width"),
             QStringLiteral("signed"),
             QStringLiteral("radix"),
             QStringLiteral("enumMap"),
             QStringLiteral("clockDomainId"),
             QStringLiteral("color"),
             QStringLiteral("height"),
             QStringLiteral("visible"),
             QStringLiteral("groupId")},
            error)) {
        return false;
    }
    if (operation.size() <= 2) {
        error = QStringLiteral("update-signal contains no property change.");
        return false;
    }
    const auto laneId = requiredString(operation, QStringLiteral("laneId"), error);
    if (!laneId) return false;
    const auto* lane = findLane(scenario, *laneId);
    if (!lane) {
        error = QStringLiteral("Lane '%1' does not exist.")
                    .arg(QString::fromStdString(*laneId));
        return false;
    }
    if (lane->kind == LaneKind::Group) {
        error = QStringLiteral("update-signal does not edit Group lanes.");
        return false;
    }
    auto replacement = *lane;
    if (operation.contains(QStringLiteral("name"))) {
        const auto name = requiredString(
            operation, QStringLiteral("name"), error);
        if (!name) return false;
        if (!uniqueLaneName(scenario, *name, *laneId)) {
            error = QStringLiteral("Lane name '%1' already exists.")
                        .arg(QString::fromStdString(*name));
            return false;
        }
        replacement.name = *name;
    }
    const auto busLike =
        lane->kind == LaneKind::Bus || lane->kind == LaneKind::Enum;
    if (operation.contains(QStringLiteral("width"))) {
        const auto width = integerField(
            operation, QStringLiteral("width"), true, error);
        if (!width) return false;
        if (!busLike || *width < 1 || *width > 65'536) {
            error = QStringLiteral(
                "Field 'width' is available only for Bus/Enum and must be from 1 to 65536.");
            return false;
        }
        replacement.width = static_cast<std::uint32_t>(*width);
    }
    if (operation.contains(QStringLiteral("signed"))) {
        if (!busLike
            || !operation.value(QStringLiteral("signed")).isBool()) {
            error = QStringLiteral(
                "Field 'signed' is available only for Bus/Enum and must be boolean.");
            return false;
        }
        replacement.isSigned =
            operation.value(QStringLiteral("signed")).toBool();
    }
    if (operation.contains(QStringLiteral("radix"))) {
        const auto value = operation.value(QStringLiteral("radix"));
        if (!busLike || !value.isString()) {
            error = QStringLiteral(
                "Field 'radix' is available only for Bus/Enum.");
            return false;
        }
        const auto radix = radixFromString(value.toString().toStdString());
        if (!radix) {
            error = QStringLiteral("Unknown radix.");
            return false;
        }
        replacement.radix = *radix;
    }
    if (operation.contains(QStringLiteral("enumMap"))) {
        if (lane->kind != LaneKind::Enum) {
            error = QStringLiteral(
                "Field 'enumMap' is available only for Enum.");
            return false;
        }
        if (!enumMapField(
                operation,
                QStringLiteral("enumMap"),
                replacement.width,
                replacement.isSigned,
                replacement.enumMap,
                error)) {
            return false;
        }
    }
    if (operation.contains(QStringLiteral("clockDomainId"))) {
        const auto value = operation.value(QStringLiteral("clockDomainId"));
        if (!value.isString() || lane->kind == LaneKind::Clock) {
            error = QStringLiteral(
                "Field 'clockDomainId' must be a string and cannot retarget a Clock lane.");
            return false;
        }
        replacement.clockDomainId =
            value.toString().trimmed().toStdString();
    }
    if (operation.contains(QStringLiteral("color"))) {
        const auto value = operation.value(QStringLiteral("color"));
        static const QRegularExpression colorExpression(
            QStringLiteral(R"(^#[0-9a-fA-F]{6}$)"));
        if (!value.isString()
            || !colorExpression.match(value.toString()).hasMatch()) {
            error = QStringLiteral("Field 'color' must use #RRGGBB.");
            return false;
        }
        replacement.color = value.toString().toLower().toStdString();
    }
    if (operation.contains(QStringLiteral("height"))) {
        const auto height = integerField(
            operation, QStringLiteral("height"), true, error);
        if (!height || *height < 30 || *height > 240) {
            if (error.isEmpty()) {
                error = QStringLiteral(
                    "Field 'height' must be from 30 to 240.");
            }
            return false;
        }
        replacement.height = static_cast<int>(*height);
    }
    if (operation.contains(QStringLiteral("visible"))) {
        const auto value = operation.value(QStringLiteral("visible"));
        if (!value.isBool()) {
            error = QStringLiteral("Field 'visible' must be boolean.");
            return false;
        }
        replacement.visible = value.toBool();
    }
    if (operation.contains(QStringLiteral("groupId"))) {
        const auto value = operation.value(QStringLiteral("groupId"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'groupId' must be a string.");
            return false;
        }
        replacement.groupId = value.toString().trimmed().toStdString();
    }
    const auto changed = stack.execute(std::make_unique<ChangeLaneCommand>(
        project, scenario, *laneId, std::move(replacement)));
    const auto* updated = findLane(scenario, *laneId);
    result.insert(QStringLiteral("changed"), changed);
    result.insert(QStringLiteral("laneId"), QString::fromStdString(*laneId));
    if (updated) {
        result.insert(
            QStringLiteral("name"),
            QString::fromStdString(updated->name));
        result.insert(
            QStringLiteral("kind"),
            QString::fromLatin1(toString(updated->kind).data()));
        result.insert(
            QStringLiteral("width"),
            static_cast<qint64>(updated->width));
        if (updated->kind == LaneKind::Enum) {
            result.insert(
                QStringLiteral("enumMap"),
                enumMapObject(updated->enumMap));
            result.insert(
                QStringLiteral("enumSymbolCount"),
                static_cast<qint64>(updated->enumMap.size()));
        }
    }
    return true;
}

bool applyUpdateClock(
    Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("clockId"),
             QStringLiteral("name"),
             QStringLiteral("periodTick"),
             QStringLiteral("period"),
             QStringLiteral("phaseTick"),
             QStringLiteral("phase"),
             QStringLiteral("dutyNumerator"),
             QStringLiteral("dutyDenominator"),
             QStringLiteral("activeEdge"),
             QStringLiteral("resetRelation")},
            error)) {
        return false;
    }
    if (operation.size() <= 2) {
        error = QStringLiteral("update-clock contains no property change.");
        return false;
    }
    const auto clockId = requiredString(
        operation, QStringLiteral("clockId"), error);
    if (!clockId) return false;
    const auto* clock = findClock(project, *clockId);
    if (!clock) {
        error = QStringLiteral("Clock domain '%1' does not exist.")
                    .arg(QString::fromStdString(*clockId));
        return false;
    }
    auto replacement = *clock;
    if (operation.contains(QStringLiteral("name"))) {
        const auto name = requiredString(
            operation, QStringLiteral("name"), error);
        if (!name) return false;
        replacement.name = *name;
    }
    if (operation.contains(QStringLiteral("periodTick"))
        || operation.contains(QStringLiteral("period"))) {
        const auto period = automationTickField(
            project,
            operation,
            QStringLiteral("periodTick"),
            QStringLiteral("period"),
            std::nullopt,
            error,
            false);
        if (!period) return false;
        replacement.period = *period;
    }
    if (operation.contains(QStringLiteral("phaseTick"))
        || operation.contains(QStringLiteral("phase"))) {
        const auto phase = automationTickField(
            project,
            operation,
            QStringLiteral("phaseTick"),
            QStringLiteral("phase"),
            std::nullopt,
            error,
            false);
        if (!phase) return false;
        replacement.phase = *phase;
    }
    if (operation.contains(QStringLiteral("dutyNumerator"))) {
        const auto value = integerField(
            operation, QStringLiteral("dutyNumerator"), true, error);
        if (!value) return false;
        replacement.dutyCycle.numerator = *value;
    }
    if (operation.contains(QStringLiteral("dutyDenominator"))) {
        const auto value = integerField(
            operation, QStringLiteral("dutyDenominator"), true, error);
        if (!value) return false;
        replacement.dutyCycle.denominator = *value;
    }
    if (operation.contains(QStringLiteral("activeEdge"))) {
        const auto value = operation.value(QStringLiteral("activeEdge"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'activeEdge' must be a string.");
            return false;
        }
        const auto edge = clockEdgeFromString(value.toString().toStdString());
        if (!edge) {
            error = QStringLiteral(
                "Field 'activeEdge' must be 'rising' or 'falling'.");
            return false;
        }
        replacement.activeEdge = *edge;
    }
    if (operation.contains(QStringLiteral("resetRelation"))) {
        const auto value = operation.value(QStringLiteral("resetRelation"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'resetRelation' must be a string.");
            return false;
        }
        replacement.resetRelation = value.toString().toStdString();
    }
    const auto changed = stack.execute(std::make_unique<ChangeClockCommand>(
        project, scenario, *clockId, std::move(replacement)));
    const auto* updated = findClock(project, *clockId);
    result.insert(QStringLiteral("changed"), changed);
    result.insert(QStringLiteral("clockId"), QString::fromStdString(*clockId));
    if (updated) {
        result.insert(
            QStringLiteral("periodTick"),
            integerValue(updated->period));
        result.insert(
            QStringLiteral("phaseTick"),
            integerValue(updated->phase));
        result.insert(
            QStringLiteral("activeEdge"),
            QString::fromLatin1(toString(updated->activeEdge).data()));
    }
    return true;
}

bool applyAssertValue(
    const Project& project,
    const Scenario& scenario,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("laneId"),
             QStringLiteral("atTick"),
             QStringLiteral("at"),
             QStringLiteral("clockId"),
             QStringLiteral("value")},
            error)) {
        return false;
    }
    const auto laneId = requiredString(
        operation, QStringLiteral("laneId"), error);
    const auto expectedInput = requiredString(
        operation, QStringLiteral("value"), error);
    if (!laneId || !expectedInput) return false;
    const auto* lane = findLane(scenario, *laneId);
    if (!lane || lane->kind == LaneKind::Group) {
        error = QStringLiteral("Lane '%1' does not exist.")
                    .arg(QString::fromStdString(*laneId));
        return false;
    }

    std::optional<std::string> clockId;
    if (!resolveClockContext(
            project, scenario, operation, {*laneId}, clockId, error)) {
        return false;
    }
    const auto tick = automationTickField(
        project,
        operation,
        QStringLiteral("atTick"),
        QStringLiteral("at"),
        clockId,
        error);
    if (!tick) return false;
    if (*tick < 0 || *tick >= scenario.duration) {
        error = QStringLiteral(
            "Assertion time %1 is outside Scenario [0, %2).")
                    .arg(*tick)
                    .arg(scenario.duration);
        return false;
    }

    QString expected;
    if (lane->kind == LaneKind::Clock) {
        expected = QString::fromStdString(*expectedInput).trimmed().toUpper();
        if (expected != QStringLiteral("0")
            && expected != QStringLiteral("1")
            && expected != QStringLiteral("X")) {
            error = QStringLiteral(
                "Clock assertion value must be 0, 1, or X.");
            return false;
        }
    } else {
        const auto validation = validateLaneValue(*lane, *expectedInput);
        if (!validation.valid) {
            error = QStringLiteral("Expected value is invalid: %1")
                        .arg(QString::fromStdString(validation.error));
            return false;
        }
        expected = QString::fromStdString(validation.normalizedValue);
    }
    const auto actual =
        sampleObject(project, *lane, *tick).value(QStringLiteral("value"))
            .toString();
    if (actual != expected) {
        error = QStringLiteral(
            "Value assertion failed for Lane '%1' at %2: expected '%3', actual '%4'.")
                    .arg(
                        QString::fromStdString(*laneId),
                        QString::number(*tick),
                        expected,
                        actual);
        return false;
    }
    result.insert(QStringLiteral("changed"), false);
    result.insert(QStringLiteral("laneId"), QString::fromStdString(*laneId));
    result.insert(QStringLiteral("atTick"), integerValue(*tick));
    result.insert(QStringLiteral("expectedValue"), expected);
    result.insert(QStringLiteral("actualValue"), actual);
    return true;
}

bool applyAddSignal(
    Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("id"),
             QStringLiteral("name"),
             QStringLiteral("kind"),
             QStringLiteral("width"),
             QStringLiteral("signed"),
             QStringLiteral("radix"),
             QStringLiteral("enumMap"),
             QStringLiteral("clockDomainId"),
             QStringLiteral("groupId"),
             QStringLiteral("color"),
             QStringLiteral("insertionIndex"),
             QStringLiteral("clockId"),
             QStringLiteral("periodTick"),
             QStringLiteral("period"),
             QStringLiteral("phaseTick"),
             QStringLiteral("phase"),
             QStringLiteral("dutyNumerator"),
             QStringLiteral("dutyDenominator"),
             QStringLiteral("activeEdge")},
            error)) {
        return false;
    }
    const auto name = requiredString(operation, QStringLiteral("name"), error);
    if (!name) return false;
    if (!uniqueLaneName(scenario, *name)) {
        error = QStringLiteral("Lane name '%1' already exists.")
                    .arg(QString::fromStdString(*name));
        return false;
    }
    const auto kindText = requiredString(
        operation, QStringLiteral("kind"), error);
    if (!kindText) return false;
    const auto kind = laneKindFromString(*kindText);
    if (!kind
        || (*kind != LaneKind::Clock
            && *kind != LaneKind::Bit
            && *kind != LaneKind::Bus
            && *kind != LaneKind::Enum)) {
        error = QStringLiteral(
            "add-signal kind must be 'clock', 'bit', 'bus', or 'enum'.");
        return false;
    }

    Lane lane;
    lane.id = operation.value(QStringLiteral("id")).isUndefined()
        ? deterministicStableId(
              "lane",
              std::string(toString(*kind)) + ":" + *name,
              [&scenario](const std::string& candidate) {
                  return findLane(scenario, candidate) != nullptr;
              })
        : operation.value(QStringLiteral("id")).toString().trimmed().toStdString();
    if (lane.id.empty()) {
        error = QStringLiteral("Field 'id' must be a non-empty string.");
        return false;
    }
    if (findLane(scenario, lane.id)) {
        error = QStringLiteral("Lane ID '%1' already exists.")
                    .arg(QString::fromStdString(lane.id));
        return false;
    }
    lane.name = *name;
    lane.kind = *kind;
    const auto busLike =
        *kind == LaneKind::Bus || *kind == LaneKind::Enum;
    lane.width = *kind == LaneKind::Bus ? 8U : 1U;
    if (*kind == LaneKind::Enum
        && !operation.contains(QStringLiteral("width"))) {
        error = QStringLiteral(
            "Enum add-signal requires an explicit width.");
        return false;
    }
    if (operation.contains(QStringLiteral("width"))) {
        const auto width = integerField(
            operation, QStringLiteral("width"), true, error);
        if (!width) return false;
        if (!busLike || *width < 1 || *width > 65'536) {
            error = QStringLiteral(
                "Field 'width' is available only for Bus/Enum and must be from 1 to 65536.");
            return false;
        }
        lane.width = static_cast<std::uint32_t>(*width);
    }
    if (operation.contains(QStringLiteral("signed"))) {
        if (!busLike
            || !operation.value(QStringLiteral("signed")).isBool()) {
            error = QStringLiteral(
                "Field 'signed' is available only for Bus/Enum and must be boolean.");
            return false;
        }
        lane.isSigned = operation.value(QStringLiteral("signed")).toBool();
    }
    lane.radix = *kind == LaneKind::Bit ? Radix::Binary : Radix::Hexadecimal;
    if (operation.contains(QStringLiteral("radix"))) {
        const auto radixText = operation.value(QStringLiteral("radix"));
        if (!busLike || !radixText.isString()) {
            error = QStringLiteral(
                "Field 'radix' is available only for Bus/Enum.");
            return false;
        }
        const auto radix = radixFromString(radixText.toString().toStdString());
        if (!radix) {
            error = QStringLiteral("Unknown Bus/Enum radix.");
            return false;
        }
        lane.radix = *radix;
    }
    if (*kind == LaneKind::Enum) {
        if (!operation.contains(QStringLiteral("enumMap"))) {
            error = QStringLiteral(
                "Enum add-signal requires enumMap.");
            return false;
        }
        if (!enumMapField(
                operation,
                QStringLiteral("enumMap"),
                lane.width,
                lane.isSigned,
                lane.enumMap,
                error)) {
            return false;
        }
    } else if (operation.contains(QStringLiteral("enumMap"))) {
        error = QStringLiteral(
            "Field 'enumMap' is available only for Enum.");
        return false;
    }
    lane.color = deterministicReadableColor(scenario, lane.id);
    if (operation.contains(QStringLiteral("color"))) {
        const auto color = operation.value(QStringLiteral("color"));
        static const QRegularExpression colorExpression(
            QStringLiteral(R"(^#[0-9a-fA-F]{6}$)"));
        if (!color.isString()
            || !colorExpression.match(color.toString()).hasMatch()) {
            error = QStringLiteral(
                "Field 'color' must use #RRGGBB.");
            return false;
        }
        lane.color = color.toString().toLower().toStdString();
    }
    if (operation.contains(QStringLiteral("groupId"))) {
        const auto groupId = requiredString(
            operation, QStringLiteral("groupId"), error);
        if (!groupId) return false;
        const auto* group = findLane(scenario, *groupId);
        if (!group || group->kind != LaneKind::Group) {
            error = QStringLiteral(
                "Group '%1' does not exist or is not a Group lane.")
                        .arg(QString::fromStdString(*groupId));
            return false;
        }
        lane.groupId = *groupId;
    }

    std::optional<std::size_t> insertionIndex;
    if (operation.contains(QStringLiteral("insertionIndex"))) {
        const auto index = integerField(
            operation, QStringLiteral("insertionIndex"), true, error);
        if (!index) return false;
        if (*index < 0
            || static_cast<std::uint64_t>(*index) > scenario.lanes.size()) {
            error = QStringLiteral(
                "Field 'insertionIndex' is outside the lane list.");
            return false;
        }
        insertionIndex = static_cast<std::size_t>(*index);
    }

    if (*kind == LaneKind::Clock) {
        const std::array<QString, 5> busOnly{
            QStringLiteral("width"),
            QStringLiteral("signed"),
            QStringLiteral("radix"),
            QStringLiteral("enumMap"),
            QStringLiteral("clockDomainId"),
        };
        if (std::any_of(
                busOnly.begin(),
                busOnly.end(),
                [&operation](const QString& field) {
                    return operation.contains(field);
                })) {
            error = QStringLiteral(
                "Clock add-signal cannot use Bus/Enum fields or clockDomainId.");
            return false;
        }
        ClockDomain clock;
        clock.id = operation.contains(QStringLiteral("clockId"))
            ? operation.value(QStringLiteral("clockId"))
                  .toString()
                  .trimmed()
                  .toStdString()
            : deterministicStableId(
                  "clock",
                  lane.id + ":" + lane.name,
                  [&project](const std::string& candidate) {
                      return findClock(project, candidate) != nullptr;
                  });
        if (clock.id.empty()) {
            error = QStringLiteral("Field 'clockId' must be a non-empty string.");
            return false;
        }
        if (findClock(project, clock.id)) {
            error = QStringLiteral("Clock ID '%1' already exists.")
                        .arg(QString::fromStdString(clock.id));
            return false;
        }
        clock.name = lane.name;
        if (operation.contains(QStringLiteral("periodTick"))
            || operation.contains(QStringLiteral("period"))) {
            const auto period = automationTickField(
                project,
                operation,
                QStringLiteral("periodTick"),
                QStringLiteral("period"),
                std::nullopt,
                error,
                false);
            if (!period) return false;
            clock.period = *period;
        } else {
            const auto defaultPeriod = toTicks(
                10, TimeUnit::Nanosecond, project.timeBase);
            if (!defaultPeriod) {
                error = QStringLiteral(
                    "Project timebase cannot represent the default 10 ns clock period; provide periodTick or period.");
                return false;
            }
            clock.period = *defaultPeriod;
        }
        if (operation.contains(QStringLiteral("phaseTick"))
            || operation.contains(QStringLiteral("phase"))) {
            const auto phase = automationTickField(
                project,
                operation,
                QStringLiteral("phaseTick"),
                QStringLiteral("phase"),
                std::nullopt,
                error,
                false);
            if (!phase) return false;
            clock.phase = *phase;
        }
        if (operation.contains(QStringLiteral("dutyNumerator"))) {
            const auto numerator = integerField(
                operation, QStringLiteral("dutyNumerator"), true, error);
            if (!numerator) return false;
            clock.dutyCycle.numerator = *numerator;
        }
        if (operation.contains(QStringLiteral("dutyDenominator"))) {
            const auto denominator = integerField(
                operation, QStringLiteral("dutyDenominator"), true, error);
            if (!denominator) return false;
            clock.dutyCycle.denominator = *denominator;
        }
        if (operation.contains(QStringLiteral("activeEdge"))) {
            const auto edgeValue = operation.value(QStringLiteral("activeEdge"));
            if (!edgeValue.isString()) {
                error = QStringLiteral("Field 'activeEdge' must be a string.");
                return false;
            }
            const auto edge = clockEdgeFromString(
                edgeValue.toString().toStdString());
            if (!edge) {
                error = QStringLiteral(
                    "Field 'activeEdge' must be 'rising' or 'falling'.");
                return false;
            }
            clock.activeEdge = *edge;
        }
        if (!clock.isValid()) {
            error = QStringLiteral("Clock parameters are invalid.");
            return false;
        }
        lane.clockDomainId = clock.id;
        const auto changed = stack.execute(std::make_unique<AddLaneCommand>(
            project,
            scenario,
            lane,
            clock,
            insertionIndex));
        result.insert(QStringLiteral("changed"), changed);
        result.insert(QStringLiteral("laneId"), QString::fromStdString(lane.id));
        result.insert(QStringLiteral("clockId"), QString::fromStdString(clock.id));
    } else {
        const std::array<QString, 8> clockOnly{
            QStringLiteral("clockId"),
            QStringLiteral("periodTick"),
            QStringLiteral("period"),
            QStringLiteral("phaseTick"),
            QStringLiteral("phase"),
            QStringLiteral("dutyNumerator"),
            QStringLiteral("dutyDenominator"),
            QStringLiteral("activeEdge"),
        };
        if (std::any_of(
                clockOnly.begin(),
                clockOnly.end(),
                [&operation](const QString& field) {
                    return operation.contains(field);
                })) {
            error = QStringLiteral(
                "Bit, Bus, and Enum add-signal cannot use Clock fields.");
            return false;
        }
        if (operation.contains(QStringLiteral("clockDomainId"))) {
            const auto clockId = requiredString(
                operation, QStringLiteral("clockDomainId"), error);
            if (!clockId) return false;
            if (!findClock(project, *clockId)) {
                error = QStringLiteral("Clock domain '%1' does not exist.")
                            .arg(QString::fromStdString(*clockId));
                return false;
            }
            lane.clockDomainId = *clockId;
        } else if (project.clockDomains.size() == 1) {
            lane.clockDomainId = project.clockDomains.front().id;
        }
        const auto changed = stack.execute(std::make_unique<AddLaneCommand>(
            scenario,
            lane,
            insertionIndex));
        result.insert(QStringLiteral("changed"), changed);
        result.insert(QStringLiteral("laneId"), QString::fromStdString(lane.id));
    }
    result.insert(QStringLiteral("name"), QString::fromStdString(lane.name));
    result.insert(
        QStringLiteral("kind"),
        QString::fromLatin1(toString(lane.kind).data()));
    result.insert(
        QStringLiteral("groupId"),
        QString::fromStdString(lane.groupId));
    if (lane.kind == LaneKind::Enum) {
        result.insert(
            QStringLiteral("enumMap"),
            enumMapObject(lane.enumMap));
        result.insert(
            QStringLiteral("enumSymbolCount"),
            static_cast<qint64>(lane.enumMap.size()));
    }
    return true;
}

bool applyDuplicateSignal(
    Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("laneId"),
             QStringLiteral("id"),
             QStringLiteral("name"),
             QStringLiteral("color"),
             QStringLiteral("groupId"),
             QStringLiteral("insertionIndex"),
             QStringLiteral("beforeLaneId"),
             QStringLiteral("afterLaneId"),
             QStringLiteral("newClockId")},
            error)) {
        return false;
    }
    const auto sourceLaneId = requiredString(
        operation, QStringLiteral("laneId"), error);
    if (!sourceLaneId) return false;
    const auto source = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [&sourceLaneId](const Lane& lane) {
            return lane.id == *sourceLaneId;
        });
    if (source == scenario.lanes.end()) {
        error = QStringLiteral("Lane '%1' does not exist.")
                    .arg(QString::fromStdString(*sourceLaneId));
        return false;
    }
    if (source->kind == LaneKind::Group) {
        error = QStringLiteral(
            "duplicate-signal does not duplicate Group lanes.");
        return false;
    }
    const auto sourceClockDomainId = source->clockDomainId;

    const auto positionModeCount =
        (operation.contains(QStringLiteral("insertionIndex")) ? 1 : 0)
        + (operation.contains(QStringLiteral("beforeLaneId")) ? 1 : 0)
        + (operation.contains(QStringLiteral("afterLaneId")) ? 1 : 0);
    if (positionModeCount > 1) {
        error = QStringLiteral(
            "Use at most one of 'insertionIndex', 'beforeLaneId', or 'afterLaneId'.");
        return false;
    }
    const auto sourceIndex = static_cast<std::size_t>(
        std::distance(scenario.lanes.begin(), source));
    auto insertionIndex = sourceIndex + 1U;
    if (operation.contains(QStringLiteral("insertionIndex"))) {
        const auto index = integerField(
            operation, QStringLiteral("insertionIndex"), true, error);
        if (!index) return false;
        if (*index < 0
            || static_cast<std::uint64_t>(*index)
                > scenario.lanes.size()) {
            error = QStringLiteral(
                "Field 'insertionIndex' is outside the lane list.");
            return false;
        }
        insertionIndex = static_cast<std::size_t>(*index);
    } else if (operation.contains(QStringLiteral("beforeLaneId"))
               || operation.contains(QStringLiteral("afterLaneId"))) {
        const auto field =
            operation.contains(QStringLiteral("beforeLaneId"))
            ? QStringLiteral("beforeLaneId")
            : QStringLiteral("afterLaneId");
        const auto targetId = requiredString(operation, field, error);
        if (!targetId) return false;
        const auto target = std::find_if(
            scenario.lanes.begin(),
            scenario.lanes.end(),
            [&targetId](const Lane& lane) {
                return lane.id == *targetId;
            });
        if (target == scenario.lanes.end()) {
            error = QStringLiteral("Reference lane '%1' does not exist.")
                        .arg(QString::fromStdString(*targetId));
            return false;
        }
        insertionIndex = static_cast<std::size_t>(
            std::distance(scenario.lanes.begin(), target));
        if (field == QStringLiteral("afterLaneId")) {
            ++insertionIndex;
        }
    }

    Lane duplicate = *source;
    if (operation.contains(QStringLiteral("name"))) {
        const auto name = requiredString(
            operation, QStringLiteral("name"), error);
        if (!name) return false;
        if (!uniqueLaneName(scenario, *name)) {
            error = QStringLiteral("Lane name '%1' already exists.")
                        .arg(QString::fromStdString(*name));
            return false;
        }
        duplicate.name = *name;
    } else {
        duplicate.name = availableLaneName(
            scenario, source->name + "_copy");
    }
    duplicate.id = operation.contains(QStringLiteral("id"))
        ? operation.value(QStringLiteral("id"))
              .toString()
              .trimmed()
              .toStdString()
        : deterministicStableId(
              "lane",
              scenario.id + ":" + source->id + ":"
                  + duplicate.name,
              [&scenario](const std::string& candidate) {
                  return findLane(scenario, candidate) != nullptr;
              });
    if (duplicate.id.empty()) {
        error = QStringLiteral("Field 'id' must be a non-empty string.");
        return false;
    }
    if (findLane(scenario, duplicate.id)) {
        error = QStringLiteral("Lane ID '%1' already exists.")
                    .arg(QString::fromStdString(duplicate.id));
        return false;
    }

    duplicate.color = deterministicReadableColor(
        scenario, duplicate.id, source->color);
    if (operation.contains(QStringLiteral("color"))) {
        const auto value = operation.value(QStringLiteral("color"));
        static const QRegularExpression colorExpression(
            QStringLiteral(R"(^#[0-9a-fA-F]{6}$)"));
        if (!value.isString()
            || !colorExpression.match(value.toString()).hasMatch()) {
            error = QStringLiteral("Field 'color' must use #RRGGBB.");
            return false;
        }
        duplicate.color =
            value.toString().toLower().toStdString();
    }
    duplicate.visible = true;
    if (operation.contains(QStringLiteral("groupId"))) {
        const auto value = operation.value(QStringLiteral("groupId"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'groupId' must be a string.");
            return false;
        }
        duplicate.groupId =
            value.toString().trimmed().toStdString();
    }
    if (!duplicate.groupId.empty()) {
        const auto* group = findLane(scenario, duplicate.groupId);
        if (!group || group->kind != LaneKind::Group) {
            error = QStringLiteral(
                "Group '%1' does not exist or is not a Group lane.")
                        .arg(QString::fromStdString(duplicate.groupId));
            return false;
        }
    }

    std::set<std::string> usedSegmentIds;
    for (const auto& lane : scenario.lanes) {
        for (const auto& segment : lane.segments) {
            usedSegmentIds.insert(segment.id);
        }
    }
    for (std::size_t index = 0;
         index < duplicate.segments.size();
         ++index) {
        auto& segment = duplicate.segments.at(index);
        std::string seed;
        appendIdentityField(seed, scenario.id);
        appendIdentityField(seed, duplicate.id);
        appendIdentityField(seed, segment.id);
        appendIdentityField(seed, std::to_string(index));
        segment.id = deterministicStableId(
            "segment-copy-staging",
            seed,
            [&usedSegmentIds](const std::string& candidate) {
                return usedSegmentIds.contains(candidate);
            });
        usedSegmentIds.insert(segment.id);
    }

    std::optional<ClockDomain> duplicateClock;
    if (duplicate.kind == LaneKind::Clock) {
        const auto* sourceClock =
            findClock(project, sourceClockDomainId);
        if (!sourceClock) {
            error = QStringLiteral(
                "Source Clock lane references a missing ClockDomain.");
            return false;
        }
        duplicateClock = *sourceClock;
        duplicateClock->id =
            operation.contains(QStringLiteral("newClockId"))
            ? operation.value(QStringLiteral("newClockId"))
                  .toString()
                  .trimmed()
                  .toStdString()
            : deterministicStableId(
                  "clock",
                  sourceClock->id + ":" + duplicate.id,
                  [&project](const std::string& candidate) {
                      return findClock(project, candidate) != nullptr;
                  });
        if (duplicateClock->id.empty()) {
            error = QStringLiteral(
                "Field 'newClockId' must be a non-empty string.");
            return false;
        }
        if (findClock(project, duplicateClock->id)) {
            error = QStringLiteral("Clock ID '%1' already exists.")
                        .arg(QString::fromStdString(duplicateClock->id));
            return false;
        }
        duplicateClock->name = duplicate.name;
        duplicate.clockDomainId = duplicateClock->id;
    } else if (operation.contains(QStringLiteral("newClockId"))) {
        error = QStringLiteral(
            "Field 'newClockId' is available only when duplicating a Clock lane.");
        return false;
    }

    const auto changed = duplicateClock
        ? stack.execute(std::make_unique<DuplicateLaneCommand>(
              project,
              scenario,
              duplicate,
              *duplicateClock,
              insertionIndex))
        : stack.execute(std::make_unique<DuplicateLaneCommand>(
              scenario,
              duplicate,
              insertionIndex));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("sourceLaneId"),
        QString::fromStdString(*sourceLaneId));
    result.insert(
        QStringLiteral("laneId"),
        QString::fromStdString(duplicate.id));
    result.insert(
        QStringLiteral("name"),
        QString::fromStdString(duplicate.name));
    result.insert(
        QStringLiteral("kind"),
        QString::fromLatin1(toString(duplicate.kind).data()));
    result.insert(
        QStringLiteral("color"),
        QString::fromStdString(duplicate.color));
    result.insert(
        QStringLiteral("groupId"),
        QString::fromStdString(duplicate.groupId));
    result.insert(
        QStringLiteral("insertionIndex"),
        static_cast<qint64>(insertionIndex));
    result.insert(
        QStringLiteral("segmentCount"),
        static_cast<qint64>(duplicate.segments.size()));
    result.insert(
        QStringLiteral("clockDomainId"),
        QString::fromStdString(duplicate.clockDomainId));
    result.insert(
        QStringLiteral("independentClockDomain"),
        duplicateClock.has_value());
    if (duplicateClock) {
        result.insert(
            QStringLiteral("sourceClockId"),
            QString::fromStdString(sourceClockDomainId));
        result.insert(
            QStringLiteral("clockId"),
            QString::fromStdString(duplicateClock->id));
    }
    result.insert(QStringLiteral("copiedEventCount"), 0);
    result.insert(QStringLiteral("copiedRelationCount"), 0);
    result.insert(QStringLiteral("copiedTraceMappingCount"), 0);
    return true;
}

bool applyAddGroup(
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("id"),
             QStringLiteral("name"),
             QStringLiteral("color"),
             QStringLiteral("insertionIndex")},
            error)) {
        return false;
    }
    const auto name = requiredString(
        operation, QStringLiteral("name"), error);
    if (!name) return false;
    if (!uniqueLaneName(scenario, *name)) {
        error = QStringLiteral("Lane name '%1' already exists.")
                    .arg(QString::fromStdString(*name));
        return false;
    }

    Lane group;
    group.id = operation.value(QStringLiteral("id")).isUndefined()
        ? deterministicStableId(
              "group",
              *name,
              [&scenario](const std::string& candidate) {
                  return findLane(scenario, candidate) != nullptr;
              })
        : operation.value(QStringLiteral("id"))
              .toString()
              .trimmed()
              .toStdString();
    if (group.id.empty()) {
        error = QStringLiteral("Field 'id' must be a non-empty string.");
        return false;
    }
    if (findLane(scenario, group.id)) {
        error = QStringLiteral("Lane ID '%1' already exists.")
                    .arg(QString::fromStdString(group.id));
        return false;
    }
    group.name = *name;
    group.kind = LaneKind::Group;
    group.color = "#90a4ae";
    group.height = 40;
    if (operation.contains(QStringLiteral("color"))) {
        const auto color = operation.value(QStringLiteral("color"));
        static const QRegularExpression colorExpression(
            QStringLiteral(R"(^#[0-9a-fA-F]{6}$)"));
        if (!color.isString()
            || !colorExpression.match(color.toString()).hasMatch()) {
            error = QStringLiteral(
                "Field 'color' must use #RRGGBB.");
            return false;
        }
        group.color = color.toString().toLower().toStdString();
    }

    std::optional<std::size_t> insertionIndex;
    if (operation.contains(QStringLiteral("insertionIndex"))) {
        const auto index = integerField(
            operation, QStringLiteral("insertionIndex"), true, error);
        if (!index) return false;
        if (*index < 0
            || static_cast<std::uint64_t>(*index) > scenario.lanes.size()) {
            error = QStringLiteral(
                "Field 'insertionIndex' is outside the lane list.");
            return false;
        }
        insertionIndex = static_cast<std::size_t>(*index);
    }

    const auto changed = stack.execute(std::make_unique<AddLaneCommand>(
        scenario,
        group,
        insertionIndex));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("laneId"),
        QString::fromStdString(group.id));
    result.insert(
        QStringLiteral("groupId"),
        QString::fromStdString(group.id));
    result.insert(
        QStringLiteral("name"),
        QString::fromStdString(group.name));
    result.insert(QStringLiteral("kind"), QStringLiteral("group"));
    return true;
}

bool applyUpdateGroup(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("groupId"),
             QStringLiteral("name"),
             QStringLiteral("color"),
             QStringLiteral("height"),
             QStringLiteral("visible")},
            error)) {
        return false;
    }
    if (operation.size() <= 2) {
        error = QStringLiteral("update-group contains no property change.");
        return false;
    }
    const auto groupId = requiredString(
        operation, QStringLiteral("groupId"), error);
    if (!groupId) return false;
    const auto* group = findLane(scenario, *groupId);
    if (!group || group->kind != LaneKind::Group) {
        error = QStringLiteral("Group '%1' does not exist.")
                    .arg(QString::fromStdString(*groupId));
        return false;
    }

    auto replacement = *group;
    if (operation.contains(QStringLiteral("name"))) {
        const auto name = requiredString(
            operation, QStringLiteral("name"), error);
        if (!name) return false;
        if (!uniqueLaneName(scenario, *name, *groupId)) {
            error = QStringLiteral("Lane name '%1' already exists.")
                        .arg(QString::fromStdString(*name));
            return false;
        }
        replacement.name = *name;
    }
    if (operation.contains(QStringLiteral("color"))) {
        const auto value = operation.value(QStringLiteral("color"));
        static const QRegularExpression colorExpression(
            QStringLiteral(R"(^#[0-9a-fA-F]{6}$)"));
        if (!value.isString()
            || !colorExpression.match(value.toString()).hasMatch()) {
            error = QStringLiteral("Field 'color' must use #RRGGBB.");
            return false;
        }
        replacement.color =
            value.toString().toLower().toStdString();
    }
    if (operation.contains(QStringLiteral("height"))) {
        const auto height = integerField(
            operation, QStringLiteral("height"), true, error);
        if (!height || *height < 30 || *height > 240) {
            if (error.isEmpty()) {
                error = QStringLiteral(
                    "Field 'height' must be from 30 to 240.");
            }
            return false;
        }
        replacement.height = static_cast<int>(*height);
    }
    if (operation.contains(QStringLiteral("visible"))) {
        const auto value = operation.value(QStringLiteral("visible"));
        if (!value.isBool()) {
            error = QStringLiteral("Field 'visible' must be boolean.");
            return false;
        }
        replacement.visible = value.toBool();
    }

    const auto changed = stack.execute(
        std::make_unique<ChangeLaneCommand>(
            project,
            scenario,
            *groupId,
            std::move(replacement)));
    const auto* updated = findLane(scenario, *groupId);
    result.insert(QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("groupId"),
        QString::fromStdString(*groupId));
    if (updated) {
        result.insert(
            QStringLiteral("name"),
            QString::fromStdString(updated->name));
        result.insert(
            QStringLiteral("color"),
            QString::fromStdString(updated->color));
        result.insert(QStringLiteral("height"), updated->height);
        result.insert(QStringLiteral("visible"), updated->visible);
    }
    result.insert(
        QStringLiteral("memberCount"),
        static_cast<qint64>(std::count_if(
            scenario.lanes.begin(),
            scenario.lanes.end(),
            [&groupId](const Lane& lane) {
                return lane.groupId == *groupId;
            })));
    return true;
}

bool applyMoveGroup(
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("groupId"),
             QStringLiteral("destinationIndex"),
             QStringLiteral("beforeLaneId"),
             QStringLiteral("afterLaneId")},
            error)) {
        return false;
    }
    const auto groupId = requiredString(
        operation, QStringLiteral("groupId"), error);
    if (!groupId) return false;
    const auto source = std::find_if(
        scenario.lanes.begin(),
        scenario.lanes.end(),
        [&groupId](const Lane& lane) {
            return lane.id == *groupId;
        });
    if (source == scenario.lanes.end()
        || source->kind != LaneKind::Group) {
        error = QStringLiteral("Group '%1' does not exist.")
                    .arg(QString::fromStdString(*groupId));
        return false;
    }
    const auto modeCount =
        (operation.contains(QStringLiteral("destinationIndex")) ? 1 : 0)
        + (operation.contains(QStringLiteral("beforeLaneId")) ? 1 : 0)
        + (operation.contains(QStringLiteral("afterLaneId")) ? 1 : 0);
    if (modeCount != 1) {
        error = QStringLiteral(
            "Use exactly one of 'destinationIndex', 'beforeLaneId', or 'afterLaneId'.");
        return false;
    }

    const auto sourceIndex = static_cast<std::size_t>(
        std::distance(scenario.lanes.begin(), source));
    std::size_t destinationIndex = sourceIndex;
    if (operation.contains(QStringLiteral("destinationIndex"))) {
        const auto index = integerField(
            operation, QStringLiteral("destinationIndex"), true, error);
        if (!index) return false;
        if (*index < 0
            || static_cast<std::uint64_t>(*index)
                >= scenario.lanes.size()) {
            error = QStringLiteral(
                "Field 'destinationIndex' is outside the lane list.");
            return false;
        }
        destinationIndex = static_cast<std::size_t>(*index);
    } else {
        const auto field =
            operation.contains(QStringLiteral("beforeLaneId"))
            ? QStringLiteral("beforeLaneId")
            : QStringLiteral("afterLaneId");
        const auto targetId = requiredString(operation, field, error);
        if (!targetId) return false;
        if (*targetId == *groupId) {
            error = QStringLiteral(
                "A group cannot be moved relative to itself.");
            return false;
        }
        const auto target = std::find_if(
            scenario.lanes.begin(),
            scenario.lanes.end(),
            [&targetId](const Lane& lane) {
                return lane.id == *targetId;
            });
        if (target == scenario.lanes.end()) {
            error = QStringLiteral("Reference lane '%1' does not exist.")
                        .arg(QString::fromStdString(*targetId));
            return false;
        }
        const auto targetIndex = static_cast<std::size_t>(
            std::distance(scenario.lanes.begin(), target));
        const auto reducedTarget =
            targetIndex - (sourceIndex < targetIndex ? 1U : 0U);
        destinationIndex =
            reducedTarget
            + (field == QStringLiteral("afterLaneId") ? 1U : 0U);
    }

    const auto changed = destinationIndex == sourceIndex
        ? false
        : stack.execute(std::make_unique<MoveLaneCommand>(
              scenario, *groupId, destinationIndex));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("groupId"),
        QString::fromStdString(*groupId));
    result.insert(
        QStringLiteral("beforeIndex"),
        static_cast<qint64>(sourceIndex));
    result.insert(
        QStringLiteral("destinationIndex"),
        static_cast<qint64>(destinationIndex));
    return true;
}

bool applyDeleteGroup(
    Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"), QStringLiteral("groupId")},
            error)) {
        return false;
    }
    const auto groupId = requiredString(
        operation, QStringLiteral("groupId"), error);
    if (!groupId) return false;
    const auto* group = findLane(scenario, *groupId);
    if (!group || group->kind != LaneKind::Group) {
        error = QStringLiteral("Group '%1' does not exist.")
                    .arg(QString::fromStdString(*groupId));
        return false;
    }

    QJsonArray memberLaneIds;
    for (const auto& lane : scenario.lanes) {
        if (lane.groupId == *groupId) {
            memberLaneIds.append(QString::fromStdString(lane.id));
        }
    }
    const auto name = group->name;
    const auto changed = stack.execute(
        std::make_unique<RemoveLaneCommand>(
            project, scenario, *groupId));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("groupId"),
        QString::fromStdString(*groupId));
    result.insert(
        QStringLiteral("name"),
        QString::fromStdString(name));
    result.insert(
        QStringLiteral("ungroupedSignalCount"),
        memberLaneIds.size());
    result.insert(
        QStringLiteral("ungroupedLaneIds"),
        memberLaneIds);
    return true;
}

const Marker* markerById(
    const Scenario& scenario,
    const std::string_view markerId)
{
    const auto iterator = std::find_if(
        scenario.markers.begin(),
        scenario.markers.end(),
        [markerId](const Marker& marker) {
            return marker.id == markerId;
        });
    return iterator == scenario.markers.end() ? nullptr : &*iterator;
}

std::optional<std::size_t> uniqueEventIndexById(
    const Scenario& scenario,
    const std::string_view eventId,
    QString& error)
{
    std::optional<std::size_t> match;
    for (std::size_t index = 0;
         index < scenario.events.size();
         ++index) {
        if (scenario.events.at(index).id != eventId) continue;
        if (match) {
            error = QStringLiteral(
                "Event stable ID '%1' is ambiguous.")
                        .arg(QString::fromStdString(
                            std::string(eventId)));
            return std::nullopt;
        }
        match = index;
    }
    if (!match) {
        error = QStringLiteral("Event '%1' does not exist.")
                    .arg(QString::fromStdString(
                        std::string(eventId)));
    }
    return match;
}

std::optional<std::size_t> uniqueMarkerIndexById(
    const Scenario& scenario,
    const std::string_view markerId,
    QString& error)
{
    std::optional<std::size_t> match;
    for (std::size_t index = 0;
         index < scenario.markers.size();
         ++index) {
        if (scenario.markers.at(index).id != markerId) continue;
        if (match) {
            error = QStringLiteral(
                "Marker stable ID '%1' is ambiguous.")
                        .arg(QString::fromStdString(
                            std::string(markerId)));
            return std::nullopt;
        }
        match = index;
    }
    if (!match) {
        error = QStringLiteral("Marker '%1' does not exist.")
                    .arg(QString::fromStdString(
                        std::string(markerId)));
    }
    return match;
}

std::string markerRepairRef(
    const Scenario& scenario,
    const std::size_t markerIndex)
{
    const auto& marker = scenario.markers.at(markerIndex);
    std::string seed;
    appendIdentityField(seed, scenario.id);
    appendIdentityField(seed, std::to_string(markerIndex));
    appendIdentityField(seed, marker.id);
    appendIdentityField(seed, marker.name);
    appendIdentityField(seed, std::to_string(marker.start));
    appendIdentityField(seed, std::to_string(marker.end));
    appendIdentityField(seed, toString(marker.kind));
    appendIdentityField(seed, marker.note);
    appendIdentityExtensions(seed, marker.extensions);
    const auto digest = QCryptographicHash::hash(
        QByteArray::fromStdString(seed),
        QCryptographicHash::Sha256).toHex();
    return QStringLiteral("marker-ref-v1:%1:%2")
        .arg(static_cast<qulonglong>(markerIndex))
        .arg(QString::fromLatin1(
            digest.constData(), digest.size()))
        .toStdString();
}

std::optional<std::size_t> resolveMarkerRepairRef(
    const Scenario& scenario,
    const std::string& repairRef,
    QString& error)
{
    const auto fields =
        QString::fromStdString(repairRef).split(':');
    bool indexValid = false;
    const auto markerIndex =
        fields.size() == 3
        ? fields.at(1).toULongLong(&indexValid)
        : qulonglong{0};
    if (fields.size() != 3
        || fields.at(0) != QStringLiteral("marker-ref-v1")
        || !indexValid
        || markerIndex
            >= static_cast<qulonglong>(
                scenario.markers.size())) {
        error = QStringLiteral(
            "Marker repair reference is invalid or stale.");
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(markerIndex);
    if (markerRepairRef(scenario, index) != repairRef) {
        error = QStringLiteral(
            "Marker repair reference is invalid or stale.");
        return std::nullopt;
    }
    return index;
}

bool uniqueMarkerNameAtIndex(
    const Scenario& scenario,
    const std::string& name,
    const std::size_t ignoredIndex)
{
    const auto candidate = QString::fromStdString(name);
    for (std::size_t index = 0;
         index < scenario.markers.size();
         ++index) {
        if (index == ignoredIndex) continue;
        if (QString::compare(
                QString::fromStdString(
                    scenario.markers.at(index).name),
                candidate,
                Qt::CaseInsensitive)
            == 0) {
            return false;
        }
    }
    return true;
}

bool uniqueMarkerIdAtIndex(
    const Scenario& scenario,
    const std::string& id,
    const std::size_t ignoredIndex)
{
    for (std::size_t index = 0;
         index < scenario.markers.size();
         ++index) {
        if (index != ignoredIndex
            && scenario.markers.at(index).id == id) {
            return false;
        }
    }
    return true;
}

bool markerGeometryValid(
    const Scenario& scenario,
    const Marker& marker)
{
    return marker.start >= 0
        && marker.end >= marker.start
        && marker.end <= scenario.duration
        && (marker.kind != MarkerKind::Point
            || marker.start == marker.end);
}

struct MarkerIntegrityProblem {
    QString code;
    QString property;
    QString message;
    bool nameIssue{false};
};

std::vector<MarkerIntegrityProblem> markerIntegrityProblems(
    const Scenario& scenario,
    const Marker& marker,
    const std::size_t nameCount)
{
    std::vector<MarkerIntegrityProblem> problems;
    const auto markerName =
        QString::fromStdString(marker.name).trimmed();
    if (markerName.isEmpty()) {
        problems.push_back({
            QStringLiteral("empty-name"),
            QStringLiteral("name"),
            QStringLiteral("Marker name is empty."),
            true,
        });
    }
    if (nameCount > 1) {
        problems.push_back({
            QStringLiteral("duplicate-name"),
            QStringLiteral("name"),
            QStringLiteral(
                "Marker name '%1' is shared by %2 Markers in the Scenario.")
                .arg(markerName)
                .arg(static_cast<qulonglong>(nameCount)),
            true,
        });
    }
    if (marker.start < 0) {
        problems.push_back({
            QStringLiteral("start-before-scenario"),
            QStringLiteral("startTick"),
            QStringLiteral("Marker starts before Scenario tick 0."),
            false,
        });
    }
    if (marker.end < marker.start) {
        problems.push_back({
            QStringLiteral("end-before-start"),
            QStringLiteral("endTick"),
            QStringLiteral("Marker end precedes its start."),
            false,
        });
    } else if (marker.end > scenario.duration) {
        problems.push_back({
            QStringLiteral("end-after-scenario"),
            QStringLiteral("endTick"),
            QStringLiteral(
                "Marker end %1 exceeds Scenario duration %2.")
                .arg(marker.end)
                .arg(scenario.duration),
            false,
        });
    }
    if (marker.kind == MarkerKind::Point
        && marker.start != marker.end) {
        problems.push_back({
            QStringLiteral("point-has-range"),
            QStringLiteral("kind"),
            QStringLiteral(
                "Point Marker must have identical start and end ticks."),
            false,
        });
    }
    return problems;
}

struct MarkerIntegrityAudit {
    QJsonArray issues;
    std::size_t affectedMarkerCount{0};
    std::size_t nameIssueCount{0};
    std::size_t geometryIssueCount{0};
};

MarkerIntegrityAudit auditMarkerIntegrity(
    const Project& project,
    const std::optional<std::string>& selectedScenarioId)
{
    MarkerIntegrityAudit audit;
    for (std::size_t scenarioIndex = 0;
         scenarioIndex < project.scenarios.size();
         ++scenarioIndex) {
        const auto& scenario =
            project.scenarios.at(scenarioIndex);
        if (selectedScenarioId
            && scenario.id != *selectedScenarioId) {
            continue;
        }
        std::map<QString, std::size_t> nameCounts;
        for (const auto& marker : scenario.markers) {
            ++nameCounts[
                QString::fromStdString(marker.name)
                    .trimmed()
                    .toCaseFolded()];
        }
        for (std::size_t markerIndex = 0;
             markerIndex < scenario.markers.size();
             ++markerIndex) {
            const auto& marker =
                scenario.markers.at(markerIndex);
            const auto markerName =
                QString::fromStdString(marker.name).trimmed();
            const auto nameCount =
                nameCounts.at(markerName.toCaseFolded());
            const auto problems =
                markerIntegrityProblems(
                    scenario, marker, nameCount);
            if (!problems.empty()) {
                ++audit.affectedMarkerCount;
            }
            for (const auto& problem : problems) {
                if (problem.nameIssue) {
                    ++audit.nameIssueCount;
                } else {
                    ++audit.geometryIssueCount;
                }
                audit.issues.append(QJsonObject{
                    {QStringLiteral("scenarioId"),
                     QString::fromStdString(scenario.id)},
                    {QStringLiteral("scenarioIndex"),
                     static_cast<qint64>(scenarioIndex)},
                    {QStringLiteral("code"), problem.code},
                    {QStringLiteral("severity"),
                     QStringLiteral("error")},
                    {QStringLiteral("message"),
                     problem.message},
                    {QStringLiteral("objectKind"),
                     QStringLiteral("marker")},
                    {QStringLiteral("path"),
                     QStringLiteral(
                         "scenarios[%1].markers[%2].%3")
                         .arg(
                             static_cast<qulonglong>(
                                 scenarioIndex))
                         .arg(
                             static_cast<qulonglong>(
                                 markerIndex))
                         .arg(problem.property)},
                    {QStringLiteral("objectIndex"),
                     static_cast<qint64>(markerIndex)},
                    {QStringLiteral("markerId"),
                     QString::fromStdString(marker.id)},
                    {QStringLiteral("name"),
                     QString::fromStdString(marker.name)},
                    {QStringLiteral("nameCount"),
                     static_cast<qint64>(nameCount)},
                    {QStringLiteral("startTick"),
                     integerValue(marker.start)},
                    {QStringLiteral("endTick"),
                     integerValue(marker.end)},
                    {QStringLiteral("kind"),
                     QString::fromLatin1(
                         toString(marker.kind).data())},
                });
            }
        }
    }
    return audit;
}

std::size_t projectLaneMatchCount(
    const Project& project,
    const std::string_view laneId)
{
    std::size_t count = 0;
    for (const auto& scenario : project.scenarios) {
        count += static_cast<std::size_t>(
            std::count_if(
                scenario.lanes.begin(),
                scenario.lanes.end(),
                [laneId](const Lane& lane) {
                    return lane.kind != LaneKind::Group
                        && lane.id == laneId;
                }));
    }
    return count;
}

std::vector<QString> traceMappingProblems(
    const Project& project,
    const std::string_view expectedLaneId,
    const std::string_view actualSignalId)
{
    std::vector<QString> problems;
    if (expectedLaneId.empty()) {
        problems.push_back(
            QStringLiteral("missing-expected-lane-id"));
    } else if (
        projectLaneMatchCount(project, expectedLaneId) == 0) {
        problems.push_back(
            QStringLiteral("expected-lane-not-found"));
    }
    if (actualSignalId.empty()) {
        problems.push_back(
            QStringLiteral("empty-actual-signal-id"));
    }
    return problems;
}

std::optional<std::size_t> uniqueTraceIndexById(
    const Project& project,
    const std::string_view traceId,
    QString& error)
{
    std::optional<std::size_t> match;
    for (std::size_t index = 0;
         index < project.importedTraces.size();
         ++index) {
        if (project.importedTraces.at(index).id != traceId) {
            continue;
        }
        if (match) {
            error = QStringLiteral(
                "Imported trace stable ID '%1' is ambiguous.")
                        .arg(QString::fromStdString(
                            std::string(traceId)));
            return std::nullopt;
        }
        match = index;
    }
    if (!match) {
        error = QStringLiteral(
            "Imported trace '%1' does not exist.")
                    .arg(QString::fromStdString(
                        std::string(traceId)));
    }
    return match;
}

std::string traceRepairRef(
    const Project& project,
    const std::size_t traceIndex)
{
    const auto& trace =
        project.importedTraces.at(traceIndex);
    std::string seed;
    appendIdentityField(seed, project.id);
    appendIdentityField(
        seed, std::to_string(traceIndex));
    appendIdentityField(seed, trace.id);
    appendIdentityField(seed, trace.path);
    appendIdentityField(seed, trace.format);
    appendIdentityField(
        seed, std::to_string(trace.offset));
    for (const auto& [laneId, signalId] :
         trace.signalMapping) {
        appendIdentityField(seed, laneId);
        appendIdentityField(seed, signalId);
    }
    appendIdentityExtensions(seed, trace.extensions);
    const auto digest = QCryptographicHash::hash(
        QByteArray::fromStdString(seed),
        QCryptographicHash::Sha256).toHex();
    return QStringLiteral("trace-ref-v1:%1:%2")
        .arg(static_cast<qulonglong>(traceIndex))
        .arg(QString::fromLatin1(
            digest.constData(), digest.size()))
        .toStdString();
}

std::optional<std::size_t> resolveTraceRepairRef(
    const Project& project,
    const std::string& repairRef,
    QString& error)
{
    const auto fields =
        QString::fromStdString(repairRef).split(':');
    bool indexValid = false;
    const auto traceIndex =
        fields.size() == 3
        ? fields.at(1).toULongLong(&indexValid)
        : qulonglong{0};
    if (fields.size() != 3
        || fields.at(0)
            != QStringLiteral("trace-ref-v1")
        || !indexValid
        || traceIndex
            >= static_cast<qulonglong>(
                project.importedTraces.size())) {
        error = QStringLiteral(
            "Imported trace repair reference is invalid or stale.");
        return std::nullopt;
    }
    const auto index =
        static_cast<std::size_t>(traceIndex);
    if (traceRepairRef(project, index) != repairRef) {
        error = QStringLiteral(
            "Imported trace repair reference is invalid or stale.");
        return std::nullopt;
    }
    return index;
}

std::string deterministicTraceIdentity(
    const Project& project,
    const std::size_t traceIndex)
{
    const auto seed =
        traceRepairRef(project, traceIndex);
    return deterministicStableId(
        "trace",
        seed,
        [&project, traceIndex](
            const std::string& candidate) {
            for (std::size_t index = 0;
                 index
                 < project.importedTraces.size();
                 ++index) {
                if (index != traceIndex
                    && project.importedTraces
                           .at(index).id
                        == candidate) {
                    return true;
                }
            }
            return false;
        });
}

QJsonObject traceAutomationObject(
    const Project& project,
    const std::size_t traceIndex)
{
    const auto& trace =
        project.importedTraces.at(traceIndex);
    const auto idMatchCount =
        static_cast<std::size_t>(
            std::count_if(
                project.importedTraces.begin(),
                project.importedTraces.end(),
                [&trace](const ImportedTrace& candidate) {
                    return candidate.id == trace.id;
                }));
    return {
        {QStringLiteral("traceId"),
         QString::fromStdString(trace.id)},
        {QStringLiteral("traceIndex"),
         static_cast<qint64>(traceIndex)},
        {QStringLiteral("traceRef"),
         QString::fromStdString(
             traceRepairRef(project, traceIndex))},
        {QStringLiteral("traceIdMatchCount"),
         static_cast<qint64>(idMatchCount)},
        {QStringLiteral("path"),
         QString::fromStdString(trace.path)},
        {QStringLiteral("format"),
         QString::fromStdString(trace.format)},
        {QStringLiteral("offsetTick"),
         integerValue(trace.offset)},
        {QStringLiteral("mappingCount"),
         static_cast<qint64>(
             trace.signalMapping.size())},
    };
}

QString normalizedTraceFormat(
    const std::string_view format)
{
    return QString::fromStdString(
               std::string(format))
        .trimmed()
        .toLower();
}

std::vector<QString> traceReferenceProblems(
    const ImportedTrace& trace)
{
    std::vector<QString> problems;
    if (QString::fromStdString(trace.path)
            .trimmed()
            .isEmpty()) {
        problems.push_back(
            QStringLiteral("empty-path"));
    }
    const auto format =
        normalizedTraceFormat(trace.format);
    if (format != QStringLiteral("vcd")
        && format != QStringLiteral("csv")
        && format != QStringLiteral("fst")) {
        problems.push_back(
            QStringLiteral("unsupported-format"));
    }
    return problems;
}

QJsonObject traceReferenceValidationObject(
    const Project& project,
    const std::size_t traceIndex)
{
    const auto& trace =
        project.importedTraces.at(traceIndex);
    const auto problems =
        traceReferenceProblems(trace);
    const auto pathPresent =
        !QString::fromStdString(trace.path)
             .trimmed()
             .isEmpty();
    const auto normalizedFormat =
        normalizedTraceFormat(trace.format);
    const auto formatSupported =
        normalizedFormat == QStringLiteral("vcd")
        || normalizedFormat == QStringLiteral("csv")
        || normalizedFormat == QStringLiteral("fst");
    QJsonArray problemValues;
    for (const auto& problem : problems) {
        problemValues.append(problem);
    }
    return {
        {QStringLiteral("path"),
         QString::fromStdString(trace.path)},
        {QStringLiteral("pathPresent"),
         pathPresent},
        {QStringLiteral("format"),
         QString::fromStdString(trace.format)},
        {QStringLiteral("normalizedFormat"),
         normalizedFormat},
        {QStringLiteral("formatSupported"),
         formatSupported},
        {QStringLiteral("supportedFormats"),
         QJsonArray{
             QStringLiteral("vcd"),
             QStringLiteral("csv"),
             QStringLiteral("fst"),
         }},
        {QStringLiteral("filesystemVerification"),
         QStringLiteral("not-performed")},
        {QStringLiteral("structurallyValid"),
         problems.empty()},
        {QStringLiteral("problems"),
         problemValues},
        {QStringLiteral("repairAction"),
         problems.empty()
             ? QStringLiteral("none")
             : QStringLiteral(
                   "replace-invalid-reference-properties")},
        {QStringLiteral("repairable"),
         !problems.empty()},
    };
}

struct TraceReferenceAudit {
    QJsonArray issues;
    std::size_t affectedTraceCount{0};
    std::size_t emptyPathCount{0};
    std::size_t unsupportedFormatCount{0};
};

TraceReferenceAudit auditTraceReferences(
    const Project& project)
{
    TraceReferenceAudit audit;
    for (std::size_t traceIndex = 0;
         traceIndex < project.importedTraces.size();
         ++traceIndex) {
        const auto& trace =
            project.importedTraces.at(traceIndex);
        const auto problems =
            traceReferenceProblems(trace);
        if (problems.empty()) continue;

        ++audit.affectedTraceCount;
        QJsonArray problemValues;
        QJsonArray paths;
        QStringList descriptions;
        for (const auto& problem : problems) {
            problemValues.append(problem);
            if (problem
                == QStringLiteral("empty-path")) {
                ++audit.emptyPathCount;
                paths.append(
                    QStringLiteral(
                        "importedTraces[%1].path")
                        .arg(
                            static_cast<qulonglong>(
                                traceIndex)));
                descriptions.append(
                    QStringLiteral(
                        "the source path is empty"));
            } else {
                ++audit.unsupportedFormatCount;
                paths.append(
                    QStringLiteral(
                        "importedTraces[%1].format")
                        .arg(
                            static_cast<qulonglong>(
                                traceIndex)));
                descriptions.append(
                    QStringLiteral(
                        "the format is not VCD, FST, or CSV"));
            }
        }
        audit.issues.append(QJsonObject{
            {QStringLiteral("scenarioId"),
             QString{}},
            {QStringLiteral("code"),
             QStringLiteral(
                 "trace-reference-invalid")},
            {QStringLiteral("severity"),
             QStringLiteral("error")},
            {QStringLiteral("message"),
             QStringLiteral(
                 "Imported trace reference is invalid: %1.")
                 .arg(
                     descriptions.join(
                         QStringLiteral(", ")))},
            {QStringLiteral("objectKind"),
             QStringLiteral(
                 "imported-trace-reference")},
            {QStringLiteral("path"),
             paths.first()},
            {QStringLiteral("paths"),
             paths},
            {QStringLiteral("objectIndex"),
             static_cast<qint64>(traceIndex)},
            {QStringLiteral("traceIndex"),
             static_cast<qint64>(traceIndex)},
            {QStringLiteral("traceId"),
             QString::fromStdString(trace.id)},
            {QStringLiteral("problems"),
             problemValues},
        });
    }
    return audit;
}

QJsonObject traceMappingValidationObject(
    const Project& project,
    const std::size_t traceIndex,
    const std::string& expectedLaneId)
{
    const auto& trace =
        project.importedTraces.at(traceIndex);
    const auto mapping =
        trace.signalMapping.find(expectedLaneId);
    const auto actualSignalId =
        mapping == trace.signalMapping.end()
        ? std::string{}
        : mapping->second;
    const auto laneMatchCount =
        projectLaneMatchCount(
            project, expectedLaneId);
    const auto problems =
        mapping == trace.signalMapping.end()
        ? std::vector<QString>{
              QStringLiteral("mapping-not-found")}
        : traceMappingProblems(
              project,
              expectedLaneId,
              actualSignalId);
    QJsonArray problemValues;
    for (const auto& problem : problems) {
        problemValues.append(problem);
    }
    const auto structurallyValid =
        mapping != trace.signalMapping.end()
        && problems.empty();
    return {
        {QStringLiteral("expectedLaneId"),
         QString::fromStdString(expectedLaneId)},
        {QStringLiteral("actualSignalId"),
         QString::fromStdString(actualSignalId)},
        {QStringLiteral("expectedLaneMatchCount"),
         static_cast<qint64>(laneMatchCount)},
        {QStringLiteral("expectedLaneReferenceValid"),
         !expectedLaneId.empty()
             && laneMatchCount > 0},
        {QStringLiteral("actualSignalIdPresent"),
         !actualSignalId.empty()},
        {QStringLiteral("externalSignalVerification"),
         QStringLiteral("not-performed")},
        {QStringLiteral("structurallyValid"),
         structurallyValid},
        {QStringLiteral("problems"), problemValues},
        {QStringLiteral("repairAction"),
         structurallyValid
             ? QStringLiteral("none")
             : QStringLiteral(
                   "remove-invalid-mapping")},
        {QStringLiteral("repairable"),
         mapping != trace.signalMapping.end()
             && !problems.empty()},
    };
}

struct TraceMappingAudit {
    QJsonArray issues;
    std::size_t affectedTraceCount{0};
    std::size_t invalidMappingCount{0};
    std::size_t missingLaneReferenceCount{0};
    std::size_t emptyActualSignalIdCount{0};
};

TraceMappingAudit auditTraceMappings(
    const Project& project)
{
    TraceMappingAudit audit;
    for (std::size_t traceIndex = 0;
         traceIndex < project.importedTraces.size();
         ++traceIndex) {
        const auto& trace =
            project.importedTraces.at(traceIndex);
        bool traceAffected = false;
        std::size_t mappingIndex = 0;
        for (const auto& [laneId, signalId] :
             trace.signalMapping) {
            const auto problems =
                traceMappingProblems(
                    project, laneId, signalId);
            if (problems.empty()) {
                ++mappingIndex;
                continue;
            }
            traceAffected = true;
            ++audit.invalidMappingCount;
            if (laneId.empty()
                || projectLaneMatchCount(
                       project, laneId) == 0) {
                ++audit.missingLaneReferenceCount;
            }
            if (signalId.empty()) {
                ++audit.emptyActualSignalIdCount;
            }
            QJsonArray problemValues;
            QStringList descriptions;
            for (const auto& problem : problems) {
                problemValues.append(problem);
                descriptions.append(
                    problem
                            == QStringLiteral(
                                "missing-expected-lane-id")
                        ? QStringLiteral(
                              "the Expected Lane ID is empty")
                        : problem
                                  == QStringLiteral(
                                      "expected-lane-not-found")
                            ? QStringLiteral(
                                  "the Expected Lane no longer exists")
                            : QStringLiteral(
                                  "the Actual signal ID is empty"));
            }
            auto escapedLaneId =
                QString::fromStdString(laneId);
            escapedLaneId.replace(
                QStringLiteral("\\"),
                QStringLiteral("\\\\"));
            escapedLaneId.replace(
                QStringLiteral("'"),
                QStringLiteral("\\'"));
            audit.issues.append(QJsonObject{
                {QStringLiteral("scenarioId"),
                 QString{}},
                {QStringLiteral("code"),
                 QStringLiteral(
                     "trace-mapping-invalid")},
                {QStringLiteral("severity"),
                 QStringLiteral("error")},
                {QStringLiteral("message"),
                 QStringLiteral(
                     "Imported trace mapping is invalid: %1.")
                     .arg(
                         descriptions.join(
                             QStringLiteral(", ")))},
                {QStringLiteral("objectKind"),
                 QStringLiteral("trace-mapping")},
                {QStringLiteral("path"),
                 QStringLiteral(
                     "importedTraces[%1].signalMapping['%2']")
                     .arg(
                         static_cast<qulonglong>(
                             traceIndex))
                     .arg(escapedLaneId)},
                {QStringLiteral("objectIndex"),
                 static_cast<qint64>(traceIndex)},
                {QStringLiteral("traceIndex"),
                 static_cast<qint64>(traceIndex)},
                {QStringLiteral("mappingIndex"),
                 static_cast<qint64>(mappingIndex)},
                {QStringLiteral("traceId"),
                 QString::fromStdString(trace.id)},
                {QStringLiteral("laneId"),
                 QString::fromStdString(laneId)},
                {QStringLiteral("actualSignalId"),
                 QString::fromStdString(signalId)},
                {QStringLiteral("problems"),
                 problemValues},
            });
            ++mappingIndex;
        }
        if (traceAffected) {
            ++audit.affectedTraceCount;
        }
    }
    return audit;
}

const Event* waveformEventAt(
    const Scenario& scenario,
    const std::string_view laneId,
    const Tick tick,
    QString& error)
{
    const Event* match = nullptr;
    for (const auto& event : scenario.events) {
        if (!event.waveformLinked
            || event.laneId != laneId
            || event.tick != tick) {
            continue;
        }
        if (match) {
            error = QStringLiteral(
                "Multiple waveform events exist for Lane '%1' at tick %2.")
                        .arg(
                            QString::fromStdString(std::string(laneId)),
                            QString::number(tick));
            return nullptr;
        }
        match = &event;
    }
    if (!match) {
        error = QStringLiteral(
            "Lane '%1' has no waveform edge at tick %2.")
                    .arg(
                        QString::fromStdString(std::string(laneId)),
                        QString::number(tick));
        return nullptr;
    }
    if (match->id.empty()) {
        error = QStringLiteral(
            "Lane '%1' has a waveform edge without a stable Event ID at tick %2.")
                    .arg(
                        QString::fromStdString(std::string(laneId)),
                        QString::number(tick));
        return nullptr;
    }
    const auto resolution =
        resolveRelationEndpoint(scenario, match->id);
    if (resolution.eventIdCount != 1) {
        error = QStringLiteral(
            "Waveform Event ID '%1' is ambiguous (%2 matches).")
                    .arg(
                        QString::fromStdString(match->id),
                        QString::number(resolution.eventIdCount));
        return nullptr;
    }
    return match;
}

bool optionalAutomationTickField(
    const Project& project,
    const QJsonObject& object,
    const QString& tickName,
    const QString& timeName,
    const std::optional<std::string>& clockId,
    Tick& target,
    bool& provided,
    QString& error)
{
    const auto hasTick = object.contains(tickName);
    const auto hasTime = object.contains(timeName);
    if (!hasTick && !hasTime) {
        provided = false;
        return true;
    }
    if (hasTick && hasTime) {
        error = QStringLiteral("Use at most one of '%1' or '%2'.")
                    .arg(tickName, timeName);
        return false;
    }
    const auto parsed = automationTickField(
        project,
        object,
        tickName,
        timeName,
        clockId,
        error);
    if (!parsed) return false;
    target = *parsed;
    provided = true;
    return true;
}

bool relationDelayField(
    const Project& project,
    const QJsonObject& object,
    const QString& baseName,
    const std::optional<std::string>& clockId,
    const bool required,
    Tick& target,
    bool& provided,
    QString& error)
{
    const auto tickName = baseName + QStringLiteral("Tick");
    const auto cyclesName = baseName + QStringLiteral("Cycles");
    const auto hasTick = object.contains(tickName);
    const auto hasTime = object.contains(baseName);
    const auto hasCycles = object.contains(cyclesName);
    const auto count = static_cast<int>(hasTick)
        + static_cast<int>(hasTime)
        + static_cast<int>(hasCycles);
    if (count == 0) {
        if (required) {
            error = QStringLiteral(
                "Use exactly one of '%1', '%2', or '%3'.")
                        .arg(tickName, baseName, cyclesName);
            return false;
        }
        provided = false;
        return true;
    }
    if (count != 1) {
        error = QStringLiteral(
            "Use exactly one of '%1', '%2', or '%3'.")
                    .arg(tickName, baseName, cyclesName);
        return false;
    }

    if (hasCycles) {
        const auto cycles = integerField(
            object, cyclesName, true, error);
        if (!cycles) return false;
        if (*cycles < 0) {
            error = QStringLiteral("Field '%1' must be non-negative.")
                        .arg(cyclesName);
            return false;
        }
        if (!clockId) {
            error = QStringLiteral(
                "Field '%1' requires an unambiguous clockId.")
                        .arg(cyclesName);
            return false;
        }
        const auto* clock = findClock(project, *clockId);
        if (!clock || !clock->isValid()) {
            error = QStringLiteral(
                "Field '%1' references an invalid clock.")
                        .arg(cyclesName);
            return false;
        }
        if (*cycles != 0
            && clock->period
                > std::numeric_limits<Tick>::max() / *cycles) {
            error = QStringLiteral("Field '%1' overflows tick range.")
                        .arg(cyclesName);
            return false;
        }
        target = *cycles * clock->period;
    } else {
        const auto parsed = automationTickField(
            project,
            object,
            tickName,
            baseName,
            clockId,
            error,
            false);
        if (!parsed) return false;
        target = *parsed;
        if (target < 0) {
            error = QStringLiteral("Field '%1' must be non-negative.")
                        .arg(hasTick ? tickName : baseName);
            return false;
        }
    }
    provided = true;
    return true;
}

bool relationClockContext(
    const Project& project,
    const Scenario& scenario,
    const QJsonObject& operation,
    const std::string& sourceLaneId,
    const std::string& targetLaneId,
    std::optional<std::string>& clockId,
    QString& error)
{
    if (!resolveClockContext(
            project,
            scenario,
            operation,
            {sourceLaneId, targetLaneId},
            clockId,
            error)) {
        return false;
    }
    std::set<std::string> endpointClocks;
    for (const auto& laneId : {sourceLaneId, targetLaneId}) {
        const auto* lane = findLane(scenario, laneId);
        if (lane && !lane->clockDomainId.empty()) {
            endpointClocks.insert(lane->clockDomainId);
        }
    }
    if (endpointClocks.size() > 1) {
        error = QStringLiteral(
            "Relation endpoint lanes use different clock domains.");
        return false;
    }
    if (clockId && !endpointClocks.empty()
        && !endpointClocks.contains(*clockId)) {
        error = QStringLiteral(
            "clockId does not match the relation endpoint clock domain.");
        return false;
    }
    if (!clockId && endpointClocks.size() == 1) {
        clockId = *endpointClocks.begin();
    }
    return true;
}

std::optional<std::size_t> uniqueRelationIndexById(
    const Scenario& scenario,
    const std::string_view relationId,
    QString& error)
{
    std::optional<std::size_t> match;
    for (std::size_t index = 0;
         index < scenario.relations.size();
         ++index) {
        if (scenario.relations.at(index).id != relationId) continue;
        if (match) {
            error = QStringLiteral(
                "Relation stable ID '%1' is ambiguous.")
                        .arg(QString::fromStdString(
                            std::string(relationId)));
            return std::nullopt;
        }
        match = index;
    }
    if (!match) {
        error = QStringLiteral("Relation '%1' does not exist.")
                    .arg(QString::fromStdString(
                        std::string(relationId)));
    }
    return match;
}

std::string relationRepairRef(
    const Scenario& scenario,
    const std::size_t relationIndex)
{
    const auto& relation = scenario.relations.at(relationIndex);
    std::string seed;
    appendIdentityField(seed, scenario.id);
    appendIdentityField(seed, std::to_string(relationIndex));
    appendIdentityField(seed, relation.id);
    appendIdentityField(seed, relation.sourceEventId);
    appendIdentityField(seed, relation.targetEventId);
    appendIdentityField(
        seed, std::to_string(relation.minimumDelay));
    appendIdentityField(
        seed, std::to_string(relation.maximumDelay));
    appendIdentityField(seed, relation.clockDomainId);
    appendIdentityField(seed, relation.condition);
    appendIdentityField(seed, toString(relation.severity));
    appendIdentityField(seed, relation.description);
    appendIdentityExtensions(seed, relation.extensions);
    const auto digest = QCryptographicHash::hash(
        QByteArray::fromStdString(seed),
        QCryptographicHash::Sha256).toHex();
    return QStringLiteral("relation-ref-v1:%1:%2")
        .arg(static_cast<qulonglong>(relationIndex))
        .arg(QString::fromLatin1(
            digest.constData(), digest.size()))
        .toStdString();
}

std::optional<std::size_t> resolveRelationRepairRef(
    const Scenario& scenario,
    const std::string& repairRef,
    QString& error)
{
    const auto fields =
        QString::fromStdString(repairRef).split(':');
    bool indexValid = false;
    const auto relationIndex =
        fields.size() == 3
        ? fields.at(1).toULongLong(&indexValid)
        : qulonglong{0};
    if (fields.size() != 3
        || fields.at(0) != QStringLiteral("relation-ref-v1")
        || !indexValid
        || relationIndex
            >= static_cast<qulonglong>(
                scenario.relations.size())) {
        error = QStringLiteral(
            "Relation repair reference is invalid or stale.");
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(relationIndex);
    if (relationRepairRef(scenario, index) != repairRef) {
        error = QStringLiteral(
            "Relation repair reference is invalid or stale.");
        return std::nullopt;
    }
    return index;
}

QJsonObject relationAutomationObject(
    const Project& project,
    const Scenario& scenario,
    const Relation& relation,
    const std::size_t relationIndex,
    const std::size_t relationIdCount)
{
    const auto sourceResolution =
        resolveRelationEndpoint(
            scenario, relation.sourceEventId);
    const auto targetResolution =
        resolveRelationEndpoint(
            scenario, relation.targetEventId);
    bool sourceReady = false;
    bool targetReady = false;
    const auto source = relationEndpointQueryObject(
        project, scenario, sourceResolution, sourceReady);
    const auto target = relationEndpointQueryObject(
        project, scenario, targetResolution, targetReady);
    const auto endpointsReady = sourceReady && targetReady;
    const auto addressable =
        !relation.id.empty() && relationIdCount == 1;
    QJsonArray identityIssues;
    if (relation.id.empty()) {
        identityIssues.append(
            QStringLiteral("missing-id"));
    }
    if (relationIdCount > 1) {
        identityIssues.append(
            QStringLiteral("duplicate-id"));
    }

    std::optional<Tick> observedDelay;
    if (sourceResolution.event && targetResolution.event) {
        observedDelay =
            targetResolution.event->tick
            - sourceResolution.event->tick;
    }
    const auto* clock =
        relation.clockDomainId.empty()
        ? nullptr
        : findClock(project, relation.clockDomainId);
    return {
        {QStringLiteral("relationId"),
         QString::fromStdString(relation.id)},
        {QStringLiteral("relationRef"),
         QString::fromStdString(
             relationRepairRef(
                 scenario, relationIndex))},
        {QStringLiteral("idCount"),
         static_cast<qint64>(relationIdCount)},
        {QStringLiteral("addressable"), addressable},
        {QStringLiteral("identityIssues"), identityIssues},
        {QStringLiteral("source"), source},
        {QStringLiteral("target"), target},
        {QStringLiteral("minimumDelayTick"),
         integerValue(relation.minimumDelay)},
        {QStringLiteral("maximumDelayTick"),
         integerValue(relation.maximumDelay)},
        {QStringLiteral("observedDelayTick"),
         observedDelay
             ? integerValue(*observedDelay)
             : QJsonValue{QJsonValue::Null}},
        {QStringLiteral("timingWithinRange"),
         observedDelay
             ? QJsonValue{
                   *observedDelay >= relation.minimumDelay
                   && *observedDelay <= relation.maximumDelay}
             : QJsonValue{QJsonValue::Null}},
        {QStringLiteral("clockDomainId"),
         QString::fromStdString(relation.clockDomainId)},
        {QStringLiteral("clockDomainName"),
         clock ? QString::fromStdString(clock->name) : QString{}},
        {QStringLiteral("condition"),
         QString::fromStdString(relation.condition)},
        {QStringLiteral("severity"),
         QString::fromLatin1(
             toString(relation.severity).data())},
        {QStringLiteral("description"),
         QString::fromStdString(relation.description)},
        {QStringLiteral("endpointsReady"), endpointsReady},
    };
}

struct EndpointClockRepairResolution {
    std::size_t eventIdCount{0};
    std::size_t laneIdCount{0};
    std::size_t clockDomainMatchCount{0};
    std::string effectiveClockDomainId;
    bool ready{false};
};

EndpointClockRepairResolution
resolveEndpointClockForRelationRepair(
    const Project& project,
    const Scenario& scenario,
    const std::string_view eventId)
{
    EndpointClockRepairResolution resolution;
    const Event* event = nullptr;
    for (const auto& candidate : scenario.events) {
        if (candidate.id != eventId) continue;
        event = &candidate;
        ++resolution.eventIdCount;
    }
    if (eventId.empty()
        || resolution.eventIdCount != 1
        || !event) {
        return resolution;
    }

    const Lane* lane = nullptr;
    for (const auto& candidate : scenario.lanes) {
        if (candidate.id != event->laneId) continue;
        lane = &candidate;
        ++resolution.laneIdCount;
    }
    if (resolution.laneIdCount != 1
        || !lane
        || lane->kind == LaneKind::Clock
        || lane->kind == LaneKind::Group) {
        return resolution;
    }

    resolution.effectiveClockDomainId =
        !event->clockDomainId.empty()
        ? event->clockDomainId
        : lane->clockDomainId;
    if (resolution.effectiveClockDomainId.empty()) {
        resolution.ready = true;
        return resolution;
    }
    resolution.clockDomainMatchCount =
        static_cast<std::size_t>(
            std::count_if(
                project.clockDomains.begin(),
                project.clockDomains.end(),
                [&resolution](const ClockDomain& clock) {
                    return clock.id
                        == resolution
                               .effectiveClockDomainId;
                }));
    resolution.ready =
        resolution.clockDomainMatchCount == 1;
    return resolution;
}

struct RelationClockRepairResolution {
    std::size_t currentClockMatchCount{0};
    EndpointClockRepairResolution source;
    EndpointClockRepairResolution target;
    std::string replacementClockDomainId;
    QString action{QStringLiteral("none")};
    bool invalid{false};
    bool endpointClockConflict{false};
    bool repairable{false};
};

RelationClockRepairResolution
resolveRelationClockRepair(
    const Project& project,
    const Scenario& scenario,
    const Relation& relation)
{
    RelationClockRepairResolution resolution;
    if (!relation.clockDomainId.empty()) {
        resolution.currentClockMatchCount =
            static_cast<std::size_t>(
                std::count_if(
                    project.clockDomains.begin(),
                    project.clockDomains.end(),
                    [&relation](const ClockDomain& clock) {
                        return clock.id
                            == relation.clockDomainId;
                    }));
    }
    resolution.invalid =
        !relation.clockDomainId.empty()
        && resolution.currentClockMatchCount != 1;
    resolution.source =
        resolveEndpointClockForRelationRepair(
            project,
            scenario,
            relation.sourceEventId);
    resolution.target =
        resolveEndpointClockForRelationRepair(
            project,
            scenario,
            relation.targetEventId);
    resolution.endpointClockConflict =
        resolution.source.ready
        && resolution.target.ready
        && !resolution.source
                .effectiveClockDomainId.empty()
        && !resolution.target
                .effectiveClockDomainId.empty()
        && resolution.source
               .effectiveClockDomainId
            != resolution.target
                   .effectiveClockDomainId;
    if (!resolution.invalid
        || resolution.currentClockMatchCount != 0
        || !resolution.source.ready
        || !resolution.target.ready
        || resolution.endpointClockConflict) {
        return resolution;
    }

    resolution.replacementClockDomainId =
        !resolution.source
             .effectiveClockDomainId.empty()
        ? resolution.source
              .effectiveClockDomainId
        : resolution.target
              .effectiveClockDomainId;
    resolution.action =
        resolution.replacementClockDomainId.empty()
        ? QStringLiteral("clear-relation-clock")
        : QStringLiteral("use-endpoint-clock");
    resolution.repairable = true;
    return resolution;
}

QJsonObject relationClockValidationObject(
    const Project& project,
    const Scenario& scenario,
    const Relation& relation,
    const std::size_t relationIdCount)
{
    const auto resolution =
        resolveRelationClockRepair(
            project, scenario, relation);
    return QJsonObject{
        {QStringLiteral("relationId"),
         QString::fromStdString(relation.id)},
        {QStringLiteral("idCount"),
         static_cast<qint64>(relationIdCount)},
        {QStringLiteral("addressable"),
         !relation.id.empty()
             && relationIdCount == 1},
        {QStringLiteral("clockDomainId"),
         QString::fromStdString(
             relation.clockDomainId)},
        {QStringLiteral("clockDomainMatchCount"),
         static_cast<qint64>(
             resolution.currentClockMatchCount)},
        {QStringLiteral("clockReferenceValid"),
         !resolution.invalid},
        {QStringLiteral("sourceEventIdCount"),
         static_cast<qint64>(
             resolution.source.eventIdCount)},
        {QStringLiteral("sourceLaneIdCount"),
         static_cast<qint64>(
             resolution.source.laneIdCount)},
        {QStringLiteral("sourceEffectiveClockDomainId"),
         QString::fromStdString(
             resolution.source
                 .effectiveClockDomainId)},
        {QStringLiteral("sourceClockDomainMatchCount"),
         static_cast<qint64>(
             resolution.source
                 .clockDomainMatchCount)},
        {QStringLiteral("sourceClockContextReady"),
         resolution.source.ready},
        {QStringLiteral("targetEventIdCount"),
         static_cast<qint64>(
             resolution.target.eventIdCount)},
        {QStringLiteral("targetLaneIdCount"),
         static_cast<qint64>(
             resolution.target.laneIdCount)},
        {QStringLiteral("targetEffectiveClockDomainId"),
         QString::fromStdString(
             resolution.target
                 .effectiveClockDomainId)},
        {QStringLiteral("targetClockDomainMatchCount"),
         static_cast<qint64>(
             resolution.target
                 .clockDomainMatchCount)},
        {QStringLiteral("targetClockContextReady"),
         resolution.target.ready},
        {QStringLiteral("endpointClockConflict"),
         resolution.endpointClockConflict},
        {QStringLiteral("clockRepairAction"),
         resolution.action},
        {QStringLiteral("replacementClockDomainId"),
         QString::fromStdString(
             resolution
                 .replacementClockDomainId)},
        {QStringLiteral("clockRepairable"),
         resolution.repairable
             && !relation.id.empty()
             && relationIdCount == 1},
    };
}

bool addRelationClockValidationContext(
    const Project& project,
    QJsonObject& issue,
    const std::optional<std::size_t> scenarioIndex)
{
    if (issue.value(QStringLiteral("code"))
            .toString()
            != QStringLiteral(
                "relation-clock-domain-invalid")
        || !scenarioIndex
        || *scenarioIndex
               >= project.scenarios.size()) {
        return false;
    }
    const auto& scenario =
        project.scenarios.at(*scenarioIndex);
    const auto relationId =
        issue.value(QStringLiteral("relationId"))
            .toString()
            .toStdString();
    issue.insert(
        QStringLiteral("objectKind"),
        QStringLiteral("relation"));
    const Relation* relation = nullptr;
    std::size_t relationIdCount = 0;
    std::size_t relationIndex = 0;
    for (std::size_t index = 0;
         index < scenario.relations.size();
         ++index) {
        if (scenario.relations.at(index).id
            != relationId) {
            continue;
        }
        relation =
            &scenario.relations.at(index);
        relationIndex = index;
        ++relationIdCount;
    }
    if (!relation || relationIdCount != 1) {
        return false;
    }

    const auto resolution =
        resolveRelationClockRepair(
            project, scenario, *relation);
    const auto path =
        QStringLiteral(
            "scenarios[%1].relations[%2].clockDomainId")
            .arg(
                static_cast<qulonglong>(
                    *scenarioIndex))
            .arg(
                static_cast<qulonglong>(
                    relationIndex));
    issue.insert(
        QStringLiteral("relationId"),
        QString::fromStdString(relation->id));
    issue.insert(
        QStringLiteral("relationRef"),
        QString::fromStdString(
            relationRepairRef(
                scenario, relationIndex)));
    issue.insert(
        QStringLiteral("objectKind"),
        QStringLiteral("relation"));
    issue.insert(
        QStringLiteral("scenarioIndex"),
        static_cast<qint64>(*scenarioIndex));
    issue.insert(
        QStringLiteral("relationIndex"),
        static_cast<qint64>(relationIndex));
    issue.insert(
        QStringLiteral("objectIndex"),
        static_cast<qint64>(relationIndex));
    issue.insert(QStringLiteral("path"), path);
    issue.insert(
        QStringLiteral("paths"),
        QJsonArray{path});
    issue.insert(
        QStringLiteral("repairProperties"),
        resolution.repairable
        ? QJsonArray{
              QStringLiteral("relation-clock"),
          }
        : QJsonArray{});
    issue.insert(
        QStringLiteral("repairOperations"),
        resolution.repairable
        ? QJsonArray{
              QStringLiteral(
                  "repair-relation-clock"),
          }
        : QJsonArray{});
    issue.insert(
        QStringLiteral("relationContext"),
        relationAutomationObject(
            project,
            scenario,
            *relation,
            relationIndex,
            relationIdCount));
    issue.insert(
        QStringLiteral("relationClockContext"),
        relationClockValidationObject(
            project,
            scenario,
            *relation,
            relationIdCount));
    return resolution.repairable;
}

bool addTraceMappingValidationContext(
    const Project& project,
    QJsonObject& issue)
{
    if (issue.value(QStringLiteral("code"))
            .toString()
            != QStringLiteral(
                "trace-mapping-invalid")) {
        return false;
    }
    const auto traceIndexValue =
        issue.value(
            QStringLiteral("traceIndex")).toInteger(-1);
    if (traceIndexValue < 0
        || static_cast<std::uint64_t>(
               traceIndexValue)
            >= project.importedTraces.size()) {
        return false;
    }
    const auto traceIndex =
        static_cast<std::size_t>(
            traceIndexValue);
    const auto laneId =
        issue.value(QStringLiteral("laneId"))
            .toString()
            .toStdString();
    const auto& trace =
        project.importedTraces.at(traceIndex);
    const auto mapping =
        trace.signalMapping.find(laneId);
    if (mapping == trace.signalMapping.end()) {
        return false;
    }
    const auto context =
        traceMappingValidationObject(
            project, traceIndex, laneId);
    if (!context.value(
            QStringLiteral("repairable")).toBool()) {
        return false;
    }
    issue.insert(
        QStringLiteral("traceId"),
        QString::fromStdString(trace.id));
    issue.insert(
        QStringLiteral("traceRef"),
        QString::fromStdString(
            traceRepairRef(
                project, traceIndex)));
    issue.insert(
        QStringLiteral("repairProperties"),
        QJsonArray{
            QStringLiteral("signal-mapping"),
        });
    issue.insert(
        QStringLiteral("repairOperations"),
        QJsonArray{
            QStringLiteral(
                "repair-trace-mapping"),
        });
    issue.insert(
        QStringLiteral("traceContext"),
        traceAutomationObject(
            project, traceIndex));
    issue.insert(
        QStringLiteral("traceMappingContext"),
        context);
    return true;
}

bool addTraceReferenceValidationContext(
    const Project& project,
    QJsonObject& issue)
{
    if (issue.value(QStringLiteral("code"))
            .toString()
            != QStringLiteral(
                "trace-reference-invalid")) {
        return false;
    }
    const auto traceIndexValue =
        issue.value(
            QStringLiteral("traceIndex")).toInteger(-1);
    if (traceIndexValue < 0
        || static_cast<std::uint64_t>(
               traceIndexValue)
            >= project.importedTraces.size()) {
        return false;
    }
    const auto traceIndex =
        static_cast<std::size_t>(
            traceIndexValue);
    const auto& trace =
        project.importedTraces.at(traceIndex);
    const auto context =
        traceReferenceValidationObject(
            project, traceIndex);
    if (!context.value(
            QStringLiteral("repairable")).toBool()) {
        return false;
    }
    QJsonArray repairProperties;
    const auto problems =
        context.value(
            QStringLiteral("problems")).toArray();
    if (problems.contains(
            QStringLiteral("empty-path"))) {
        repairProperties.append(
            QStringLiteral("path"));
    }
    if (problems.contains(
            QStringLiteral("unsupported-format"))) {
        repairProperties.append(
            QStringLiteral("format"));
    }
    issue.insert(
        QStringLiteral("traceId"),
        QString::fromStdString(trace.id));
    issue.insert(
        QStringLiteral("traceRef"),
        QString::fromStdString(
            traceRepairRef(
                project, traceIndex)));
    issue.insert(
        QStringLiteral("repairProperties"),
        repairProperties);
    issue.insert(
        QStringLiteral("repairOperations"),
        QJsonArray{
            QStringLiteral(
                "repair-trace-reference"),
        });
    issue.insert(
        QStringLiteral("traceContext"),
        traceAutomationObject(
            project, traceIndex));
    issue.insert(
        QStringLiteral("traceReferenceContext"),
        context);
    return true;
}

bool addTraceIdentityValidationContext(
    const Project& project,
    QJsonObject& issue)
{
    const auto code =
        issue.value(QStringLiteral("code")).toString();
    if ((code != QStringLiteral("missing-stable-id")
         && code
             != QStringLiteral("duplicate-stable-id"))
        || issue.value(
                 QStringLiteral("objectKind")).toString()
            != QStringLiteral("imported-trace")) {
        return false;
    }
    const auto traceIndexValue =
        issue.value(
            QStringLiteral("objectIndex")).toInteger(-1);
    if (traceIndexValue < 0
        || static_cast<std::uint64_t>(
               traceIndexValue)
            >= project.importedTraces.size()) {
        return false;
    }
    const auto traceIndex =
        static_cast<std::size_t>(
            traceIndexValue);
    const auto& trace =
        project.importedTraces.at(traceIndex);
    issue.insert(
        QStringLiteral("traceId"),
        QString::fromStdString(trace.id));
    issue.insert(
        QStringLiteral("traceIndex"),
        static_cast<qint64>(traceIndex));
    issue.insert(
        QStringLiteral("traceRef"),
        QString::fromStdString(
            traceRepairRef(
                project, traceIndex)));
    issue.insert(
        QStringLiteral("repairProperties"),
        QJsonArray{
            QStringLiteral("stable-id"),
        });
    issue.insert(
        QStringLiteral("repairOperations"),
        QJsonArray{
            QStringLiteral(
                "repair-trace-identity"),
        });
    issue.insert(
        QStringLiteral("traceContext"),
        traceAutomationObject(
            project, traceIndex));
    return true;
}

bool addValidationRepairReference(
    const Project& project,
    QJsonObject& issue,
    const std::optional<std::size_t> scenarioIndexHint =
        std::nullopt)
{
    std::optional<std::size_t> scenarioIndex =
        scenarioIndexHint;
    if (!scenarioIndex
        && issue.contains(QStringLiteral("scenarioIndex"))) {
        const auto value =
            issue.value(
                QStringLiteral("scenarioIndex")).toInteger(-1);
        if (value >= 0
            && static_cast<std::uint64_t>(value)
                < project.scenarios.size()) {
            scenarioIndex =
                static_cast<std::size_t>(value);
        }
    }
    if (!scenarioIndex) {
        const auto scenarioId =
            issue.value(
                QStringLiteral("scenarioId")).toString();
        for (std::size_t index = 0;
             index < project.scenarios.size();
             ++index) {
            if (QString::fromStdString(
                    project.scenarios.at(index).id)
                != scenarioId) {
                continue;
            }
            if (scenarioIndex) return false;
            scenarioIndex = index;
        }
    }
    if (!scenarioIndex
        || *scenarioIndex >= project.scenarios.size()) {
        return false;
    }
    const auto& scenario =
        project.scenarios.at(*scenarioIndex);
    const auto objectKind =
        issue.value(QStringLiteral("objectKind")).toString();
    if (objectKind == QStringLiteral("marker")) {
        const auto value =
            issue.value(
                QStringLiteral("objectIndex")).toInteger(-1);
        if (value < 0
            || static_cast<std::uint64_t>(value)
                >= scenario.markers.size()) {
            return false;
        }
        const auto markerIndex =
            static_cast<std::size_t>(value);
        issue.insert(
            QStringLiteral("markerId"),
            QString::fromStdString(
                scenario.markers.at(markerIndex).id));
        issue.insert(
            QStringLiteral("markerRef"),
            QString::fromStdString(
                markerRepairRef(
                    scenario, markerIndex)));
        issue.insert(
            QStringLiteral("repairOperations"),
            QJsonArray{
                QStringLiteral("update-marker"),
                QStringLiteral("delete-marker"),
            });
        return true;
    }

    const auto code =
        issue.value(QStringLiteral("code")).toString();
    const auto relationId =
        issue.value(QStringLiteral("relationId")).toString();
    if (objectKind != QStringLiteral("relation")
        && relationId.isEmpty()) {
        return false;
    }
    if (code == QStringLiteral("relation-satisfied")
        || code == QStringLiteral("relation-not-applicable")) {
        return false;
    }
    std::optional<std::size_t> relationIndex;
    if (objectKind == QStringLiteral("relation")
        && issue.contains(QStringLiteral("objectIndex"))) {
        const auto value =
            issue.value(
                QStringLiteral("objectIndex")).toInteger(-1);
        if (value >= 0
            && static_cast<std::uint64_t>(value)
                < scenario.relations.size()) {
            relationIndex =
                static_cast<std::size_t>(value);
        }
    } else if (!relationId.isEmpty()) {
        for (std::size_t index = 0;
             index < scenario.relations.size();
             ++index) {
            if (QString::fromStdString(
                    scenario.relations.at(index).id)
                != relationId) {
                continue;
            }
            if (relationIndex) return false;
            relationIndex = index;
        }
    }
    if (!relationIndex) return false;
    const auto& relation =
        scenario.relations.at(*relationIndex);
    const auto relationIdCount =
        static_cast<std::size_t>(
            std::count_if(
                scenario.relations.begin(),
                scenario.relations.end(),
                [&relation](const Relation& candidate) {
                    return candidate.id == relation.id;
                }));
    const auto relationContext =
        relationAutomationObject(
            project,
            scenario,
            relation,
            *relationIndex,
            relationIdCount);
    issue.insert(
        QStringLiteral("relationId"),
        QString::fromStdString(relation.id));
    issue.insert(
        QStringLiteral("relationRef"),
        QString::fromStdString(
            relationRepairRef(
                scenario, *relationIndex)));
    issue.insert(
        QStringLiteral("objectKind"),
        QStringLiteral("relation"));
    issue.insert(
        QStringLiteral("scenarioIndex"),
        static_cast<qint64>(*scenarioIndex));
    issue.insert(
        QStringLiteral("objectIndex"),
        static_cast<qint64>(*relationIndex));
    issue.insert(
        QStringLiteral("relationContext"),
        relationContext);

    const auto basePath =
        QStringLiteral("scenarios[%1].relations[%2]")
            .arg(
                static_cast<qulonglong>(
                    *scenarioIndex))
            .arg(
                static_cast<qulonglong>(
                    *relationIndex));
    QJsonArray repairProperties;
    QJsonArray paths;
    if (code == QStringLiteral("missing-stable-id")
        || code == QStringLiteral("duplicate-stable-id")) {
        repairProperties.append(
            QStringLiteral("stable-id"));
        paths.append(basePath + QStringLiteral(".id"));
    } else if (code == QStringLiteral(
                   "missing-source-event")) {
        repairProperties.append(
            QStringLiteral("source-endpoint"));
        paths.append(
            basePath + QStringLiteral(".sourceEventId"));
    } else if (code == QStringLiteral(
                   "missing-target-event")) {
        repairProperties.append(
            QStringLiteral("target-endpoint"));
        paths.append(
            basePath + QStringLiteral(".targetEventId"));
    } else if (code == QStringLiteral(
                   "multiple-possible-targets")) {
        const auto sourceReady =
            relationContext
                .value(QStringLiteral("source"))
                .toObject()
                .value(QStringLiteral("relationEndpoint"))
                .toBool();
        const auto targetReady =
            relationContext
                .value(QStringLiteral("target"))
                .toObject()
                .value(QStringLiteral("relationEndpoint"))
                .toBool();
        if (!sourceReady) {
            repairProperties.append(
                QStringLiteral("source-endpoint"));
            paths.append(
                basePath + QStringLiteral(".sourceEventId"));
        }
        if (!targetReady || sourceReady) {
            repairProperties.append(
                QStringLiteral("target-endpoint"));
            paths.append(
                basePath + QStringLiteral(".targetEventId"));
        }
    } else if (code == QStringLiteral(
                   "invalid-relation-condition")) {
        repairProperties.append(
            QStringLiteral("condition"));
        paths.append(
            basePath + QStringLiteral(".condition"));
    } else if (code == QStringLiteral(
                   "clock-domain-mismatch")
               || code == QStringLiteral(
                   "relation-clock-domain-invalid")) {
        repairProperties.append(
            QStringLiteral("clock"));
        paths.append(
            basePath + QStringLiteral(".clockDomainId"));
    } else if (code == QStringLiteral(
                   "insufficient-time-range")) {
        repairProperties.append(
            QStringLiteral("scenario-duration"));
        repairProperties.append(
            QStringLiteral("delay-range"));
        paths.append(
            QStringLiteral("scenarios[%1].durationTick")
                .arg(
                    static_cast<qulonglong>(
                        *scenarioIndex)));
        paths.append(
            basePath + QStringLiteral(".maximumDelayTick"));
    } else if (code == QStringLiteral(
                   "relation-violated")) {
        repairProperties.append(
            QStringLiteral("delay-range"));
        paths.append(
            basePath + QStringLiteral(".minimumDelayTick"));
        paths.append(
            basePath + QStringLiteral(".maximumDelayTick"));
    } else {
        repairProperties.append(
            QStringLiteral("relation"));
        paths.append(basePath);
    }
    if (!issue.contains(QStringLiteral("path"))
        && !paths.isEmpty()) {
        issue.insert(
            QStringLiteral("path"),
            paths.at(0));
    }
    issue.insert(
        QStringLiteral("paths"),
        paths);
    issue.insert(
        QStringLiteral("repairProperties"),
        repairProperties);
    issue.insert(
        QStringLiteral("repairOperations"),
        QJsonArray{
            QStringLiteral("update-relation"),
            QStringLiteral("delete-relation"),
        });
    return true;
}

struct LaneClockRepairResolution {
    std::size_t currentClockMatchCount{0};
    std::string replacementClockDomainId;
    QString action{QStringLiteral("none")};
    bool invalid{false};
    bool repairable{false};
};

LaneClockRepairResolution
resolveLaneClockRepair(
    const Project& project,
    const Lane& lane)
{
    LaneClockRepairResolution resolution;
    if (!lane.clockDomainId.empty()) {
        resolution.currentClockMatchCount =
            static_cast<std::size_t>(
                std::count_if(
                    project.clockDomains.begin(),
                    project.clockDomains.end(),
                    [&lane](const ClockDomain& clock) {
                        return clock.id
                            == lane.clockDomainId;
                    }));
    }
    const auto invalidGroupReference =
        lane.kind == LaneKind::Group
        && !lane.clockDomainId.empty();
    const auto invalidClockReference =
        lane.kind == LaneKind::Clock
        && (lane.clockDomainId.empty()
            || resolution
                   .currentClockMatchCount != 1);
    const auto invalidOptionalReference =
        lane.kind != LaneKind::Clock
        && lane.kind != LaneKind::Group
        && !lane.clockDomainId.empty()
        && resolution
               .currentClockMatchCount != 1;
    resolution.invalid =
        invalidGroupReference
        || invalidClockReference
        || invalidOptionalReference;
    if (!resolution.invalid) {
        return resolution;
    }

    if (lane.kind == LaneKind::Clock) {
        if (project.clockDomains.size() == 1
            && !project.clockDomains.front()
                    .id.empty()) {
            resolution.replacementClockDomainId =
                project.clockDomains.front().id;
            resolution.action =
                QStringLiteral(
                    "use-only-project-clock");
            resolution.repairable = true;
        }
    } else if (lane.kind == LaneKind::Group
               || resolution
                      .currentClockMatchCount == 0) {
        resolution.action =
            QStringLiteral("clear-lane-clock");
        resolution.repairable = true;
    }
    return resolution;
}

QJsonObject laneClockValidationObject(
    const Project& project,
    const Scenario& scenario,
    const Lane& lane,
    const std::size_t laneIdCount)
{
    const auto resolution =
        resolveLaneClockRepair(project, lane);
    std::size_t cycleEventCount = 0;
    std::size_t preservedCycleEventCount = 0;
    std::size_t clearedCycleEventCount = 0;
    for (const auto& event :
         scenario.events) {
        if (event.laneId != lane.id
            || !event.clockDomainId.empty()
            || !event.cycle) {
            continue;
        }
        ++cycleEventCount;
        if (!resolution.repairable) continue;
        const ClockDomain* clock = nullptr;
        std::size_t clockMatches = 0;
        for (const auto& candidate :
             project.clockDomains) {
            if (candidate.id
                != resolution
                       .replacementClockDomainId) {
                continue;
            }
            clock = &candidate;
            ++clockMatches;
        }
        const auto cycleTick =
            clockMatches == 1 && clock
            && *event.cycle >= 0
            ? tickAtCycle(
                  *clock,
                  *event.cycle,
                  clock->activeEdge)
            : std::optional<Tick>{};
        if (cycleTick
            && *cycleTick == event.tick) {
            ++preservedCycleEventCount;
        } else {
            ++clearedCycleEventCount;
        }
    }
    return QJsonObject{
        {QStringLiteral("laneId"),
         QString::fromStdString(lane.id)},
        {QStringLiteral("idCount"),
         static_cast<qint64>(laneIdCount)},
        {QStringLiteral("addressable"),
         !lane.id.empty() && laneIdCount == 1},
        {QStringLiteral("name"),
         QString::fromStdString(lane.name)},
        {QStringLiteral("kind"),
         QString::fromLatin1(
             toString(lane.kind).data())},
        {QStringLiteral("clockDomainId"),
         QString::fromStdString(
             lane.clockDomainId)},
        {QStringLiteral("clockDomainMatchCount"),
         static_cast<qint64>(
             resolution
                 .currentClockMatchCount)},
        {QStringLiteral("clockReferenceValid"),
         !resolution.invalid},
        {QStringLiteral("projectClockDomainCount"),
         static_cast<qint64>(
             project.clockDomains.size())},
        {QStringLiteral("clockRepairAction"),
         resolution.action},
        {QStringLiteral("replacementClockDomainId"),
         QString::fromStdString(
             resolution
                 .replacementClockDomainId)},
        {QStringLiteral("clockRepairable"),
         resolution.repairable
             && !lane.id.empty()
             && laneIdCount == 1},
        {QStringLiteral("cycleEventCount"),
         static_cast<qint64>(
             cycleEventCount)},
        {QStringLiteral("preservedCycleEventCount"),
         static_cast<qint64>(
             preservedCycleEventCount)},
        {QStringLiteral("clearedCycleEventCount"),
         static_cast<qint64>(
             clearedCycleEventCount)},
    };
}

bool addLaneClockValidationContext(
    const Project& project,
    QJsonObject& issue,
    const std::optional<std::size_t> scenarioIndex)
{
    if (issue.value(QStringLiteral("code"))
            .toString()
            != QStringLiteral(
                "lane-clock-domain-invalid")
        || !scenarioIndex
        || *scenarioIndex
               >= project.scenarios.size()) {
        return false;
    }
    const auto& scenario =
        project.scenarios.at(*scenarioIndex);
    const auto laneId =
        issue.value(QStringLiteral("laneId"))
            .toString()
            .toStdString();
    const Lane* lane = nullptr;
    std::size_t laneIdCount = 0;
    std::size_t laneIndex = 0;
    for (std::size_t index = 0;
         index < scenario.lanes.size();
         ++index) {
        if (scenario.lanes.at(index).id
            != laneId) {
            continue;
        }
        lane = &scenario.lanes.at(index);
        laneIndex = index;
        ++laneIdCount;
    }
    if (!lane || laneIdCount != 1) {
        return false;
    }

    const auto resolution =
        resolveLaneClockRepair(
            project, *lane);
    const auto path =
        QStringLiteral(
            "scenarios[%1].lanes[%2].clockDomainId")
            .arg(
                static_cast<qulonglong>(
                    *scenarioIndex))
            .arg(
                static_cast<qulonglong>(
                    laneIndex));
    issue.insert(
        QStringLiteral("objectKind"),
        QStringLiteral("lane"));
    issue.insert(
        QStringLiteral("scenarioIndex"),
        static_cast<qint64>(*scenarioIndex));
    issue.insert(
        QStringLiteral("laneIndex"),
        static_cast<qint64>(laneIndex));
    issue.insert(
        QStringLiteral("objectIndex"),
        static_cast<qint64>(laneIndex));
    issue.insert(
        QStringLiteral("path"), path);
    issue.insert(
        QStringLiteral("paths"),
        QJsonArray{path});
    issue.insert(
        QStringLiteral("repairProperties"),
        resolution.repairable
        ? QJsonArray{
              QStringLiteral("lane-clock"),
          }
        : QJsonArray{});
    issue.insert(
        QStringLiteral("repairOperations"),
        resolution.repairable
        ? QJsonArray{
              QStringLiteral(
                  "repair-lane-clock"),
          }
        : QJsonArray{});
    issue.insert(
        QStringLiteral("laneContext"),
        laneClockValidationObject(
            project,
            scenario,
            *lane,
            laneIdCount));
    return resolution.repairable;
}

struct LaneGroupRepairResolution {
    std::size_t groupMatchCount{0};
    std::size_t groupLaneMatchCount{0};
    std::optional<std::size_t> targetLaneIndex;
    const Lane* target{nullptr};
    bool targetIsSelf{false};
    bool invalid{false};
    bool repairable{false};
    QString action{QStringLiteral("none")};
};

LaneGroupRepairResolution
resolveLaneGroupRepair(
    const Scenario& scenario,
    const Lane& lane)
{
    LaneGroupRepairResolution resolution;
    if (lane.groupId.empty()) return resolution;

    for (std::size_t index = 0;
         index < scenario.lanes.size();
         ++index) {
        const auto& candidate =
            scenario.lanes.at(index);
        if (candidate.id != lane.groupId) continue;
        resolution.target = &candidate;
        resolution.targetLaneIndex = index;
        ++resolution.groupMatchCount;
        if (candidate.kind == LaneKind::Group) {
            ++resolution.groupLaneMatchCount;
        }
    }
    resolution.targetIsSelf =
        resolution.groupMatchCount == 1
        && resolution.target
        && resolution.target->id == lane.id;
    const auto valid =
        lane.kind != LaneKind::Group
        && resolution.groupMatchCount == 1
        && resolution.target
        && resolution.target->kind
            == LaneKind::Group
        && !resolution.targetIsSelf;
    resolution.invalid = !valid;
    resolution.repairable = resolution.invalid;
    if (resolution.repairable) {
        resolution.action =
            QStringLiteral("clear-lane-group");
    }
    return resolution;
}

QJsonObject laneGroupValidationObject(
    const Scenario& scenario,
    const Lane& lane,
    const std::size_t laneIdCount)
{
    const auto resolution =
        resolveLaneGroupRepair(
            scenario, lane);
    const auto targetResolved =
        resolution.groupMatchCount == 1
        && resolution.target;
    return QJsonObject{
        {QStringLiteral("laneId"),
         QString::fromStdString(lane.id)},
        {QStringLiteral("idCount"),
         static_cast<qint64>(laneIdCount)},
        {QStringLiteral("addressable"),
         !lane.id.empty()
             && laneIdCount == 1},
        {QStringLiteral("name"),
         QString::fromStdString(lane.name)},
        {QStringLiteral("kind"),
         QString::fromLatin1(
             toString(lane.kind).data())},
        {QStringLiteral("groupId"),
         QString::fromStdString(
             lane.groupId)},
        {QStringLiteral("groupIdMatchCount"),
         static_cast<qint64>(
             resolution.groupMatchCount)},
        {QStringLiteral("groupLaneMatchCount"),
         static_cast<qint64>(
             resolution.groupLaneMatchCount)},
        {QStringLiteral("groupReferenceValid"),
         !resolution.invalid},
        {QStringLiteral("targetResolved"),
         targetResolved},
        {QStringLiteral("targetLaneIndex"),
         targetResolved
             ? QJsonValue{
                   static_cast<qint64>(
                       *resolution
                            .targetLaneIndex)}
             : QJsonValue{
                   QJsonValue::Null}},
        {QStringLiteral("targetLaneName"),
         targetResolved
             ? QString::fromStdString(
                   resolution.target->name)
             : QString{}},
        {QStringLiteral("targetLaneKind"),
         targetResolved
             ? QString::fromLatin1(
                   toString(
                       resolution.target->kind)
                       .data())
             : QString{}},
        {QStringLiteral("targetIsSelf"),
         resolution.targetIsSelf},
        {QStringLiteral("groupRepairAction"),
         resolution.action},
        {QStringLiteral("replacementGroupId"),
         QString{}},
        {QStringLiteral("groupRepairable"),
         resolution.repairable
             && !lane.id.empty()
             && laneIdCount == 1},
    };
}

bool addLaneGroupValidationContext(
    const Project& project,
    QJsonObject& issue,
    const std::optional<std::size_t> scenarioIndex)
{
    if (issue.value(QStringLiteral("code"))
            .toString()
            != QStringLiteral(
                "lane-group-reference-invalid")
        || !scenarioIndex
        || *scenarioIndex
               >= project.scenarios.size()) {
        return false;
    }
    const auto& scenario =
        project.scenarios.at(*scenarioIndex);
    const auto laneId =
        issue.value(QStringLiteral("laneId"))
            .toString()
            .toStdString();
    const Lane* lane = nullptr;
    std::size_t laneIdCount = 0;
    std::size_t laneIndex = 0;
    for (std::size_t index = 0;
         index < scenario.lanes.size();
         ++index) {
        if (scenario.lanes.at(index).id
            != laneId) {
            continue;
        }
        lane = &scenario.lanes.at(index);
        laneIndex = index;
        ++laneIdCount;
    }
    if (!lane || laneIdCount != 1) {
        return false;
    }

    const auto resolution =
        resolveLaneGroupRepair(
            scenario, *lane);
    const auto path =
        QStringLiteral(
            "scenarios[%1].lanes[%2].groupId")
            .arg(
                static_cast<qulonglong>(
                    *scenarioIndex))
            .arg(
                static_cast<qulonglong>(
                    laneIndex));
    issue.insert(
        QStringLiteral("objectKind"),
        QStringLiteral("lane"));
    issue.insert(
        QStringLiteral("scenarioIndex"),
        static_cast<qint64>(
            *scenarioIndex));
    issue.insert(
        QStringLiteral("laneIndex"),
        static_cast<qint64>(
            laneIndex));
    issue.insert(
        QStringLiteral("objectIndex"),
        static_cast<qint64>(
            laneIndex));
    issue.insert(
        QStringLiteral("path"), path);
    issue.insert(
        QStringLiteral("paths"),
        QJsonArray{path});
    issue.insert(
        QStringLiteral("repairProperties"),
        resolution.repairable
        ? QJsonArray{
              QStringLiteral("lane-group"),
          }
        : QJsonArray{});
    issue.insert(
        QStringLiteral("repairOperations"),
        resolution.repairable
        ? QJsonArray{
              QStringLiteral(
                  "repair-lane-group"),
          }
        : QJsonArray{});
    issue.insert(
        QStringLiteral("laneContext"),
        laneGroupValidationObject(
            scenario,
            *lane,
            laneIdCount));
    return resolution.repairable;
}

bool addWaveformValidationContext(
    const Project& project,
    QJsonObject& issue,
    const std::optional<std::size_t> scenarioIndex)
{
    const auto code =
        issue.value(QStringLiteral("code")).toString();
    if ((code != QStringLiteral("invalid-bus-value")
         && code != QStringLiteral("undefined-region"))
        || !scenarioIndex
        || *scenarioIndex >= project.scenarios.size()) {
        return false;
    }

    const auto& scenario =
        project.scenarios.at(*scenarioIndex);
    const auto laneId =
        issue.value(QStringLiteral("laneId")).toString();
    std::optional<std::size_t> laneIndex;
    for (std::size_t index = 0;
         index < scenario.lanes.size();
         ++index) {
        if (QString::fromStdString(
                scenario.lanes.at(index).id)
            != laneId) {
            continue;
        }
        if (laneIndex) return false;
        laneIndex = index;
    }
    if (!laneIndex) return false;

    QString tickError;
    const auto issueTick =
        integerField(
            issue,
            QStringLiteral("tick"),
            true,
            tickError);
    if (!issueTick) return false;

    const auto& lane =
        scenario.lanes.at(*laneIndex);
    Tick start = *issueTick;
    Tick end = start;
    std::optional<std::size_t> segmentIndex;
    if (code == QStringLiteral("invalid-bus-value")) {
        for (std::size_t index = 0;
             index < lane.segments.size();
             ++index) {
            const auto& segment =
                lane.segments.at(index);
            if (segment.start != start
                || validateLaneValue(
                       lane, segment.value).valid) {
                continue;
            }
            if (segmentIndex) return false;
            segmentIndex = index;
        }
        if (!segmentIndex) return false;
        end = lane.segments.at(*segmentIndex).end;
    } else {
        std::optional<std::pair<Tick, Tick>> gap;
        Tick cursor = 0;
        for (const auto& segment : lane.segments) {
            if (segment.start > cursor
                && cursor == start) {
                gap = std::pair{cursor, segment.start};
                break;
            }
            cursor = std::max(cursor, segment.end);
        }
        if (!gap
            && cursor < scenario.duration
            && cursor == start) {
            gap = std::pair{cursor, scenario.duration};
        }
        if (!gap) return false;
        start = gap->first;
        end = gap->second;
    }

    const auto lanePath =
        QStringLiteral("scenarios[%1].lanes[%2]")
            .arg(
                static_cast<qulonglong>(
                    *scenarioIndex))
            .arg(
                static_cast<qulonglong>(
                    *laneIndex));
    const auto path =
        segmentIndex
        ? lanePath
              + QStringLiteral(".segments[%1].value")
                    .arg(
                        static_cast<qulonglong>(
                            *segmentIndex))
        : lanePath + QStringLiteral(".segments");
    QJsonObject context{
        {QStringLiteral("laneId"),
         QString::fromStdString(lane.id)},
        {QStringLiteral("laneName"),
         QString::fromStdString(lane.name)},
        {QStringLiteral("kind"),
         QString::fromLatin1(
             toString(lane.kind).data())},
        {QStringLiteral("width"),
         static_cast<qint64>(lane.width)},
        {QStringLiteral("signed"), lane.isSigned},
        {QStringLiteral("radix"),
         QString::fromLatin1(
             toString(lane.radix).data())},
        {QStringLiteral("startTick"),
         integerValue(start)},
        {QStringLiteral("endTick"),
         integerValue(end)},
        {QStringLiteral("start"),
         QString::fromStdString(
             formatTick(start, project.timeBase))},
        {QStringLiteral("end"),
         QString::fromStdString(
             formatTick(end, project.timeBase))},
        {QStringLiteral("undefined"),
         !segmentIndex.has_value()},
    };
    if (segmentIndex) {
        const auto& segment =
            lane.segments.at(*segmentIndex);
        context.insert(
            QStringLiteral("segmentId"),
            QString::fromStdString(segment.id));
        context.insert(
            QStringLiteral("value"),
            QString::fromStdString(segment.value));
    }

    issue.insert(
        QStringLiteral("objectKind"),
        segmentIndex
        ? QStringLiteral("segment")
        : QStringLiteral("lane"));
    issue.insert(
        QStringLiteral("scenarioIndex"),
        static_cast<qint64>(*scenarioIndex));
    issue.insert(
        QStringLiteral("laneIndex"),
        static_cast<qint64>(*laneIndex));
    issue.insert(
        QStringLiteral("objectIndex"),
        static_cast<qint64>(
            segmentIndex.value_or(*laneIndex)));
    issue.insert(QStringLiteral("path"), path);
    issue.insert(
        QStringLiteral("paths"),
        QJsonArray{path});
    issue.insert(
        QStringLiteral("repairProperties"),
        QJsonArray{
            segmentIndex
                ? QStringLiteral("value")
                : QStringLiteral("waveform-range"),
        });
    issue.insert(
        QStringLiteral("repairOperations"),
        segmentIndex
        ? QJsonArray{
              QStringLiteral("set-range"),
              QStringLiteral("clear-range"),
          }
        : QJsonArray{
              QStringLiteral("set-range"),
          });
    issue.insert(
        QStringLiteral("repairRange"),
        QJsonObject{
            {QStringLiteral("laneId"),
             QString::fromStdString(lane.id)},
            {QStringLiteral("startTick"),
             integerValue(start)},
            {QStringLiteral("endTick"),
             integerValue(end)},
        });
    issue.insert(
        QStringLiteral("waveformContext"),
        context);
    return true;
}

bool eventActionControlsWaveform(
    const EventAction action)
{
    return action == EventAction::Drive
        || action == EventAction::Expect
        || action == EventAction::Pulse
        || action == EventAction::Toggle;
}

struct EventLinkResolution {
    const Lane* lane{nullptr};
    const Segment* segment{nullptr};
    std::size_t segmentMatchCount{0};
    std::size_t linkedEventCount{0};
    bool actionSupported{false};
    bool segmentValueValid{false};
    std::string normalizedSegmentValue;

    [[nodiscard]] bool repairable(
        const Event& event) const noexcept
    {
        return event.waveformLinked
            && !event.linkedSegmentId.empty()
            && segmentMatchCount == 1
            && linkedEventCount == 1
            && lane
            && segment
            && lane->kind != LaneKind::Clock
            && lane->kind != LaneKind::Group
            && actionSupported
            && segmentValueValid;
    }

    [[nodiscard]] bool consistent(
        const Event& event) const noexcept
    {
        return repairable(event)
            && event.laneId == lane->id
            && event.tick == segment->start
            && event.value == normalizedSegmentValue;
    }
};

EventLinkResolution resolveEventLink(
    const Scenario& scenario,
    const Event& event)
{
    EventLinkResolution resolution;
    resolution.actionSupported =
        eventActionControlsWaveform(event.action);
    if (event.linkedSegmentId.empty()) {
        return resolution;
    }
    for (const auto& lane : scenario.lanes) {
        for (const auto& segment : lane.segments) {
            if (segment.id
                != event.linkedSegmentId) {
                continue;
            }
            resolution.lane = &lane;
            resolution.segment = &segment;
            ++resolution.segmentMatchCount;
        }
    }
    resolution.linkedEventCount =
        static_cast<std::size_t>(
            std::count_if(
                scenario.events.begin(),
                scenario.events.end(),
                [&event](const Event& candidate) {
                    return candidate.waveformLinked
                        && candidate.linkedSegmentId
                            == event.linkedSegmentId;
                }));
    if (resolution.segmentMatchCount == 1
        && resolution.lane
        && resolution.segment) {
        const auto validation =
            validateLaneValue(
                *resolution.lane,
                resolution.segment->value);
        resolution.segmentValueValid =
            validation.valid;
        resolution.normalizedSegmentValue =
            validation.normalizedValue;
    }
    return resolution;
}

struct EventCycleResolution {
    std::string effectiveClockDomainId;
    QString clockSource{QStringLiteral("none")};
    const ClockDomain* clock{nullptr};
    std::size_t clockMatchCount{0};
    std::optional<Tick> expectedTick;

    [[nodiscard]] bool consistent(
        const Event& event) const noexcept
    {
        return !event.cycle
            || (*event.cycle >= 0
                && clockMatchCount == 1
                && clock
                && expectedTick
                && *expectedTick == event.tick);
    }
};

EventCycleResolution resolveEventCycle(
    const Project& project,
    const Scenario& scenario,
    const Event& event)
{
    EventCycleResolution resolution;
    if (!event.clockDomainId.empty()) {
        resolution.effectiveClockDomainId =
            event.clockDomainId;
        resolution.clockSource =
            QStringLiteral("event");
    } else {
        const auto* lane =
            findLane(scenario, event.laneId);
        if (lane
            && !lane->clockDomainId.empty()) {
            resolution.effectiveClockDomainId =
                lane->clockDomainId;
            resolution.clockSource =
                QStringLiteral("lane");
        }
    }
    for (const auto& candidate :
         project.clockDomains) {
        if (candidate.id
            != resolution.effectiveClockDomainId) {
            continue;
        }
        resolution.clock = &candidate;
        ++resolution.clockMatchCount;
    }
    if (event.cycle
        && *event.cycle >= 0
        && resolution.clockMatchCount == 1
        && resolution.clock) {
        resolution.expectedTick =
            tickAtCycle(
                *resolution.clock,
                *event.cycle,
                resolution.clock->activeEdge);
    }
    return resolution;
}

struct EventClockRepairResolution {
    const Lane* lane{nullptr};
    std::size_t laneMatchCount{0};
    std::size_t currentClockMatchCount{0};
    std::size_t laneClockMatchCount{0};
    std::string replacementClockDomainId;
    QString action{QStringLiteral("none")};

    [[nodiscard]] bool repairable(
        const Event& event) const noexcept
    {
        return !event.clockDomainId.empty()
            && currentClockMatchCount == 0
            && laneMatchCount <= 1
            && (!lane
                || lane->clockDomainId.empty()
                || laneClockMatchCount == 1);
    }
};

EventClockRepairResolution
resolveEventClockRepair(
    const Project& project,
    const Scenario& scenario,
    const Event& event)
{
    EventClockRepairResolution resolution;
    for (const auto& clock :
         project.clockDomains) {
        if (clock.id == event.clockDomainId) {
            ++resolution.currentClockMatchCount;
        }
    }
    if (!event.laneId.empty()) {
        for (const auto& lane :
             scenario.lanes) {
            if (lane.id != event.laneId) {
                continue;
            }
            resolution.lane = &lane;
            ++resolution.laneMatchCount;
        }
    }
    if (resolution.laneMatchCount == 1
        && resolution.lane
        && !resolution.lane
                ->clockDomainId.empty()) {
        for (const auto& clock :
             project.clockDomains) {
            if (clock.id
                == resolution.lane
                       ->clockDomainId) {
                ++resolution
                      .laneClockMatchCount;
            }
        }
        if (resolution.laneClockMatchCount == 1) {
            resolution.replacementClockDomainId =
                resolution.lane->clockDomainId;
            resolution.action =
                QStringLiteral("use-lane-clock");
        }
    } else if (resolution.laneMatchCount <= 1) {
        resolution.action =
            QStringLiteral("clear-event-clock");
    }
    if (!resolution.repairable(event)) {
        resolution.action =
            QStringLiteral("none");
        resolution
            .replacementClockDomainId.clear();
    }
    return resolution;
}

QJsonObject eventValidationObject(
    const Project& project,
    const Scenario& scenario,
    const Event& event,
    const std::size_t eventIdCount)
{
    const auto* lane =
        findLane(scenario, event.laneId);
    const auto* clock =
        event.clockDomainId.empty()
        ? nullptr
        : findClock(project, event.clockDomainId);
    const auto linkedSegmentResolved =
        lane
        && !event.linkedSegmentId.empty()
        && std::any_of(
            lane->segments.begin(),
            lane->segments.end(),
            [&event](const Segment& segment) {
                return segment.id
                    == event.linkedSegmentId;
            });
    const auto link =
        resolveEventLink(scenario, event);
    const auto cycle =
        resolveEventCycle(project, scenario, event);
    const auto clockRepair =
        resolveEventClockRepair(
            project, scenario, event);
    QJsonObject context{
        {QStringLiteral("eventId"),
         QString::fromStdString(event.id)},
        {QStringLiteral("idCount"),
         static_cast<qint64>(eventIdCount)},
        {QStringLiteral("addressable"),
         !event.id.empty() && eventIdCount == 1},
        {QStringLiteral("laneId"),
         QString::fromStdString(event.laneId)},
        {QStringLiteral("laneResolved"),
         lane != nullptr},
        {QStringLiteral("laneName"),
         lane
             ? QString::fromStdString(lane->name)
             : QString{}},
        {QStringLiteral("laneKind"),
         lane
             ? QString::fromLatin1(
                   toString(lane->kind).data())
             : QString{}},
        {QStringLiteral("timeTick"),
         integerValue(event.tick)},
        {QStringLiteral("time"),
         QString::fromStdString(
             formatTick(event.tick, project.timeBase))},
        {QStringLiteral("withinScenario"),
         event.tick >= 0
             && event.tick < scenario.duration},
        {QStringLiteral("scenarioDurationTick"),
         integerValue(scenario.duration)},
        {QStringLiteral("scenarioDuration"),
         QString::fromStdString(
             formatTick(
                 scenario.duration,
                 project.timeBase))},
        {QStringLiteral("action"),
         QString::fromLatin1(
             toString(event.action).data())},
        {QStringLiteral("value"),
         QString::fromStdString(event.value)},
        {QStringLiteral("expectedResult"),
         QString::fromStdString(
             event.expectedResult)},
        {QStringLiteral("clockDomainId"),
         QString::fromStdString(
             event.clockDomainId)},
        {QStringLiteral("clockDomainName"),
         clock
             && clockRepair
                    .currentClockMatchCount == 1
             ? QString::fromStdString(clock->name)
             : QString{}},
        {QStringLiteral("clockResolved"),
         event.clockDomainId.empty()
             || clockRepair
                    .currentClockMatchCount == 1},
        {QStringLiteral("description"),
         QString::fromStdString(event.description)},
        {QStringLiteral("waveformLinked"),
         event.waveformLinked},
        {QStringLiteral("linkedSegmentResolved"),
         linkedSegmentResolved},
        {QStringLiteral("linkedSegmentOwnerResolved"),
         link.segmentMatchCount == 1},
        {QStringLiteral("linkedSegmentMatchCount"),
         static_cast<qint64>(
             link.segmentMatchCount)},
        {QStringLiteral("linkedEventCount"),
         static_cast<qint64>(
             link.linkedEventCount)},
        {QStringLiteral("linkConsistent"),
         link.consistent(event)},
        {QStringLiteral("linkRepairable"),
         link.repairable(event)},
    };
    context.insert(
        QStringLiteral("linkedLaneId"),
        link.lane
            && link.segmentMatchCount == 1
        ? QString::fromStdString(link.lane->id)
        : QString{});
    context.insert(
        QStringLiteral("linkedLaneName"),
        link.lane
            && link.segmentMatchCount == 1
        ? QString::fromStdString(link.lane->name)
        : QString{});
    context.insert(
        QStringLiteral("linkedLaneKind"),
        link.lane
            && link.segmentMatchCount == 1
        ? QString::fromLatin1(
              toString(link.lane->kind).data())
        : QString{});
    context.insert(
        QStringLiteral("linkedLaneIndex"),
        link.lane
            && link.segmentMatchCount == 1
        ? QJsonValue{
              static_cast<qint64>(
                  link.lane
                  - scenario.lanes.data())}
        : QJsonValue{QJsonValue::Null});
    context.insert(
        QStringLiteral("linkedSegmentIndex"),
        link.lane
            && link.segment
            && link.segmentMatchCount == 1
        ? QJsonValue{
              static_cast<qint64>(
                  link.segment
                  - link.lane->segments.data())}
        : QJsonValue{QJsonValue::Null});
    context.insert(
        QStringLiteral("linkedSegmentStartTick"),
        link.segment
            && link.segmentMatchCount == 1
        ? integerValue(link.segment->start)
        : QJsonValue{QJsonValue::Null});
    context.insert(
        QStringLiteral("linkedSegmentEndTick"),
        link.segment
            && link.segmentMatchCount == 1
        ? integerValue(link.segment->end)
        : QJsonValue{QJsonValue::Null});
    context.insert(
        QStringLiteral("linkedSegmentStart"),
        link.segment
            && link.segmentMatchCount == 1
        ? QJsonValue{
              QString::fromStdString(
                  formatTick(
                      link.segment->start,
                      project.timeBase))}
        : QJsonValue{QJsonValue::Null});
    context.insert(
        QStringLiteral("linkedSegmentEnd"),
        link.segment
            && link.segmentMatchCount == 1
        ? QJsonValue{
              QString::fromStdString(
                  formatTick(
                      link.segment->end,
                      project.timeBase))}
        : QJsonValue{QJsonValue::Null});
    context.insert(
        QStringLiteral("linkedSegmentValue"),
        link.segment
            && link.segmentMatchCount == 1
        ? QJsonValue{
              QString::fromStdString(
                  link.segment->value)}
        : QJsonValue{QJsonValue::Null});
    context.insert(
        QStringLiteral("cycle"),
        event.cycle
            ? integerValue(*event.cycle)
            : QJsonValue{QJsonValue::Null});
    context.insert(
        QStringLiteral("cyclePresent"),
        event.cycle.has_value());
    context.insert(
        QStringLiteral("cycleClockSource"),
        cycle.clockSource);
    context.insert(
        QStringLiteral("effectiveClockDomainId"),
        QString::fromStdString(
            cycle.effectiveClockDomainId));
    context.insert(
        QStringLiteral("cycleClockMatchCount"),
        static_cast<qint64>(
            cycle.clockMatchCount));
    context.insert(
        QStringLiteral("cycleClockResolved"),
        cycle.clockMatchCount == 1
            && cycle.clock);
    context.insert(
        QStringLiteral("cycleExpectedTimeTick"),
        cycle.expectedTick
            ? integerValue(*cycle.expectedTick)
            : QJsonValue{QJsonValue::Null});
    context.insert(
        QStringLiteral("cycleExpectedTime"),
        cycle.expectedTick
            ? QJsonValue{
                  QString::fromStdString(
                      formatTick(
                          *cycle.expectedTick,
                          project.timeBase))}
            : QJsonValue{QJsonValue::Null});
    context.insert(
        QStringLiteral("cycleConsistent"),
        cycle.consistent(event));
    context.insert(
        QStringLiteral("cycleRepairable"),
        event.cycle.has_value()
            && !cycle.consistent(event)
            && !event.id.empty()
            && eventIdCount == 1);
    context.insert(
        QStringLiteral("clockDomainMatchCount"),
        static_cast<qint64>(
            clockRepair.currentClockMatchCount));
    context.insert(
        QStringLiteral("clockReferenceValid"),
        event.clockDomainId.empty()
            || clockRepair
                   .currentClockMatchCount == 1);
    context.insert(
        QStringLiteral("laneClockDomainId"),
        clockRepair.lane
            && clockRepair.laneMatchCount == 1
        ? QString::fromStdString(
              clockRepair.lane
                  ->clockDomainId)
        : QString{});
    context.insert(
        QStringLiteral("laneClockMatchCount"),
        static_cast<qint64>(
            clockRepair.laneClockMatchCount));
    context.insert(
        QStringLiteral("clockRepairAction"),
        clockRepair.action);
    context.insert(
        QStringLiteral("replacementClockDomainId"),
        QString::fromStdString(
            clockRepair
                .replacementClockDomainId));
    context.insert(
        QStringLiteral("clockRepairable"),
        clockRepair.repairable(event)
            && !event.id.empty()
            && eventIdCount == 1);
    return context;
}

bool addEventValidationContext(
    const Project& project,
    QJsonObject& issue,
    const std::optional<std::size_t> scenarioIndex)
{
    const auto code =
        issue.value(QStringLiteral("code")).toString();
    if ((code != QStringLiteral("missing-lane")
         && code != QStringLiteral(
             "event-outside-scenario")
         && code != QStringLiteral(
             "event-waveform-mismatch")
         && code != QStringLiteral(
             "event-cycle-mismatch")
         && code != QStringLiteral(
             "event-clock-domain-invalid"))
        || !scenarioIndex
        || *scenarioIndex >= project.scenarios.size()) {
        return false;
    }

    const auto& scenario =
        project.scenarios.at(*scenarioIndex);
    const auto eventId =
        issue.value(QStringLiteral("eventId"))
            .toString();
    if (eventId.isEmpty()) return false;
    QString eventError;
    const auto eventIndex =
        uniqueEventIndexById(
            scenario,
            eventId.toStdString(),
            eventError);
    if (!eventIndex) return false;

    const auto& event =
        scenario.events.at(*eventIndex);
    const auto lane =
        findLane(scenario, event.laneId);
    const auto link =
        resolveEventLink(scenario, event);
    const auto basePath =
        QStringLiteral("scenarios[%1].events[%2]")
            .arg(
                static_cast<qulonglong>(
                    *scenarioIndex))
            .arg(
                static_cast<qulonglong>(
                    *eventIndex));
    QJsonArray paths;
    const auto appendPath =
        [&paths](const QString& path) {
            if (!paths.contains(path)) {
                paths.append(path);
            }
        };
    if (code == QStringLiteral("missing-lane")) {
        appendPath(
            basePath
            + QStringLiteral(".laneId"));
    } else if (code
               == QStringLiteral(
                   "event-outside-scenario")) {
        appendPath(
            basePath
            + QStringLiteral(".timeTick"));
    } else if (code == QStringLiteral(
                   "event-waveform-mismatch")) {
        if (event.linkedSegmentId.empty()
            || link.segmentMatchCount != 1
            || link.linkedEventCount != 1) {
            appendPath(
                basePath
                + QStringLiteral(
                    ".linkedSegmentId"));
        }
        if (link.segmentMatchCount == 1
            && link.lane
            && link.segment) {
            if (event.laneId != link.lane->id) {
                appendPath(
                    basePath
                    + QStringLiteral(".laneId"));
            }
            if (event.tick
                != link.segment->start) {
                appendPath(
                    basePath
                    + QStringLiteral(".timeTick"));
            }
            if (link.segmentValueValid
                && event.value
                    != link.normalizedSegmentValue) {
                appendPath(
                    basePath
                    + QStringLiteral(".value"));
            }
        }
        if (!link.actionSupported) {
            appendPath(
                basePath
                + QStringLiteral(".action"));
        }
        if (paths.isEmpty()) {
            appendPath(
                basePath
                + QStringLiteral(
                    ".linkedSegmentId"));
        }
    } else if (code == QStringLiteral(
                   "event-cycle-mismatch")) {
        appendPath(
            basePath
            + QStringLiteral(".cycle"));
        const auto cycle =
            resolveEventCycle(
                project, scenario, event);
        if (!event.clockDomainId.empty()
            && cycle.clockMatchCount != 1) {
            appendPath(
                basePath
                + QStringLiteral(
                    ".clockDomainId"));
        }
        if (cycle.expectedTick
            && *cycle.expectedTick != event.tick) {
            appendPath(
                basePath
                + QStringLiteral(".timeTick"));
        }
    } else {
        appendPath(
            basePath
            + QStringLiteral(
                ".clockDomainId"));
    }
    QJsonArray repairProperties;
    QJsonArray repairOperations;
    if (code == QStringLiteral(
            "event-cycle-mismatch")) {
        repairProperties.append(
            QStringLiteral("event-cycle"));
        repairOperations.append(
            QStringLiteral("clear-event-cycle"));
    } else if (code == QStringLiteral(
                   "event-clock-domain-invalid")) {
        const auto clockRepair =
            resolveEventClockRepair(
                project, scenario, event);
        if (clockRepair.repairable(event)) {
            repairProperties.append(
                QStringLiteral(
                    "event-clock"));
            repairOperations.append(
                QStringLiteral(
                    "repair-event-clock"));
        }
    } else if (link.repairable(event)
               && !link.consistent(event)) {
        repairProperties.append(
            QStringLiteral("event-link"));
        repairOperations.append(
            QStringLiteral("repair-event-link"));
    }
    if (code == QStringLiteral("event-outside-scenario")
        && event.tick >= 0
        && event.tick
            < std::numeric_limits<Tick>::max()) {
        repairProperties.append(
            QStringLiteral("scenario-duration"));
        repairOperations.append(
            QStringLiteral("set-duration"));
        issue.insert(
            QStringLiteral("minimumDurationTick"),
            integerValue(event.tick + 1));
    }
    if (code != QStringLiteral(
            "event-cycle-mismatch")
        && code != QStringLiteral(
            "event-clock-domain-invalid")) {
        repairProperties.append(
            QStringLiteral("event"));
        repairOperations.append(
            QStringLiteral("delete-event"));
    }

    issue.insert(
        QStringLiteral("objectKind"),
        QStringLiteral("event"));
    issue.insert(
        QStringLiteral("scenarioIndex"),
        static_cast<qint64>(*scenarioIndex));
    if (lane) {
        issue.insert(
            QStringLiteral("laneIndex"),
            static_cast<qint64>(
                lane - scenario.lanes.data()));
    }
    issue.insert(
        QStringLiteral("objectIndex"),
        static_cast<qint64>(*eventIndex));
    issue.insert(
        QStringLiteral("path"),
        paths.first());
    issue.insert(
        QStringLiteral("paths"),
        paths);
    issue.insert(
        QStringLiteral("repairProperties"),
        repairProperties);
    issue.insert(
        QStringLiteral("repairOperations"),
        repairOperations);
    issue.insert(
        QStringLiteral("eventContext"),
        eventValidationObject(
            project, scenario, event, 1));
    return !repairOperations.isEmpty();
}

bool uniqueRelationIdAtIndex(
    const Scenario& scenario,
    const std::string& id,
    const std::size_t ignoredIndex)
{
    for (std::size_t index = 0;
         index < scenario.relations.size();
         ++index) {
        if (index != ignoredIndex
            && scenario.relations.at(index).id == id) {
            return false;
        }
    }
    return true;
}

bool relationEquivalent(
    Relation left,
    Relation right)
{
    left.id.clear();
    right.id.clear();
    return left == right;
}

void relationReport(
    const Scenario& scenario,
    const Relation& relation,
    QJsonObject& result)
{
    const auto* source = findEvent(scenario, relation.sourceEventId);
    const auto* target = findEvent(scenario, relation.targetEventId);
    result.insert(
        QStringLiteral("relationId"),
        QString::fromStdString(relation.id));
    result.insert(
        QStringLiteral("sourceLaneId"),
        QString::fromStdString(source ? source->laneId : std::string{}));
    result.insert(
        QStringLiteral("sourceAtTick"),
        source ? integerValue(source->tick)
               : QJsonValue{QJsonValue::Null});
    result.insert(
        QStringLiteral("targetLaneId"),
        QString::fromStdString(target ? target->laneId : std::string{}));
    result.insert(
        QStringLiteral("targetAtTick"),
        target ? integerValue(target->tick)
               : QJsonValue{QJsonValue::Null});
    result.insert(
        QStringLiteral("minimumDelayTick"),
        integerValue(relation.minimumDelay));
    result.insert(
        QStringLiteral("maximumDelayTick"),
        integerValue(relation.maximumDelay));
    result.insert(
        QStringLiteral("clockId"),
        QString::fromStdString(relation.clockDomainId));
    result.insert(
        QStringLiteral("severity"),
        QString::fromLatin1(toString(relation.severity).data()));
}

void markerReport(
    const Marker& marker,
    QJsonObject& result)
{
    result.insert(
        QStringLiteral("markerId"),
        QString::fromStdString(marker.id));
    result.insert(
        QStringLiteral("name"),
        QString::fromStdString(marker.name));
    result.insert(
        QStringLiteral("startTick"),
        integerValue(marker.start));
    result.insert(
        QStringLiteral("endTick"),
        integerValue(marker.end));
    result.insert(
        QStringLiteral("kind"),
        QString::fromLatin1(toString(marker.kind).data()));
    result.insert(
        QStringLiteral("note"),
        QString::fromStdString(marker.note));
}

bool applyAddRelation(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("id"),
             QStringLiteral("sourceLaneId"),
             QStringLiteral("sourceAtTick"),
             QStringLiteral("sourceAt"),
             QStringLiteral("targetLaneId"),
             QStringLiteral("targetAtTick"),
             QStringLiteral("targetAt"),
             QStringLiteral("minimumDelayTick"),
             QStringLiteral("minimumDelay"),
             QStringLiteral("minimumDelayCycles"),
             QStringLiteral("maximumDelayTick"),
             QStringLiteral("maximumDelay"),
             QStringLiteral("maximumDelayCycles"),
             QStringLiteral("clockId"),
             QStringLiteral("condition"),
             QStringLiteral("severity"),
             QStringLiteral("description")},
            error)) {
        return false;
    }

    const auto sourceLaneId = requiredString(
        operation, QStringLiteral("sourceLaneId"), error);
    const auto targetLaneId = requiredString(
        operation, QStringLiteral("targetLaneId"), error);
    if (!sourceLaneId || !targetLaneId) return false;
    const auto* sourceLane = findLane(scenario, *sourceLaneId);
    const auto* targetLane = findLane(scenario, *targetLaneId);
    if (!sourceLane || !targetLane
        || sourceLane->kind == LaneKind::Clock
        || sourceLane->kind == LaneKind::Group
        || targetLane->kind == LaneKind::Clock
        || targetLane->kind == LaneKind::Group) {
        error = QStringLiteral(
            "Relation endpoints must be existing non-Clock signal lanes.");
        return false;
    }

    std::optional<std::string> clockId;
    if (!relationClockContext(
            project,
            scenario,
            operation,
            *sourceLaneId,
            *targetLaneId,
            clockId,
            error)) {
        return false;
    }
    const auto sourceTick = automationTickField(
        project,
        operation,
        QStringLiteral("sourceAtTick"),
        QStringLiteral("sourceAt"),
        clockId,
        error);
    if (!sourceTick) return false;
    const auto targetTick = automationTickField(
        project,
        operation,
        QStringLiteral("targetAtTick"),
        QStringLiteral("targetAt"),
        clockId,
        error);
    if (!targetTick) return false;
    if (*sourceTick < 0 || *sourceTick >= scenario.duration
        || *targetTick < 0 || *targetTick >= scenario.duration) {
        error = QStringLiteral(
            "Relation endpoint time is outside the Scenario.");
        return false;
    }
    const auto* sourceEvent = waveformEventAt(
        scenario, *sourceLaneId, *sourceTick, error);
    if (!sourceEvent) return false;
    const auto* targetEvent = waveformEventAt(
        scenario, *targetLaneId, *targetTick, error);
    if (!targetEvent) return false;

    Relation relation;
    bool minimumProvided = false;
    bool maximumProvided = false;
    if (!relationDelayField(
            project,
            operation,
            QStringLiteral("minimumDelay"),
            clockId,
            true,
            relation.minimumDelay,
            minimumProvided,
            error)
        || !relationDelayField(
            project,
            operation,
            QStringLiteral("maximumDelay"),
            clockId,
            true,
            relation.maximumDelay,
            maximumProvided,
            error)) {
        return false;
    }
    if (!minimumProvided || !maximumProvided
        || relation.maximumDelay < relation.minimumDelay) {
        error = QStringLiteral(
            "Relation delay range is invalid.");
        return false;
    }
    relation.sourceEventId = sourceEvent->id;
    relation.targetEventId = targetEvent->id;
    relation.clockDomainId = clockId.value_or(std::string{});

    if (operation.contains(QStringLiteral("condition"))) {
        const auto value = operation.value(QStringLiteral("condition"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'condition' must be a string.");
            return false;
        }
        relation.condition = value.toString().trimmed().toStdString();
    }
    if (operation.contains(QStringLiteral("severity"))) {
        const auto value = operation.value(QStringLiteral("severity"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'severity' must be a string.");
            return false;
        }
        const auto severity = severityFromString(
            value.toString().trimmed().toLower().toStdString());
        if (!severity) {
            error = QStringLiteral(
                "Field 'severity' must be 'information', 'warning', or 'error'.");
            return false;
        }
        relation.severity = *severity;
    }
    if (operation.contains(QStringLiteral("description"))) {
        const auto value = operation.value(QStringLiteral("description"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'description' must be a string.");
            return false;
        }
        relation.description = value.toString().trimmed().toStdString();
    }

    if (operation.contains(QStringLiteral("id"))) {
        const auto id = requiredString(
            operation, QStringLiteral("id"), error);
        if (!id) return false;
        relation.id = *id;
    } else {
        std::string seed;
        appendIdentityField(seed, scenario.id);
        appendIdentityField(seed, *sourceLaneId);
        appendIdentityField(seed, std::to_string(*sourceTick));
        appendIdentityField(seed, *targetLaneId);
        appendIdentityField(seed, std::to_string(*targetTick));
        appendIdentityField(
            seed, std::to_string(relation.minimumDelay));
        appendIdentityField(
            seed, std::to_string(relation.maximumDelay));
        appendIdentityField(seed, relation.clockDomainId);
        appendIdentityField(seed, relation.condition);
        appendIdentityField(seed, toString(relation.severity));
        appendIdentityField(seed, relation.description);
        relation.id = deterministicStableId(
            "relation",
            seed,
            [&scenario](const std::string& id) {
                return findRelation(scenario, id) != nullptr;
            });
    }

    if (const auto* existing = findRelation(scenario, relation.id)) {
        if (!relationEquivalent(*existing, relation)) {
            error = QStringLiteral("Relation ID '%1' already exists.")
                        .arg(QString::fromStdString(relation.id));
            return false;
        }
        result.insert(QStringLiteral("changed"), false);
        relationReport(scenario, *existing, result);
        return true;
    }
    const auto duplicate = std::find_if(
        scenario.relations.begin(),
        scenario.relations.end(),
        [&relation](const Relation& existing) {
            return relationEquivalent(existing, relation);
        });
    if (duplicate != scenario.relations.end()) {
        result.insert(QStringLiteral("changed"), false);
        relationReport(scenario, *duplicate, result);
        return true;
    }

    const auto changed = stack.execute(
        std::make_unique<AddRelationCommand>(scenario, relation));
    result.insert(QStringLiteral("changed"), changed);
    const auto* added = findRelation(scenario, relation.id);
    relationReport(scenario, added ? *added : relation, result);
    return true;
}

bool applyUpdateRelation(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("relationId"),
             QStringLiteral("relationRef"),
             QStringLiteral("newId"),
             QStringLiteral("sourceLaneId"),
             QStringLiteral("sourceAtTick"),
             QStringLiteral("sourceAt"),
             QStringLiteral("targetLaneId"),
             QStringLiteral("targetAtTick"),
             QStringLiteral("targetAt"),
             QStringLiteral("minimumDelayTick"),
             QStringLiteral("minimumDelay"),
             QStringLiteral("minimumDelayCycles"),
             QStringLiteral("maximumDelayTick"),
             QStringLiteral("maximumDelay"),
             QStringLiteral("maximumDelayCycles"),
             QStringLiteral("clockId"),
             QStringLiteral("condition"),
             QStringLiteral("severity"),
             QStringLiteral("description")},
            error)) {
        return false;
    }
    const auto hasRelationId =
        operation.contains(QStringLiteral("relationId"));
    const auto hasRelationRef =
        operation.contains(QStringLiteral("relationRef"));
    if (hasRelationId == hasRelationRef) {
        error = QStringLiteral(
            "Use exactly one of 'relationId' or 'relationRef'.");
        return false;
    }
    std::optional<std::size_t> relationIndex;
    if (hasRelationId) {
        const auto relationId = requiredString(
            operation, QStringLiteral("relationId"), error);
        if (!relationId) return false;
        relationIndex = uniqueRelationIndexById(
            scenario, *relationId, error);
    } else {
        const auto relationRef = requiredString(
            operation, QStringLiteral("relationRef"), error);
        if (!relationRef) return false;
        relationIndex = resolveRelationRepairRef(
            scenario, *relationRef, error);
    }
    if (!relationIndex) return false;
    const auto existing = scenario.relations.at(*relationIndex);
    const auto relationLabel = existing.id.empty()
        ? QStringLiteral("<empty>")
        : QString::fromStdString(existing.id);

    const std::array<QString, 16> editableFields{
        QStringLiteral("newId"),
        QStringLiteral("sourceLaneId"),
        QStringLiteral("sourceAtTick"),
        QStringLiteral("sourceAt"),
        QStringLiteral("targetLaneId"),
        QStringLiteral("targetAtTick"),
        QStringLiteral("targetAt"),
        QStringLiteral("minimumDelayTick"),
        QStringLiteral("minimumDelay"),
        QStringLiteral("minimumDelayCycles"),
        QStringLiteral("maximumDelayTick"),
        QStringLiteral("maximumDelay"),
        QStringLiteral("maximumDelayCycles"),
        QStringLiteral("clockId"),
        QStringLiteral("condition"),
        QStringLiteral("severity"),
    };
    const auto hasDescription =
        operation.contains(QStringLiteral("description"));
    const auto hasEdit = hasDescription
        || std::any_of(
            editableFields.begin(),
            editableFields.end(),
            [&operation](const QString& field) {
                return operation.contains(field);
            });
    if (!hasEdit) {
        error = QStringLiteral(
            "update-relation contains no property change.");
        return false;
    }

    const auto sourceResolution =
        resolveRelationEndpoint(scenario, existing.sourceEventId);
    const auto targetResolution =
        resolveRelationEndpoint(scenario, existing.targetEventId);
    bool existingSourceReady = false;
    bool existingTargetReady = false;
    const auto sourceState = relationEndpointQueryObject(
        project, scenario, sourceResolution, existingSourceReady);
    const auto targetState = relationEndpointQueryObject(
        project, scenario, targetResolution, existingTargetReady);
    const auto sourceLaneProvided =
        operation.contains(QStringLiteral("sourceLaneId"));
    const auto targetLaneProvided =
        operation.contains(QStringLiteral("targetLaneId"));
    const auto sourceTimeFieldProvided =
        operation.contains(QStringLiteral("sourceAtTick"))
        || operation.contains(QStringLiteral("sourceAt"));
    const auto targetTimeFieldProvided =
        operation.contains(QStringLiteral("targetAtTick"))
        || operation.contains(QStringLiteral("targetAt"));
    const auto requireExplicitRepair =
        [&error, &relationLabel](
            const bool ready,
            const bool laneProvided,
            const bool timeProvided,
            const QJsonObject& state,
            const QString& endpoint,
            const QString& prefix) {
            if (ready || (laneProvided && timeProvided)) return true;
            auto issue = state.value(QStringLiteral("issue")).toString();
            if (issue.isEmpty()) issue = QStringLiteral("invalid-event");
            error = QStringLiteral(
                "Relation '%1' has an unusable %2 endpoint (%3). "
                "Repair it by providing both '%4LaneId' and exactly one of "
                "'%4AtTick' or '%4At'.")
                        .arg(
                            relationLabel,
                            endpoint,
                            issue,
                            prefix);
            return false;
        };
    if (!requireExplicitRepair(
            existingSourceReady,
            sourceLaneProvided,
            sourceTimeFieldProvided,
            sourceState,
            QStringLiteral("source"),
            QStringLiteral("source"))
        || !requireExplicitRepair(
            existingTargetReady,
            targetLaneProvided,
            targetTimeFieldProvided,
            targetState,
            QStringLiteral("target"),
            QStringLiteral("target"))) {
        return false;
    }

    const auto* existingSource =
        existingSourceReady ? sourceResolution.event : nullptr;
    const auto* existingTarget =
        existingTargetReady ? targetResolution.event : nullptr;
    auto sourceLaneId =
        existingSource ? existingSource->laneId : std::string{};
    auto targetLaneId =
        existingTarget ? existingTarget->laneId : std::string{};
    if (sourceLaneProvided) {
        const auto value = requiredString(
            operation, QStringLiteral("sourceLaneId"), error);
        if (!value) return false;
        sourceLaneId = *value;
    }
    if (targetLaneProvided) {
        const auto value = requiredString(
            operation, QStringLiteral("targetLaneId"), error);
        if (!value) return false;
        targetLaneId = *value;
    }
    const auto* sourceLane = findLane(scenario, sourceLaneId);
    const auto* targetLane = findLane(scenario, targetLaneId);
    if (!sourceLane || !targetLane
        || sourceLane->kind == LaneKind::Clock
        || sourceLane->kind == LaneKind::Group
        || targetLane->kind == LaneKind::Clock
        || targetLane->kind == LaneKind::Group) {
        error = QStringLiteral(
            "Relation endpoints must be existing non-Clock signal lanes.");
        return false;
    }

    std::optional<std::string> clockId;
    if (!relationClockContext(
            project,
            scenario,
            operation,
            sourceLaneId,
            targetLaneId,
            clockId,
            error)) {
        return false;
    }
    auto sourceTick = existingSource ? existingSource->tick : Tick{0};
    auto targetTick = existingTarget ? existingTarget->tick : Tick{0};
    bool sourceTimeProvided = false;
    bool targetTimeProvided = false;
    if (!optionalAutomationTickField(
            project,
            operation,
            QStringLiteral("sourceAtTick"),
            QStringLiteral("sourceAt"),
            clockId,
            sourceTick,
            sourceTimeProvided,
            error)
        || !optionalAutomationTickField(
            project,
            operation,
            QStringLiteral("targetAtTick"),
            QStringLiteral("targetAt"),
            clockId,
            targetTick,
            targetTimeProvided,
            error)) {
        return false;
    }
    if (sourceTick < 0 || sourceTick >= scenario.duration
        || targetTick < 0 || targetTick >= scenario.duration) {
        error = QStringLiteral(
            "Relation endpoint time is outside the Scenario.");
        return false;
    }
    const auto* sourceEvent = waveformEventAt(
        scenario, sourceLaneId, sourceTick, error);
    if (!sourceEvent) return false;
    const auto* targetEvent = waveformEventAt(
        scenario, targetLaneId, targetTick, error);
    if (!targetEvent) return false;

    auto replacement = existing;
    if (operation.contains(QStringLiteral("newId"))) {
        const auto value = requiredString(
            operation, QStringLiteral("newId"), error);
        if (!value) return false;
        if (!uniqueRelationIdAtIndex(
                scenario, *value, *relationIndex)) {
            error = QStringLiteral("Relation ID '%1' already exists.")
                        .arg(QString::fromStdString(*value));
            return false;
        }
        replacement.id = *value;
    }
    replacement.sourceEventId = sourceEvent->id;
    replacement.targetEventId = targetEvent->id;
    bool minimumProvided = false;
    bool maximumProvided = false;
    if (!relationDelayField(
            project,
            operation,
            QStringLiteral("minimumDelay"),
            clockId,
            false,
            replacement.minimumDelay,
            minimumProvided,
            error)
        || !relationDelayField(
            project,
            operation,
            QStringLiteral("maximumDelay"),
            clockId,
            false,
            replacement.maximumDelay,
            maximumProvided,
            error)) {
        return false;
    }
    if (replacement.minimumDelay < 0
        || replacement.maximumDelay < replacement.minimumDelay) {
        error = QStringLiteral(
            "Relation delay range is invalid.");
        return false;
    }
    const auto endpointChanged =
        operation.contains(QStringLiteral("sourceLaneId"))
        || sourceTimeProvided
        || operation.contains(QStringLiteral("targetLaneId"))
        || targetTimeProvided;
    const auto usesCycleDelay =
        operation.contains(QStringLiteral("minimumDelayCycles"))
        || operation.contains(QStringLiteral("maximumDelayCycles"));
    if (operation.contains(QStringLiteral("clockId"))
        || endpointChanged
        || usesCycleDelay) {
        replacement.clockDomainId =
            clockId.value_or(std::string{});
    }

    if (operation.contains(QStringLiteral("condition"))) {
        const auto value = operation.value(QStringLiteral("condition"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'condition' must be a string.");
            return false;
        }
        replacement.condition =
            value.toString().trimmed().toStdString();
    }
    if (operation.contains(QStringLiteral("severity"))) {
        const auto value = operation.value(QStringLiteral("severity"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'severity' must be a string.");
            return false;
        }
        const auto severity = severityFromString(
            value.toString().trimmed().toLower().toStdString());
        if (!severity) {
            error = QStringLiteral(
                "Field 'severity' must be 'information', 'warning', or 'error'.");
            return false;
        }
        replacement.severity = *severity;
    }
    if (operation.contains(QStringLiteral("description"))) {
        const auto value = operation.value(QStringLiteral("description"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'description' must be a string.");
            return false;
        }
        replacement.description =
            value.toString().trimmed().toStdString();
    }
    result.insert(
        QStringLiteral("repairedSourceEndpoint"),
        !existingSourceReady);
    result.insert(
        QStringLiteral("repairedTargetEndpoint"),
        !existingTargetReady);
    const auto existingIdentityReady =
        !existing.id.empty()
        && uniqueRelationIdAtIndex(
            scenario, existing.id, *relationIndex);
    const auto replacementIdentityReady =
        !replacement.id.empty()
        && uniqueRelationIdAtIndex(
            scenario, replacement.id, *relationIndex);
    result.insert(
        QStringLiteral("selectedByRepairRef"),
        hasRelationRef);
    result.insert(
        QStringLiteral("previousRelationId"),
        QString::fromStdString(existing.id));
    result.insert(
        QStringLiteral("repairedIdentity"),
        !existingIdentityReady && replacementIdentityReady);
    if (replacement == existing) {
        result.insert(QStringLiteral("changed"), false);
        relationReport(scenario, existing, result);
        result.insert(
            QStringLiteral("relationRef"),
            QString::fromStdString(
                relationRepairRef(scenario, *relationIndex)));
        return true;
    }

    const auto changed = stack.execute(
        std::make_unique<ChangeRelationAtIndexCommand>(
            scenario,
            *relationIndex,
            existing,
            replacement));
    result.insert(QStringLiteral("changed"), changed);
    const auto& updated =
        scenario.relations.at(*relationIndex);
    relationReport(scenario, updated, result);
    result.insert(
        QStringLiteral("relationRef"),
        QString::fromStdString(
            relationRepairRef(scenario, *relationIndex)));
    return true;
}

bool applyDeleteRelation(
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("relationId"),
             QStringLiteral("relationRef")},
            error)) {
        return false;
    }
    const auto hasRelationId =
        operation.contains(QStringLiteral("relationId"));
    const auto hasRelationRef =
        operation.contains(QStringLiteral("relationRef"));
    if (hasRelationId == hasRelationRef) {
        error = QStringLiteral(
            "Use exactly one of 'relationId' or 'relationRef'.");
        return false;
    }
    std::optional<std::size_t> relationIndex;
    if (hasRelationId) {
        const auto relationId = requiredString(
            operation, QStringLiteral("relationId"), error);
        if (!relationId) return false;
        relationIndex = uniqueRelationIndexById(
            scenario, *relationId, error);
    } else {
        const auto relationRef = requiredString(
            operation, QStringLiteral("relationRef"), error);
        if (!relationRef) return false;
        relationIndex = resolveRelationRepairRef(
            scenario, *relationRef, error);
    }
    if (!relationIndex) return false;
    const auto removed = scenario.relations.at(*relationIndex);
    const auto removedRef =
        relationRepairRef(scenario, *relationIndex);
    const auto changed = stack.execute(
        std::make_unique<RemoveRelationAtIndexCommand>(
            scenario, *relationIndex, removed));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("relationId"),
        QString::fromStdString(removed.id));
    result.insert(
        QStringLiteral("relationRef"),
        QString::fromStdString(removedRef));
    result.insert(
        QStringLiteral("selectedByRepairRef"),
        hasRelationRef);
    return true;
}

bool applyAddMarker(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("id"),
             QStringLiteral("name"),
             QStringLiteral("atTick"),
             QStringLiteral("at"),
             QStringLiteral("startTick"),
             QStringLiteral("start"),
             QStringLiteral("endTick"),
             QStringLiteral("end"),
             QStringLiteral("clockId"),
             QStringLiteral("kind"),
             QStringLiteral("note")},
            error)) {
        return false;
    }
    const auto name = requiredString(
        operation, QStringLiteral("name"), error);
    if (!name) return false;
    if (!uniqueMarkerName(scenario, *name)) {
        error = QStringLiteral("Marker name '%1' already exists.")
                    .arg(QString::fromStdString(*name));
        return false;
    }

    std::optional<std::string> clockId;
    if (operation.contains(QStringLiteral("clockId"))) {
        const auto value = requiredString(
            operation, QStringLiteral("clockId"), error);
        if (!value) return false;
        clockId = *value;
    }
    const auto hasAt =
        operation.contains(QStringLiteral("atTick"))
        || operation.contains(QStringLiteral("at"));
    const auto hasRange =
        operation.contains(QStringLiteral("startTick"))
        || operation.contains(QStringLiteral("start"))
        || operation.contains(QStringLiteral("endTick"))
        || operation.contains(QStringLiteral("end"));
    if (hasAt == hasRange) {
        error = QStringLiteral(
            "Use either 'atTick'/'at' for a point or start/end for an interval.");
        return false;
    }

    Marker marker;
    marker.name = *name;
    if (hasAt) {
        const auto at = automationTickField(
            project,
            operation,
            QStringLiteral("atTick"),
            QStringLiteral("at"),
            clockId,
            error);
        if (!at) return false;
        marker.start = *at;
        marker.end = *at;
        marker.kind = MarkerKind::Point;
    } else {
        const auto start = automationTickField(
            project,
            operation,
            QStringLiteral("startTick"),
            QStringLiteral("start"),
            clockId,
            error);
        if (!start) return false;
        const auto end = automationTickField(
            project,
            operation,
            QStringLiteral("endTick"),
            QStringLiteral("end"),
            clockId,
            error);
        if (!end) return false;
        marker.start = *start;
        marker.end = *end;
        marker.kind = MarkerKind::Interval;
    }
    if (marker.start < 0 || marker.end < marker.start
        || marker.end > scenario.duration
        || (hasRange && marker.end == marker.start)) {
        error = QStringLiteral(
            "Marker interval is outside the Scenario or empty.");
        return false;
    }
    if (operation.contains(QStringLiteral("kind"))) {
        const auto value = operation.value(QStringLiteral("kind"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'kind' must be a string.");
            return false;
        }
        const auto kind = markerKindFromString(
            value.toString().trimmed().toLower().toStdString());
        if (!kind) {
            error = QStringLiteral(
                "Field 'kind' must be 'point', 'interval', 'phase', 'error', or 'note'.");
            return false;
        }
        marker.kind = *kind;
    }
    if (marker.kind == MarkerKind::Point
        && marker.start != marker.end) {
        error = QStringLiteral(
            "A point Marker must use 'atTick' or 'at'.");
        return false;
    }
    if (operation.contains(QStringLiteral("note"))) {
        const auto value = operation.value(QStringLiteral("note"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'note' must be a string.");
            return false;
        }
        marker.note = value.toString().trimmed().toStdString();
    }

    if (operation.contains(QStringLiteral("id"))) {
        const auto id = requiredString(
            operation, QStringLiteral("id"), error);
        if (!id) return false;
        marker.id = *id;
    } else {
        std::string seed;
        appendIdentityField(seed, scenario.id);
        appendIdentityField(seed, marker.name);
        appendIdentityField(seed, std::to_string(marker.start));
        appendIdentityField(seed, std::to_string(marker.end));
        appendIdentityField(seed, toString(marker.kind));
        appendIdentityField(seed, marker.note);
        marker.id = deterministicStableId(
            "marker",
            seed,
            [&scenario](const std::string& id) {
                return markerById(scenario, id) != nullptr;
            });
    }
    if (markerById(scenario, marker.id)) {
        error = QStringLiteral("Marker ID '%1' already exists.")
                    .arg(QString::fromStdString(marker.id));
        return false;
    }

    const auto changed = stack.execute(
        std::make_unique<AddMarkerCommand>(scenario, marker));
    result.insert(QStringLiteral("changed"), changed);
    const auto* added = markerById(scenario, marker.id);
    markerReport(added ? *added : marker, result);
    return true;
}

bool applyUpdateMarker(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("markerId"),
             QStringLiteral("markerRef"),
             QStringLiteral("newId"),
             QStringLiteral("name"),
             QStringLiteral("atTick"),
             QStringLiteral("at"),
             QStringLiteral("startTick"),
             QStringLiteral("start"),
             QStringLiteral("endTick"),
             QStringLiteral("end"),
             QStringLiteral("clockId"),
             QStringLiteral("kind"),
             QStringLiteral("note")},
            error)) {
        return false;
    }
    const auto hasMarkerId =
        operation.contains(QStringLiteral("markerId"));
    const auto hasMarkerRef =
        operation.contains(QStringLiteral("markerRef"));
    if (hasMarkerId == hasMarkerRef) {
        error = QStringLiteral(
            "Use exactly one of 'markerId' or 'markerRef'.");
        return false;
    }
    std::optional<std::size_t> markerIndex;
    if (hasMarkerId) {
        const auto markerId = requiredString(
            operation, QStringLiteral("markerId"), error);
        if (!markerId) return false;
        markerIndex = uniqueMarkerIndexById(
            scenario, *markerId, error);
    } else {
        const auto markerRef = requiredString(
            operation, QStringLiteral("markerRef"), error);
        if (!markerRef) return false;
        markerIndex = resolveMarkerRepairRef(
            scenario, *markerRef, error);
    }
    if (!markerIndex) return false;
    const auto existing = scenario.markers.at(*markerIndex);

    const std::array<QString, 10> editableFields{
        QStringLiteral("newId"),
        QStringLiteral("name"),
        QStringLiteral("atTick"),
        QStringLiteral("at"),
        QStringLiteral("startTick"),
        QStringLiteral("start"),
        QStringLiteral("endTick"),
        QStringLiteral("end"),
        QStringLiteral("kind"),
        QStringLiteral("note"),
    };
    if (std::none_of(
            editableFields.begin(),
            editableFields.end(),
            [&operation](const QString& field) {
                return operation.contains(field);
            })) {
        error = QStringLiteral(
            "update-marker contains no property change.");
        return false;
    }

    std::optional<std::string> clockId;
    if (operation.contains(QStringLiteral("clockId"))) {
        const auto value = requiredString(
            operation, QStringLiteral("clockId"), error);
        if (!value) return false;
        clockId = *value;
    }
    const auto hasAt =
        operation.contains(QStringLiteral("atTick"))
        || operation.contains(QStringLiteral("at"));
    const auto hasRange =
        operation.contains(QStringLiteral("startTick"))
        || operation.contains(QStringLiteral("start"))
        || operation.contains(QStringLiteral("endTick"))
        || operation.contains(QStringLiteral("end"));
    if (hasAt && hasRange) {
        error = QStringLiteral(
            "Point time cannot be combined with Marker start/end.");
        return false;
    }

    auto replacement = existing;
    if (operation.contains(QStringLiteral("newId"))) {
        const auto value = requiredString(
            operation, QStringLiteral("newId"), error);
        if (!value) return false;
        if (!uniqueMarkerIdAtIndex(
                scenario, *value, *markerIndex)) {
            error = QStringLiteral("Marker ID '%1' already exists.")
                        .arg(QString::fromStdString(*value));
            return false;
        }
        replacement.id = *value;
    }
    if (operation.contains(QStringLiteral("name"))) {
        const auto value = requiredString(
            operation, QStringLiteral("name"), error);
        if (!value) return false;
        if (!uniqueMarkerNameAtIndex(
                scenario, *value, *markerIndex)) {
            error = QStringLiteral("Marker name '%1' already exists.")
                        .arg(QString::fromStdString(*value));
            return false;
        }
        replacement.name = *value;
    }
    if (operation.contains(QStringLiteral("kind"))) {
        const auto value = operation.value(QStringLiteral("kind"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'kind' must be a string.");
            return false;
        }
        const auto kind = markerKindFromString(
            value.toString().trimmed().toLower().toStdString());
        if (!kind) {
            error = QStringLiteral(
                "Field 'kind' must be 'point', 'interval', 'phase', 'error', or 'note'.");
            return false;
        }
        replacement.kind = *kind;
    }
    if (operation.contains(QStringLiteral("note"))) {
        const auto value = operation.value(QStringLiteral("note"));
        if (!value.isString()) {
            error = QStringLiteral("Field 'note' must be a string.");
            return false;
        }
        replacement.note = value.toString().trimmed().toStdString();
    }

    if (hasAt) {
        const auto at = automationTickField(
            project,
            operation,
            QStringLiteral("atTick"),
            QStringLiteral("at"),
            clockId,
            error);
        if (!at) return false;
        replacement.start = *at;
        replacement.end = *at;
    } else {
        bool startProvided = false;
        bool endProvided = false;
        if (!optionalAutomationTickField(
                project,
                operation,
                QStringLiteral("startTick"),
                QStringLiteral("start"),
                clockId,
                replacement.start,
                startProvided,
                error)
            || !optionalAutomationTickField(
                project,
                operation,
                QStringLiteral("endTick"),
                QStringLiteral("end"),
                clockId,
                replacement.end,
                endProvided,
                error)) {
            return false;
        }
    }
    if (replacement.start < 0
        || replacement.end < replacement.start
        || replacement.end > scenario.duration) {
        error = QStringLiteral("Marker interval is invalid.");
        return false;
    }
    if (replacement.kind == MarkerKind::Point
        && replacement.start != replacement.end) {
        error = QStringLiteral(
            "A point Marker must have the same start and end.");
        return false;
    }
    const auto existingIdentityReady =
        !existing.id.empty()
        && uniqueMarkerIdAtIndex(
            scenario, existing.id, *markerIndex);
    const auto replacementIdentityReady =
        !replacement.id.empty()
        && uniqueMarkerIdAtIndex(
            scenario, replacement.id, *markerIndex);
    const auto repairedIdentity =
        !existingIdentityReady && replacementIdentityReady;
    const auto repairedGeometry =
        !markerGeometryValid(scenario, existing)
        && markerGeometryValid(scenario, replacement);
    result.insert(
        QStringLiteral("selectedByRepairRef"),
        hasMarkerRef);
    result.insert(
        QStringLiteral("previousMarkerId"),
        QString::fromStdString(existing.id));
    result.insert(
        QStringLiteral("repairedIdentity"),
        repairedIdentity);
    result.insert(
        QStringLiteral("repairedGeometry"),
        repairedGeometry);
    if (replacement == existing) {
        result.insert(QStringLiteral("changed"), false);
        markerReport(existing, result);
        result.insert(
            QStringLiteral("markerRef"),
            QString::fromStdString(
                markerRepairRef(scenario, *markerIndex)));
        return true;
    }

    const auto changed = stack.execute(
        std::make_unique<ChangeMarkerAtIndexCommand>(
            scenario,
            *markerIndex,
            existing,
            replacement));
    result.insert(QStringLiteral("changed"), changed);
    const auto& updated = scenario.markers.at(*markerIndex);
    markerReport(updated, result);
    result.insert(
        QStringLiteral("markerRef"),
        QString::fromStdString(
            markerRepairRef(scenario, *markerIndex)));
    return true;
}

bool applyDeleteMarker(
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("markerId"),
             QStringLiteral("markerRef")},
            error)) {
        return false;
    }
    const auto hasMarkerId =
        operation.contains(QStringLiteral("markerId"));
    const auto hasMarkerRef =
        operation.contains(QStringLiteral("markerRef"));
    if (hasMarkerId == hasMarkerRef) {
        error = QStringLiteral(
            "Use exactly one of 'markerId' or 'markerRef'.");
        return false;
    }
    std::optional<std::size_t> markerIndex;
    if (hasMarkerId) {
        const auto markerId = requiredString(
            operation, QStringLiteral("markerId"), error);
        if (!markerId) return false;
        markerIndex = uniqueMarkerIndexById(
            scenario, *markerId, error);
    } else {
        const auto markerRef = requiredString(
            operation, QStringLiteral("markerRef"), error);
        if (!markerRef) return false;
        markerIndex = resolveMarkerRepairRef(
            scenario, *markerRef, error);
    }
    if (!markerIndex) return false;
    const auto removed = scenario.markers.at(*markerIndex);
    const auto removedRef =
        markerRepairRef(scenario, *markerIndex);
    const auto changed = stack.execute(
        std::make_unique<RemoveMarkerAtIndexCommand>(
            scenario, *markerIndex, removed));
    result.insert(QStringLiteral("changed"), changed);
    markerReport(removed, result);
    result.insert(
        QStringLiteral("markerRef"),
        QString::fromStdString(removedRef));
    result.insert(
        QStringLiteral("selectedByRepairRef"),
        hasMarkerRef);
    return true;
}

bool applyDeleteEvent(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("eventId")},
            error)) {
        return false;
    }
    const auto eventId =
        requiredString(
            operation,
            QStringLiteral("eventId"),
            error);
    if (!eventId) return false;
    const auto eventIndex =
        uniqueEventIndexById(
            scenario, *eventId, error);
    if (!eventIndex) return false;

    const auto validation =
        validateProjectForAutomation(project).json;
    const auto scenarioId =
        QString::fromStdString(scenario.id);
    const auto selectedEventId =
        QString::fromStdString(*eventId);
    const auto validationIssues =
        validation
            .value(QStringLiteral("issues"))
            .toArray();
    const auto safelyDeletable =
        std::any_of(
            validationIssues.cbegin(),
            validationIssues.cend(),
            [&scenarioId, &selectedEventId](
                const QJsonValue& value) {
                const auto issue = value.toObject();
                return issue
                           .value(QStringLiteral("scenarioId"))
                           .toString()
                        == scenarioId
                    && issue
                           .value(QStringLiteral("eventId"))
                           .toString()
                        == selectedEventId
                    && issue
                           .value(
                               QStringLiteral(
                                   "repairOperations"))
                           .toArray()
                           .contains(
                               QStringLiteral(
                                   "delete-event"));
            });
    if (!safelyDeletable) {
        error = QStringLiteral(
                    "Event '%1' is not reported by validate as safely deletable.")
                    .arg(selectedEventId);
        return false;
    }

    const auto removed =
        scenario.events.at(*eventIndex);
    const auto context =
        eventValidationObject(
            project, scenario, removed, 1);
    const auto beforeEventCount =
        scenario.events.size();
    const auto beforeRelationCount =
        scenario.relations.size();
    const auto segmentCount =
        [](const Scenario& candidate) {
            std::size_t count = 0;
            for (const auto& lane : candidate.lanes) {
                count += lane.segments.size();
            }
            return count;
        };
    const auto beforeSegmentCount =
        segmentCount(scenario);
    const auto changed =
        stack.execute(
            std::make_unique<RemoveEventCommand>(
                scenario, *eventId));
    const auto afterEventCount =
        scenario.events.size();

    result.insert(
        QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("eventId"),
        QString::fromStdString(*eventId));
    result.insert(
        QStringLiteral("eventContext"),
        context);
    result.insert(
        QStringLiteral("removedEventCount"),
        static_cast<qint64>(
            beforeEventCount > afterEventCount
            ? beforeEventCount - afterEventCount
            : 0));
    result.insert(
        QStringLiteral("createdEventCount"),
        static_cast<qint64>(
            afterEventCount > beforeEventCount
            ? afterEventCount - beforeEventCount
            : 0));
    result.insert(
        QStringLiteral("removedRelationCount"),
        static_cast<qint64>(beforeRelationCount)
            - static_cast<qint64>(
                scenario.relations.size()));
    result.insert(
        QStringLiteral("removedSegmentCount"),
        static_cast<qint64>(beforeSegmentCount)
            - static_cast<qint64>(
                segmentCount(scenario)));
    return true;
}

bool applyRepairEventLink(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("eventId")},
            error)) {
        return false;
    }
    const auto eventId =
        requiredString(
            operation,
            QStringLiteral("eventId"),
            error);
    if (!eventId) return false;
    const auto eventIndex =
        uniqueEventIndexById(
            scenario, *eventId, error);
    if (!eventIndex) return false;

    const auto validation =
        validateProjectForAutomation(project).json;
    const auto scenarioId =
        QString::fromStdString(scenario.id);
    const auto selectedEventId =
        QString::fromStdString(*eventId);
    const auto validationIssues =
        validation
            .value(QStringLiteral("issues"))
            .toArray();
    const auto safelyRepairable =
        std::any_of(
            validationIssues.cbegin(),
            validationIssues.cend(),
            [&scenarioId, &selectedEventId](
                const QJsonValue& value) {
                const auto issue = value.toObject();
                return issue
                           .value(QStringLiteral("scenarioId"))
                           .toString()
                        == scenarioId
                    && issue
                           .value(QStringLiteral("eventId"))
                           .toString()
                        == selectedEventId
                    && issue
                           .value(
                               QStringLiteral(
                                   "repairOperations"))
                           .toArray()
                           .contains(
                               QStringLiteral(
                                   "repair-event-link"));
            });
    if (!safelyRepairable) {
        error = QStringLiteral(
                    "Event '%1' is not reported by validate as having a safely repairable waveform link.")
                    .arg(selectedEventId);
        return false;
    }

    const auto before =
        scenario.events.at(*eventIndex);
    const auto beforeContext =
        eventValidationObject(
            project, scenario, before, 1);
    const auto beforeEventCount =
        scenario.events.size();
    const auto beforeRelationCount =
        scenario.relations.size();
    const auto changed =
        stack.execute(
            std::make_unique<
                RepairWaveformEventLinkCommand>(
                    project, scenario, *eventId));
    QString afterError;
    const auto afterIndex =
        uniqueEventIndexById(
            scenario, *eventId, afterError);
    if (!afterIndex) {
        throw std::runtime_error(
            "event link repair lost the selected Event");
    }
    const auto& after =
        scenario.events.at(*afterIndex);

    result.insert(
        QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("eventId"),
        selectedEventId);
    result.insert(
        QStringLiteral("beforeEventContext"),
        beforeContext);
    result.insert(
        QStringLiteral("eventContext"),
        eventValidationObject(
            project, scenario, after, 1));
    result.insert(
        QStringLiteral("updatedEventCount"),
        before == after ? 0 : 1);
    result.insert(
        QStringLiteral("createdEventCount"),
        static_cast<qint64>(
            scenario.events.size()
                > beforeEventCount
            ? scenario.events.size()
                - beforeEventCount
            : 0));
    result.insert(
        QStringLiteral("removedEventCount"),
        static_cast<qint64>(
            beforeEventCount
                > scenario.events.size()
            ? beforeEventCount
                - scenario.events.size()
            : 0));
    result.insert(
        QStringLiteral("removedRelationCount"),
        static_cast<qint64>(
            beforeRelationCount)
            - static_cast<qint64>(
                scenario.relations.size()));
    result.insert(
        QStringLiteral("removedSegmentCount"),
        0);
    return true;
}

bool applyClearEventCycle(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("eventId")},
            error)) {
        return false;
    }
    const auto eventId =
        requiredString(
            operation,
            QStringLiteral("eventId"),
            error);
    if (!eventId) return false;
    const auto eventIndex =
        uniqueEventIndexById(
            scenario, *eventId, error);
    if (!eventIndex) return false;

    const auto validation =
        validateProjectForAutomation(project).json;
    const auto scenarioId =
        QString::fromStdString(scenario.id);
    const auto selectedEventId =
        QString::fromStdString(*eventId);
    const auto validationIssues =
        validation
            .value(QStringLiteral("issues"))
            .toArray();
    const auto safelyRepairable =
        std::any_of(
            validationIssues.cbegin(),
            validationIssues.cend(),
            [&scenarioId, &selectedEventId](
                const QJsonValue& value) {
                const auto issue = value.toObject();
                return issue
                           .value(QStringLiteral("scenarioId"))
                           .toString()
                        == scenarioId
                    && issue
                           .value(QStringLiteral("eventId"))
                           .toString()
                        == selectedEventId
                    && issue
                           .value(
                               QStringLiteral(
                                   "repairOperations"))
                           .toArray()
                           .contains(
                               QStringLiteral(
                                   "clear-event-cycle"));
            });
    if (!safelyRepairable) {
        error = QStringLiteral(
                    "Event '%1' is not reported by validate as having inconsistent cycle metadata.")
                    .arg(selectedEventId);
        return false;
    }

    const auto before =
        scenario.events.at(*eventIndex);
    const auto beforeContext =
        eventValidationObject(
            project, scenario, before, 1);
    const auto changed =
        stack.execute(
            std::make_unique<
                ClearEventCycleCommand>(
                    scenario, *eventId));
    QString afterError;
    const auto afterIndex =
        uniqueEventIndexById(
            scenario, *eventId, afterError);
    if (!afterIndex) {
        throw std::runtime_error(
            "event cycle repair lost the selected Event");
    }
    const auto& after =
        scenario.events.at(*afterIndex);

    result.insert(
        QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("eventId"),
        selectedEventId);
    result.insert(
        QStringLiteral("beforeEventContext"),
        beforeContext);
    result.insert(
        QStringLiteral("eventContext"),
        eventValidationObject(
            project, scenario, after, 1));
    result.insert(
        QStringLiteral("updatedEventCount"),
        before == after ? 0 : 1);
    result.insert(
        QStringLiteral("createdEventCount"), 0);
    result.insert(
        QStringLiteral("removedEventCount"), 0);
    result.insert(
        QStringLiteral("removedRelationCount"), 0);
    result.insert(
        QStringLiteral("removedSegmentCount"), 0);
    return true;
}

bool applyRepairEventClock(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("eventId")},
            error)) {
        return false;
    }
    const auto eventId =
        requiredString(
            operation,
            QStringLiteral("eventId"),
            error);
    if (!eventId) return false;
    const auto eventIndex =
        uniqueEventIndexById(
            scenario, *eventId, error);
    if (!eventIndex) return false;

    const auto validation =
        validateProjectForAutomation(project).json;
    const auto scenarioId =
        QString::fromStdString(scenario.id);
    const auto selectedEventId =
        QString::fromStdString(*eventId);
    const auto validationIssues =
        validation
            .value(QStringLiteral("issues"))
            .toArray();
    const auto safelyRepairable =
        std::any_of(
            validationIssues.cbegin(),
            validationIssues.cend(),
            [&scenarioId, &selectedEventId](
                const QJsonValue& value) {
                const auto issue = value.toObject();
                return issue
                           .value(QStringLiteral("scenarioId"))
                           .toString()
                        == scenarioId
                    && issue
                           .value(QStringLiteral("eventId"))
                           .toString()
                        == selectedEventId
                    && issue
                           .value(
                               QStringLiteral(
                                   "repairOperations"))
                           .toArray()
                           .contains(
                               QStringLiteral(
                                   "repair-event-clock"));
            });
    if (!safelyRepairable) {
        error = QStringLiteral(
                    "Event '%1' is not reported by validate as having a safely repairable ClockDomain reference.")
                    .arg(selectedEventId);
        return false;
    }

    const auto before =
        scenario.events.at(*eventIndex);
    const auto beforeContext =
        eventValidationObject(
            project, scenario, before, 1);
    const auto changed =
        stack.execute(
            std::make_unique<
                RepairEventClockReferenceCommand>(
                    project, scenario, *eventId));
    QString afterError;
    const auto afterIndex =
        uniqueEventIndexById(
            scenario, *eventId, afterError);
    if (!afterIndex) {
        throw std::runtime_error(
            "event clock repair lost the selected Event");
    }
    const auto& after =
        scenario.events.at(*afterIndex);

    result.insert(
        QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("eventId"),
        selectedEventId);
    result.insert(
        QStringLiteral("beforeEventContext"),
        beforeContext);
    result.insert(
        QStringLiteral("eventContext"),
        eventValidationObject(
            project, scenario, after, 1));
    result.insert(
        QStringLiteral("updatedEventCount"),
        before == after ? 0 : 1);
    result.insert(
        QStringLiteral("createdEventCount"), 0);
    result.insert(
        QStringLiteral("removedEventCount"), 0);
    result.insert(
        QStringLiteral("removedRelationCount"), 0);
    result.insert(
        QStringLiteral("removedSegmentCount"), 0);
    return true;
}

bool applyRepairRelationClock(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("relationId")},
            error)) {
        return false;
    }
    const auto relationId =
        requiredString(
            operation,
            QStringLiteral("relationId"),
            error);
    if (!relationId) return false;
    const auto relationIndex =
        uniqueRelationIndexById(
            scenario, *relationId, error);
    if (!relationIndex) return false;

    const auto validation =
        validateProjectForAutomation(project).json;
    const auto scenarioId =
        QString::fromStdString(scenario.id);
    const auto selectedRelationId =
        QString::fromStdString(*relationId);
    const auto validationIssues =
        validation
            .value(QStringLiteral("issues"))
            .toArray();
    const auto safelyRepairable =
        std::any_of(
            validationIssues.cbegin(),
            validationIssues.cend(),
            [&scenarioId, &selectedRelationId](
                const QJsonValue& value) {
                const auto issue =
                    value.toObject();
                return issue
                           .value(
                               QStringLiteral(
                                   "scenarioId"))
                           .toString()
                        == scenarioId
                    && issue
                           .value(
                               QStringLiteral(
                                   "relationId"))
                           .toString()
                        == selectedRelationId
                    && issue
                           .value(
                               QStringLiteral(
                                   "repairOperations"))
                           .toArray()
                           .contains(
                               QStringLiteral(
                                   "repair-relation-clock"));
            });
    if (!safelyRepairable) {
        error = QStringLiteral(
                    "Relation '%1' is not reported by validate as having a safely repairable ClockDomain reference.")
                    .arg(selectedRelationId);
        return false;
    }

    const auto before =
        scenario.relations.at(*relationIndex);
    const auto relationIdCount =
        static_cast<std::size_t>(
            std::count_if(
                scenario.relations.begin(),
                scenario.relations.end(),
                [&before](const Relation& candidate) {
                    return candidate.id == before.id;
                }));
    const auto beforeRelationContext =
        relationAutomationObject(
            project,
            scenario,
            before,
            *relationIndex,
            relationIdCount);
    const auto beforeClockContext =
        relationClockValidationObject(
            project,
            scenario,
            before,
            relationIdCount);
    const auto changed =
        stack.execute(
            std::make_unique<
                RepairRelationClockReferenceCommand>(
                project, scenario, *relationId));
    QString afterError;
    const auto afterIndex =
        uniqueRelationIndexById(
            scenario, *relationId, afterError);
    if (!afterIndex) {
        throw std::runtime_error(
            "relation clock repair lost the selected Relation");
    }
    const auto& after =
        scenario.relations.at(*afterIndex);

    result.insert(
        QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("relationId"),
        selectedRelationId);
    result.insert(
        QStringLiteral("relationIndex"),
        static_cast<qint64>(*afterIndex));
    result.insert(
        QStringLiteral("beforeRelationContext"),
        beforeRelationContext);
    result.insert(
        QStringLiteral("beforeRelationClockContext"),
        beforeClockContext);
    result.insert(
        QStringLiteral("relationContext"),
        relationAutomationObject(
            project,
            scenario,
            after,
            *afterIndex,
            1));
    result.insert(
        QStringLiteral("relationClockContext"),
        relationClockValidationObject(
            project,
            scenario,
            after,
            1));
    result.insert(
        QStringLiteral("updatedRelationCount"),
        before == after ? 0 : 1);
    result.insert(
        QStringLiteral("createdEventCount"), 0);
    result.insert(
        QStringLiteral("updatedEventCount"), 0);
    result.insert(
        QStringLiteral("removedEventCount"), 0);
    result.insert(
        QStringLiteral("removedRelationCount"), 0);
    result.insert(
        QStringLiteral("removedSegmentCount"), 0);
    return true;
}

bool applyRepairTraceMapping(
    Project& project,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("traceId"),
             QStringLiteral("traceRef"),
             QStringLiteral("laneId")},
            error)) {
        return false;
    }
    const auto hasTraceId =
        operation.contains(QStringLiteral("traceId"));
    const auto hasTraceRef =
        operation.contains(QStringLiteral("traceRef"));
    if (hasTraceId == hasTraceRef) {
        error = QStringLiteral(
            "Use exactly one of 'traceId' or 'traceRef'.");
        return false;
    }
    std::optional<std::size_t> traceIndex;
    if (hasTraceId) {
        const auto traceId = requiredString(
            operation,
            QStringLiteral("traceId"),
            error);
        if (!traceId) return false;
        traceIndex = uniqueTraceIndexById(
            project, *traceId, error);
    } else {
        const auto traceRef = requiredString(
            operation,
            QStringLiteral("traceRef"),
            error);
        if (!traceRef) return false;
        traceIndex = resolveTraceRepairRef(
            project, *traceRef, error);
    }
    if (!traceIndex) return false;

    const auto laneValue =
        operation.value(QStringLiteral("laneId"));
    if (!laneValue.isString()) {
        error = QStringLiteral(
            "Field 'laneId' must be a string.");
        return false;
    }
    const auto laneId =
        laneValue.toString().toStdString();
    const auto before =
        project.importedTraces.at(*traceIndex);
    const auto mapping =
        before.signalMapping.find(laneId);
    if (mapping == before.signalMapping.end()) {
        error = QStringLiteral(
            "Imported trace mapping for Lane '%1' does not exist.")
                    .arg(QString::fromStdString(laneId));
        return false;
    }
    const auto problems =
        traceMappingProblems(
            project, laneId, mapping->second);
    if (problems.empty()) {
        error = QStringLiteral(
            "Imported trace mapping is not reported by validate as structurally invalid.");
        return false;
    }

    const auto beforeTraceContext =
        traceAutomationObject(
            project, *traceIndex);
    const auto beforeMappingContext =
        traceMappingValidationObject(
            project, *traceIndex, laneId);
    const auto actualSignalId =
        mapping->second;
    const auto changed =
        stack.execute(
            std::make_unique<
                RemoveTraceMappingAtIndexCommand>(
                project,
                *traceIndex,
                before,
                laneId));
    const auto& after =
        project.importedTraces.at(*traceIndex);
    result.insert(
        QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("selectedByRepairRef"),
        hasTraceRef);
    result.insert(
        QStringLiteral("traceId"),
        QString::fromStdString(after.id));
    result.insert(
        QStringLiteral("traceIndex"),
        static_cast<qint64>(*traceIndex));
    result.insert(
        QStringLiteral("traceRef"),
        QString::fromStdString(
            traceRepairRef(
                project, *traceIndex)));
    result.insert(
        QStringLiteral("laneId"),
        QString::fromStdString(laneId));
    result.insert(
        QStringLiteral("actualSignalId"),
        QString::fromStdString(actualSignalId));
    result.insert(
        QStringLiteral("beforeMappingCount"),
        static_cast<qint64>(
            before.signalMapping.size()));
    result.insert(
        QStringLiteral("mappingCount"),
        static_cast<qint64>(
            after.signalMapping.size()));
    result.insert(
        QStringLiteral("beforeTraceContext"),
        beforeTraceContext);
    result.insert(
        QStringLiteral("beforeTraceMappingContext"),
        beforeMappingContext);
    result.insert(
        QStringLiteral("traceContext"),
        traceAutomationObject(
            project, *traceIndex));
    result.insert(
        QStringLiteral("removedTraceMappingCount"),
        changed ? 1 : 0);
    result.insert(
        QStringLiteral("updatedTraceCount"),
        changed ? 1 : 0);
    result.insert(
        QStringLiteral("createdEventCount"), 0);
    result.insert(
        QStringLiteral("updatedEventCount"), 0);
    result.insert(
        QStringLiteral("removedEventCount"), 0);
    result.insert(
        QStringLiteral("removedRelationCount"), 0);
    result.insert(
        QStringLiteral("removedSegmentCount"), 0);
    return true;
}

bool applyRepairTraceReference(
    Project& project,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("traceRef"),
             QStringLiteral("path"),
             QStringLiteral("format")},
            error)) {
        return false;
    }
    const auto traceRef = requiredString(
        operation,
        QStringLiteral("traceRef"),
        error);
    if (!traceRef) return false;
    if (!operation.contains(QStringLiteral("path"))
        && !operation.contains(
            QStringLiteral("format"))) {
        error = QStringLiteral(
            "Provide at least one of 'path' or 'format'.");
        return false;
    }
    const auto traceIndex =
        resolveTraceRepairRef(
            project, *traceRef, error);
    if (!traceIndex) return false;

    const auto before =
        project.importedTraces.at(*traceIndex);
    if (traceReferenceProblems(before).empty()) {
        error = QStringLiteral(
            "Imported trace reference is not reported by validate as structurally invalid.");
        return false;
    }

    auto replacementPath = before.path;
    auto replacementFormat = before.format;
    if (operation.contains(QStringLiteral("path"))) {
        const auto requested = requiredString(
            operation,
            QStringLiteral("path"),
            error);
        if (!requested) return false;
        replacementPath = *requested;
    }
    if (operation.contains(QStringLiteral("format"))) {
        const auto requested = requiredString(
            operation,
            QStringLiteral("format"),
            error);
        if (!requested) return false;
        replacementFormat =
            normalizedTraceFormat(*requested)
                .toStdString();
    }

    auto replacement = before;
    replacement.path = replacementPath;
    replacement.format = replacementFormat;
    const auto remainingProblems =
        traceReferenceProblems(replacement);
    if (!remainingProblems.empty()) {
        QStringList descriptions;
        for (const auto& problem :
             remainingProblems) {
            descriptions.append(
                problem
                        == QStringLiteral(
                            "empty-path")
                    ? QStringLiteral(
                          "path must be non-empty")
                    : QStringLiteral(
                          "format must be VCD, FST, or CSV"));
        }
        error = QStringLiteral(
            "Repaired imported trace reference remains invalid: %1.")
                    .arg(
                        descriptions.join(
                            QStringLiteral(", ")));
        return false;
    }

    const auto beforeTraceContext =
        traceAutomationObject(
            project, *traceIndex);
    const auto beforeReferenceContext =
        traceReferenceValidationObject(
            project, *traceIndex);
    const auto changed =
        stack.execute(
            std::make_unique<
                ChangeTraceSourceAtIndexCommand>(
                project,
                *traceIndex,
                before,
                replacementPath,
                replacementFormat));
    const auto& after =
        project.importedTraces.at(*traceIndex);
    QJsonArray changedProperties;
    if (before.path != after.path) {
        changedProperties.append(
            QStringLiteral("path"));
    }
    if (before.format != after.format) {
        changedProperties.append(
            QStringLiteral("format"));
    }
    result.insert(
        QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("selectedByRepairRef"),
        true);
    result.insert(
        QStringLiteral("traceId"),
        QString::fromStdString(after.id));
    result.insert(
        QStringLiteral("traceIndex"),
        static_cast<qint64>(*traceIndex));
    result.insert(
        QStringLiteral("beforeTraceRef"),
        QString::fromStdString(*traceRef));
    result.insert(
        QStringLiteral("traceRef"),
        QString::fromStdString(
            traceRepairRef(
                project, *traceIndex)));
    result.insert(
        QStringLiteral("beforePath"),
        QString::fromStdString(before.path));
    result.insert(
        QStringLiteral("path"),
        QString::fromStdString(after.path));
    result.insert(
        QStringLiteral("beforeFormat"),
        QString::fromStdString(before.format));
    result.insert(
        QStringLiteral("format"),
        QString::fromStdString(after.format));
    result.insert(
        QStringLiteral("changedProperties"),
        changedProperties);
    result.insert(
        QStringLiteral("beforeTraceContext"),
        beforeTraceContext);
    result.insert(
        QStringLiteral(
            "beforeTraceReferenceContext"),
        beforeReferenceContext);
    result.insert(
        QStringLiteral("traceContext"),
        traceAutomationObject(
            project, *traceIndex));
    result.insert(
        QStringLiteral("traceReferenceContext"),
        traceReferenceValidationObject(
            project, *traceIndex));
    result.insert(
        QStringLiteral("updatedTraceCount"),
        changed ? 1 : 0);
    result.insert(
        QStringLiteral(
            "repairedTraceReference"),
        changed);
    result.insert(
        QStringLiteral("createdEventCount"), 0);
    result.insert(
        QStringLiteral("updatedEventCount"), 0);
    result.insert(
        QStringLiteral("removedEventCount"), 0);
    result.insert(
        QStringLiteral("removedRelationCount"), 0);
    result.insert(
        QStringLiteral("removedSegmentCount"), 0);
    return true;
}

bool applyRepairTraceIdentity(
    Project& project,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("traceRef"),
             QStringLiteral("newId")},
            error)) {
        return false;
    }
    const auto traceRef = requiredString(
        operation,
        QStringLiteral("traceRef"),
        error);
    if (!traceRef) return false;
    const auto traceIndex =
        resolveTraceRepairRef(
            project, *traceRef, error);
    if (!traceIndex) return false;

    const auto before =
        project.importedTraces.at(*traceIndex);
    const auto idMatchCount =
        static_cast<std::size_t>(
            std::count_if(
                project.importedTraces.begin(),
                project.importedTraces.end(),
                [&before](
                    const ImportedTrace& trace) {
                    return trace.id == before.id;
                }));
    if (!before.id.empty() && idMatchCount == 1) {
        error = QStringLiteral(
            "Imported trace identity is not reported by validate as missing or duplicate.");
        return false;
    }

    const auto generated =
        !operation.contains(QStringLiteral("newId"));
    std::string replacementId;
    if (generated) {
        replacementId =
            deterministicTraceIdentity(
                project, *traceIndex);
    } else {
        const auto requested = requiredString(
            operation,
            QStringLiteral("newId"),
            error);
        if (!requested) return false;
        replacementId = *requested;
    }
    const auto idTaken = std::any_of(
        project.importedTraces.begin(),
        project.importedTraces.end(),
        [&project, traceIndex, &replacementId](
            const ImportedTrace& trace) {
            return &trace
                    != &project.importedTraces
                           .at(*traceIndex)
                && trace.id == replacementId;
        });
    if (idTaken) {
        error = QStringLiteral(
            "Imported trace stable ID '%1' is already in use.")
                    .arg(QString::fromStdString(
                        replacementId));
        return false;
    }

    const auto beforeTraceContext =
        traceAutomationObject(
            project, *traceIndex);
    const auto changed =
        stack.execute(
            std::make_unique<
                ChangeTraceIdentityAtIndexCommand>(
                project,
                *traceIndex,
                before,
                replacementId));
    const auto& after =
        project.importedTraces.at(*traceIndex);
    result.insert(
        QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("selectedByRepairRef"),
        true);
    result.insert(
        QStringLiteral("generatedId"),
        generated);
    result.insert(
        QStringLiteral("beforeTraceId"),
        QString::fromStdString(before.id));
    result.insert(
        QStringLiteral("traceId"),
        QString::fromStdString(after.id));
    result.insert(
        QStringLiteral("traceIndex"),
        static_cast<qint64>(*traceIndex));
    result.insert(
        QStringLiteral("beforeTraceRef"),
        QString::fromStdString(*traceRef));
    result.insert(
        QStringLiteral("traceRef"),
        QString::fromStdString(
            traceRepairRef(
                project, *traceIndex)));
    result.insert(
        QStringLiteral("beforeTraceIdMatchCount"),
        static_cast<qint64>(idMatchCount));
    result.insert(
        QStringLiteral("beforeTraceContext"),
        beforeTraceContext);
    result.insert(
        QStringLiteral("traceContext"),
        traceAutomationObject(
            project, *traceIndex));
    result.insert(
        QStringLiteral("updatedTraceCount"),
        changed ? 1 : 0);
    result.insert(
        QStringLiteral("repairedIdentity"),
        changed);
    result.insert(
        QStringLiteral("createdEventCount"), 0);
    result.insert(
        QStringLiteral("updatedEventCount"), 0);
    result.insert(
        QStringLiteral("removedEventCount"), 0);
    result.insert(
        QStringLiteral("removedRelationCount"), 0);
    result.insert(
        QStringLiteral("removedSegmentCount"), 0);
    return true;
}

bool applyRepairLaneClock(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("laneId")},
            error)) {
        return false;
    }
    const auto laneId =
        requiredString(
            operation,
            QStringLiteral("laneId"),
            error);
    if (!laneId) return false;
    const Lane* lane = nullptr;
    std::size_t laneMatches = 0;
    std::size_t laneIndex = 0;
    for (std::size_t index = 0;
         index < scenario.lanes.size();
         ++index) {
        if (scenario.lanes.at(index).id
            != *laneId) {
            continue;
        }
        lane = &scenario.lanes.at(index);
        laneIndex = index;
        ++laneMatches;
    }
    if (laneMatches != 1 || !lane) {
        error = laneMatches == 0
            ? QStringLiteral(
                  "Lane '%1' does not exist.")
                  .arg(
                      QString::fromStdString(
                          *laneId))
            : QStringLiteral(
                  "Lane stable ID '%1' is ambiguous.")
                  .arg(
                      QString::fromStdString(
                          *laneId));
        return false;
    }

    const auto validation =
        validateProjectForAutomation(project).json;
    const auto scenarioId =
        QString::fromStdString(scenario.id);
    const auto selectedLaneId =
        QString::fromStdString(*laneId);
    const auto validationIssues =
        validation
            .value(QStringLiteral("issues"))
            .toArray();
    const auto safelyRepairable =
        std::any_of(
            validationIssues.cbegin(),
            validationIssues.cend(),
            [&scenarioId, &selectedLaneId](
                const QJsonValue& value) {
                const auto issue = value.toObject();
                return issue
                           .value(QStringLiteral("scenarioId"))
                           .toString()
                        == scenarioId
                    && issue
                           .value(QStringLiteral("laneId"))
                           .toString()
                        == selectedLaneId
                    && issue
                           .value(
                               QStringLiteral(
                                   "repairOperations"))
                           .toArray()
                           .contains(
                               QStringLiteral(
                                   "repair-lane-clock"));
            });
    if (!safelyRepairable) {
        error = QStringLiteral(
                    "Lane '%1' is not reported by validate as having a safely repairable ClockDomain reference.")
                    .arg(selectedLaneId);
        return false;
    }

    const auto before = *lane;
    const auto beforeContext =
        laneClockValidationObject(
            project, scenario, before, 1);
    const auto cycleCount =
        [&scenario, &laneId]() {
            return static_cast<std::size_t>(
                std::count_if(
                    scenario.events.begin(),
                    scenario.events.end(),
                    [&laneId](const Event& event) {
                        return event.laneId
                                   == *laneId
                            && event.cycle
                            && event.clockDomainId.empty();
                    }));
        };
    const auto beforeCycleCount =
        cycleCount();
    const auto changed =
        stack.execute(
            std::make_unique<
                RepairLaneClockReferenceCommand>(
                    project, scenario, *laneId));
    const Lane* after = nullptr;
    laneMatches = 0;
    for (const auto& candidate :
         scenario.lanes) {
        if (candidate.id != *laneId) continue;
        after = &candidate;
        ++laneMatches;
    }
    if (laneMatches != 1 || !after) {
        throw std::runtime_error(
            "lane clock repair lost the selected Lane");
    }
    const auto afterCycleCount =
        cycleCount();

    result.insert(
        QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("laneId"),
        selectedLaneId);
    result.insert(
        QStringLiteral("laneIndex"),
        static_cast<qint64>(laneIndex));
    result.insert(
        QStringLiteral("beforeLaneContext"),
        beforeContext);
    result.insert(
        QStringLiteral("laneContext"),
        laneClockValidationObject(
            project, scenario, *after, 1));
    result.insert(
        QStringLiteral("updatedLaneCount"),
        before == *after ? 0 : 1);
    result.insert(
        QStringLiteral("clearedEventCycleCount"),
        static_cast<qint64>(
            beforeCycleCount
                > afterCycleCount
            ? beforeCycleCount
                - afterCycleCount
            : 0));
    result.insert(
        QStringLiteral("updatedEventCount"),
        static_cast<qint64>(
            beforeCycleCount
                > afterCycleCount
            ? beforeCycleCount
                - afterCycleCount
            : 0));
    result.insert(
        QStringLiteral("createdEventCount"), 0);
    result.insert(
        QStringLiteral("removedEventCount"), 0);
    result.insert(
        QStringLiteral("removedRelationCount"), 0);
    result.insert(
        QStringLiteral("removedSegmentCount"), 0);
    return true;
}

bool applyRepairLaneGroup(
    const Project& project,
    Scenario& scenario,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("laneId")},
            error)) {
        return false;
    }
    const auto laneId =
        requiredString(
            operation,
            QStringLiteral("laneId"),
            error);
    if (!laneId) return false;

    const Lane* lane = nullptr;
    std::size_t laneMatches = 0;
    std::size_t laneIndex = 0;
    for (std::size_t index = 0;
         index < scenario.lanes.size();
         ++index) {
        if (scenario.lanes.at(index).id
            != *laneId) {
            continue;
        }
        lane = &scenario.lanes.at(index);
        laneIndex = index;
        ++laneMatches;
    }
    if (laneMatches != 1 || !lane) {
        error = laneMatches == 0
            ? QStringLiteral(
                  "Lane '%1' does not exist.")
                  .arg(
                      QString::fromStdString(
                          *laneId))
            : QStringLiteral(
                  "Lane stable ID '%1' is ambiguous.")
                  .arg(
                      QString::fromStdString(
                          *laneId));
        return false;
    }

    const auto validation =
        validateProjectForAutomation(project).json;
    const auto scenarioId =
        QString::fromStdString(scenario.id);
    const auto selectedLaneId =
        QString::fromStdString(*laneId);
    const auto validationIssues =
        validation
            .value(QStringLiteral("issues"))
            .toArray();
    const auto safelyRepairable =
        std::any_of(
            validationIssues.cbegin(),
            validationIssues.cend(),
            [&scenarioId, &selectedLaneId](
                const QJsonValue& value) {
                const auto issue =
                    value.toObject();
                return issue
                           .value(
                               QStringLiteral(
                                   "scenarioId"))
                           .toString()
                        == scenarioId
                    && issue
                           .value(
                               QStringLiteral(
                                   "laneId"))
                           .toString()
                        == selectedLaneId
                    && issue
                           .value(
                               QStringLiteral(
                                   "repairOperations"))
                           .toArray()
                           .contains(
                               QStringLiteral(
                                   "repair-lane-group"));
            });
    if (!safelyRepairable) {
        error = QStringLiteral(
                    "Lane '%1' is not reported by validate as having a safely repairable Group reference.")
                    .arg(selectedLaneId);
        return false;
    }

    const auto before = *lane;
    const auto beforeContext =
        laneGroupValidationObject(
            scenario, before, 1);
    const auto changed =
        stack.execute(
            std::make_unique<
                RepairLaneGroupReferenceCommand>(
                    scenario, *laneId));
    const Lane* after = nullptr;
    laneMatches = 0;
    for (const auto& candidate :
         scenario.lanes) {
        if (candidate.id != *laneId) continue;
        after = &candidate;
        ++laneMatches;
    }
    if (laneMatches != 1 || !after) {
        throw std::runtime_error(
            "lane group repair lost the selected Lane");
    }

    result.insert(
        QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("laneId"),
        selectedLaneId);
    result.insert(
        QStringLiteral("laneIndex"),
        static_cast<qint64>(
            laneIndex));
    result.insert(
        QStringLiteral("beforeLaneContext"),
        beforeContext);
    result.insert(
        QStringLiteral("laneContext"),
        laneGroupValidationObject(
            scenario, *after, 1));
    result.insert(
        QStringLiteral("updatedLaneCount"),
        before == *after ? 0 : 1);
    result.insert(
        QStringLiteral("createdEventCount"), 0);
    result.insert(
        QStringLiteral("updatedEventCount"), 0);
    result.insert(
        QStringLiteral("removedEventCount"), 0);
    result.insert(
        QStringLiteral("removedRelationCount"), 0);
    result.insert(
        QStringLiteral("removedSegmentCount"), 0);
    return true;
}

std::optional<std::size_t> scenarioInsertionIndex(
    const Project& project,
    const QJsonObject& operation,
    const std::size_t fallback,
    QString& error)
{
    if (!operation.contains(QStringLiteral("insertionIndex"))) {
        return fallback;
    }
    const auto value = integerField(
        operation, QStringLiteral("insertionIndex"), true, error);
    if (!value) return std::nullopt;
    if (*value < 0
        || static_cast<std::uint64_t>(*value) > project.scenarios.size()) {
        error = QStringLiteral(
            "Field 'insertionIndex' is outside the scenario list.");
        return std::nullopt;
    }
    return static_cast<std::size_t>(*value);
}

std::string generatedScenarioId(
    const Project& project,
    const std::string_view seed)
{
    return deterministicStableId(
        "scenario",
        project.id + ":" + std::string(seed),
        [&project](const std::string& candidate) {
            return scenarioIndexByStableId(project, candidate).has_value();
        });
}

bool applyCreateScenario(
    Project& project,
    std::string& activeScenarioId,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("id"),
             QStringLiteral("name"),
             QStringLiteral("durationTick"),
             QStringLiteral("duration"),
             QStringLiteral("insertionIndex")},
            error)) {
        return false;
    }
    const auto name = requiredString(
        operation, QStringLiteral("name"), error);
    if (!name) return false;
    if (!scenarioNameAvailable(project, *name)) {
        error = QStringLiteral(
            "Scenario name '%1' is empty or already in use.")
                    .arg(QString::fromStdString(*name));
        return false;
    }
    const auto activeIndex = scenarioIndexByStableId(
        project, activeScenarioId);
    if (!activeIndex) {
        error = QStringLiteral("The active Scenario no longer exists.");
        return false;
    }

    Scenario scenario;
    scenario.name = *name;
    scenario.id = operation.contains(QStringLiteral("id"))
        ? operation.value(QStringLiteral("id")).toString().trimmed().toStdString()
        : generatedScenarioId(project, *name);
    if (scenario.id.empty()) {
        error = QStringLiteral("Field 'id' must be a non-empty string.");
        return false;
    }
    if (scenarioIndexByStableId(project, scenario.id)) {
        error = QStringLiteral("Scenario ID '%1' already exists.")
                    .arg(QString::fromStdString(scenario.id));
        return false;
    }
    scenario.duration = project.scenarios.at(*activeIndex).duration;
    const auto hasDurationTick = operation.contains(
        QStringLiteral("durationTick"));
    const auto hasDuration = operation.contains(QStringLiteral("duration"));
    if (hasDurationTick || hasDuration) {
        const auto duration = automationTickField(
            project,
            operation,
            QStringLiteral("durationTick"),
            QStringLiteral("duration"),
            std::nullopt,
            error,
            false);
        if (!duration) return false;
        scenario.duration = *duration;
    }
    if (scenario.duration <= 0) {
        error = QStringLiteral("Scenario duration must be positive.");
        return false;
    }
    const auto insertionIndex = scenarioInsertionIndex(
        project, operation, *activeIndex + 1, error);
    if (!insertionIndex) return false;
    const auto createdId = scenario.id;
    const auto changed = stack.execute(
        std::make_unique<CreateScenarioCommand>(
            project, std::move(scenario), *insertionIndex));
    activeScenarioId = createdId;
    result.insert(QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("scenarioId"),
        QString::fromStdString(createdId));
    result.insert(
        QStringLiteral("name"),
        QString::fromStdString(*name));
    result.insert(
        QStringLiteral("destinationIndex"),
        static_cast<qint64>(*insertionIndex));
    result.insert(
        QStringLiteral("durationTick"),
        integerValue(project.scenarios.at(
            *scenarioIndexByStableId(project, createdId)).duration));
    result.insert(QStringLiteral("selected"), true);
    return true;
}

bool applyDuplicateScenario(
    Project& project,
    std::string& activeScenarioId,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"),
             QStringLiteral("id"),
             QStringLiteral("name"),
             QStringLiteral("insertionIndex")},
            error)) {
        return false;
    }
    const auto sourceIndex = scenarioIndexByStableId(
        project, activeScenarioId);
    if (!sourceIndex) {
        error = QStringLiteral("The active Scenario no longer exists.");
        return false;
    }
    const auto& source = project.scenarios.at(*sourceIndex);
    const auto sourceId = source.id;
    auto name = operation.contains(QStringLiteral("name"))
        ? operation.value(QStringLiteral("name")).toString().trimmed().toStdString()
        : nextScenarioDuplicateName(project, source);
    if (!scenarioNameAvailable(project, name)) {
        error = QStringLiteral(
            "Scenario name '%1' is empty or already in use.")
                    .arg(QString::fromStdString(name));
        return false;
    }
    auto duplicateId = operation.contains(QStringLiteral("id"))
        ? operation.value(QStringLiteral("id")).toString().trimmed().toStdString()
        : generatedScenarioId(
              project, sourceId + ":" + name);
    if (duplicateId.empty()) {
        error = QStringLiteral("Field 'id' must be a non-empty string.");
        return false;
    }
    if (scenarioIndexByStableId(project, duplicateId)) {
        error = QStringLiteral("Scenario ID '%1' already exists.")
                    .arg(QString::fromStdString(duplicateId));
        return false;
    }
    const auto insertionIndex = scenarioInsertionIndex(
        project, operation, *sourceIndex + 1, error);
    if (!insertionIndex) return false;
    const auto changed = stack.execute(
        std::make_unique<DuplicateScenarioCommand>(
            project,
            sourceId,
            duplicateId,
            name,
            *insertionIndex));
    activeScenarioId = duplicateId;
    const auto& duplicate = project.scenarios.at(
        *scenarioIndexByStableId(project, duplicateId));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("sourceScenarioId"),
        QString::fromStdString(sourceId));
    result.insert(
        QStringLiteral("scenarioId"),
        QString::fromStdString(duplicateId));
    result.insert(QStringLiteral("name"), QString::fromStdString(name));
    result.insert(
        QStringLiteral("destinationIndex"),
        static_cast<qint64>(*insertionIndex));
    result.insert(
        QStringLiteral("copiedLaneCount"),
        static_cast<qint64>(duplicate.lanes.size()));
    result.insert(
        QStringLiteral("copiedMarkerCount"),
        static_cast<qint64>(duplicate.markers.size()));
    result.insert(
        QStringLiteral("copiedRelationCount"),
        static_cast<qint64>(duplicate.relations.size()));
    result.insert(QStringLiteral("selected"), true);
    return true;
}

bool applyRenameScenario(
    Project& project,
    const std::string& activeScenarioId,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"), QStringLiteral("name")},
            error)) {
        return false;
    }
    const auto name = requiredString(
        operation, QStringLiteral("name"), error);
    if (!name) return false;
    if (!scenarioNameAvailable(project, *name, activeScenarioId)) {
        error = QStringLiteral(
            "Scenario name '%1' is empty or already in use.")
                    .arg(QString::fromStdString(*name));
        return false;
    }
    const auto changed = stack.execute(
        std::make_unique<RenameScenarioCommand>(
            project, activeScenarioId, *name));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("scenarioId"),
        QString::fromStdString(activeScenarioId));
    result.insert(QStringLiteral("name"), QString::fromStdString(*name));
    return true;
}

bool applyDeleteScenario(
    Project& project,
    std::string& activeScenarioId,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op")},
            error)) {
        return false;
    }
    if (project.scenarios.size() <= 1) {
        error = QStringLiteral("The last Scenario cannot be deleted.");
        return false;
    }
    const auto sourceIndex = scenarioIndexByStableId(
        project, activeScenarioId);
    if (!sourceIndex) {
        error = QStringLiteral("The active Scenario no longer exists.");
        return false;
    }
    const auto removedId = activeScenarioId;
    const auto removedName = project.scenarios.at(*sourceIndex).name;
    const auto fallbackIndex = *sourceIndex + 1 < project.scenarios.size()
        ? *sourceIndex + 1
        : *sourceIndex - 1;
    const auto fallbackId = project.scenarios.at(fallbackIndex).id;
    const auto changed = stack.execute(
        std::make_unique<DeleteScenarioCommand>(project, removedId));
    activeScenarioId = fallbackId;
    result.insert(QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("removedScenarioId"),
        QString::fromStdString(removedId));
    result.insert(
        QStringLiteral("removedName"),
        QString::fromStdString(removedName));
    result.insert(
        QStringLiteral("selectedScenarioId"),
        QString::fromStdString(fallbackId));
    result.insert(
        QStringLiteral("fallbackPolicy"),
        QStringLiteral("next-else-previous"));
    return true;
}

bool applyReorderScenario(
    Project& project,
    const std::string& activeScenarioId,
    CommandStack& stack,
    const QJsonObject& operation,
    QJsonObject& result,
    QString& error)
{
    if (!containsOnly(
            operation,
            {QStringLiteral("op"), QStringLiteral("destinationIndex")},
            error)) {
        return false;
    }
    const auto destination = integerField(
        operation, QStringLiteral("destinationIndex"), true, error);
    if (!destination) return false;
    if (*destination < 0
        || static_cast<std::uint64_t>(*destination)
            >= project.scenarios.size()) {
        error = QStringLiteral(
            "Field 'destinationIndex' is outside the scenario list.");
        return false;
    }
    const auto sourceIndex = scenarioIndexByStableId(
        project, activeScenarioId);
    if (!sourceIndex) {
        error = QStringLiteral("The active Scenario no longer exists.");
        return false;
    }
    const auto changed = stack.execute(
        std::make_unique<ReorderScenarioCommand>(
            project,
            activeScenarioId,
            static_cast<std::size_t>(*destination)));
    result.insert(QStringLiteral("changed"), changed);
    result.insert(
        QStringLiteral("scenarioId"),
        QString::fromStdString(activeScenarioId));
    result.insert(
        QStringLiteral("beforeIndex"),
        static_cast<qint64>(*sourceIndex));
    result.insert(
        QStringLiteral("destinationIndex"),
        *destination);
    return true;
}

} // namespace

AutomationDocument describeAutomationCapabilities()
{
    AutomationDocument result;

    struct CommandCapability {
        const char* name;
        bool requiresProject;
        bool canWriteProject;
    };
    constexpr std::array commandCapabilities{
        CommandCapability{"capabilities", false, false},
        CommandCapability{"new", false, true},
        CommandCapability{"inspect", true, false},
        CommandCapability{"signals", true, false},
        CommandCapability{"sample", true, false},
        CommandCapability{"window", true, false},
        CommandCapability{"edges", true, false},
        CommandCapability{"markers", true, false},
        CommandCapability{"relations", true, false},
        CommandCapability{"validate", true, false},
        CommandCapability{"apply", true, true},
    };
    QJsonArray commands;
    for (const auto& command : commandCapabilities) {
        commands.append(QJsonObject{
            {QStringLiteral("name"),
             QString::fromLatin1(command.name)},
            {QStringLiteral("requiresProject"),
             command.requiresProject},
            {QStringLiteral("canWriteProject"),
             command.canWriteProject},
        });
    }

    struct OperationCapability {
        const char* name;
        const char* category;
        const char* required;
        const char* optional;
        bool idempotent;
        bool dangerous;
        bool mutatesProject;
    };
    constexpr std::array operationCapabilities{
        OperationCapability{
            "create-scenario", "scenario", "op,name",
            "id,durationTick,duration,insertionIndex",
            false, false, true},
        OperationCapability{
            "duplicate-scenario", "scenario", "op",
            "id,name,insertionIndex", false, false, true},
        OperationCapability{
            "rename-scenario", "scenario", "op,name", "",
            true, false, true},
        OperationCapability{
            "delete-scenario", "scenario", "op", "",
            false, true, true},
        OperationCapability{
            "reorder-scenario", "scenario", "op,destinationIndex", "",
            true, false, true},
        OperationCapability{
            "add-group", "group", "op,name",
            "id,color,insertionIndex", false, false, true},
        OperationCapability{
            "update-group", "group", "op,groupId",
            "name,color,height,visible", true, false, true},
        OperationCapability{
            "move-group", "group", "op,groupId",
            "destinationIndex,beforeLaneId,afterLaneId", true, false, true},
        OperationCapability{
            "delete-group", "group", "op,groupId", "", false, true, true},
        OperationCapability{
            "add-signal", "signal", "op,name,kind",
            "id,width,signed,radix,enumMap,clockDomainId,groupId,color,"
            "insertionIndex,clockId,periodTick,period,phaseTick,phase,"
            "dutyNumerator,dutyDenominator,activeEdge",
            false, false, true},
        OperationCapability{
            "duplicate-signal", "signal", "op,laneId",
            "id,name,color,groupId,insertionIndex,beforeLaneId,afterLaneId,"
            "newClockId",
            false, false, true},
        OperationCapability{
            "rename-lane", "signal", "op,laneId,name", "", true, false, true},
        OperationCapability{
            "update-signal", "signal", "op,laneId",
            "name,width,signed,radix,enumMap,clockDomainId,color,height,"
            "visible,groupId",
            true, false, true},
        OperationCapability{
            "move-signal", "signal", "op,laneId",
            "destinationIndex,beforeLaneId,afterLaneId", true, false, true},
        OperationCapability{
            "delete-signal", "signal", "op,laneId", "", false, true, true},
        OperationCapability{
            "update-clock", "clock", "op,clockId",
            "name,periodTick,period,phaseTick,phase,dutyNumerator,"
            "dutyDenominator,activeEdge,resetRelation",
            true, false, true},
        OperationCapability{
            "assert-value", "waveform", "op,laneId,value",
            "atTick,at,clockId", true, false, false},
        OperationCapability{
            "set-range", "waveform", "op",
            "laneId,assignments,startTick,start,endTick,end,clockId,value",
            true, false, true},
        OperationCapability{
            "set-sequence", "waveform", "op",
            "laneId,values,sequences,startTick,start,stepTick,step,"
            "stepCycles,clockId,repeat",
            true, false, true},
        OperationCapability{
            "clear-range", "waveform", "op",
            "laneId,laneIds,startTick,start,endTick,end,clockId",
            true, true, true},
        OperationCapability{
            "transfer-range", "waveform", "op,mode",
            "laneId,laneIds,mappings,sourceStartTick,sourceStart,sourceEndTick,"
            "sourceEnd,destinationTick,destination,clockId,overwrite",
            false, true, true},
        OperationCapability{
            "set-duration", "scenario", "op",
            "durationTick,duration,clockId,truncate", true, true, true},
        OperationCapability{
            "repair-event-link", "intent", "op,eventId", "",
            false, false, true},
        OperationCapability{
            "clear-event-cycle", "intent", "op,eventId", "",
            false, false, true},
        OperationCapability{
            "repair-event-clock", "intent", "op,eventId", "",
            false, false, true},
        OperationCapability{
            "repair-relation-clock", "intent", "op,relationId", "",
            false, false, true},
        OperationCapability{
            "repair-trace-identity", "trace", "op,traceRef", "newId",
            false, false, true},
        OperationCapability{
            "repair-trace-reference", "trace", "op,traceRef", "path,format",
            false, false, true},
        OperationCapability{
            "repair-trace-mapping", "trace", "op,laneId", "traceId,traceRef",
            false, false, true},
        OperationCapability{
            "repair-lane-clock", "signal", "op,laneId", "",
            false, false, true},
        OperationCapability{
            "repair-lane-group", "signal", "op,laneId", "",
            false, false, true},
        OperationCapability{
            "delete-event", "intent", "op,eventId", "", false, true, true},
        OperationCapability{
            "add-relation", "intent", "op,sourceLaneId,targetLaneId",
            "id,sourceAtTick,sourceAt,targetAtTick,targetAt,minimumDelayTick,"
            "minimumDelay,minimumDelayCycles,maximumDelayTick,maximumDelay,"
            "maximumDelayCycles,clockId,condition,severity,description",
            true, false, true},
        OperationCapability{
            "update-relation", "intent", "op",
            "relationId,relationRef,newId,sourceLaneId,sourceAtTick,sourceAt,"
            "targetLaneId,targetAtTick,targetAt,minimumDelayTick,minimumDelay,"
            "minimumDelayCycles,maximumDelayTick,maximumDelay,"
            "maximumDelayCycles,clockId,condition,severity,description",
            true, false, true},
        OperationCapability{
            "delete-relation", "intent", "op", "relationId,relationRef",
            false, true, true},
        OperationCapability{
            "add-marker", "intent", "op,name",
            "id,atTick,at,startTick,start,endTick,end,clockId,kind,note",
            false, false, true},
        OperationCapability{
            "update-marker", "intent", "op",
            "markerId,markerRef,newId,name,atTick,at,startTick,start,endTick,end,"
            "clockId,kind,note",
            true, false, true},
        OperationCapability{
            "delete-marker", "intent", "op", "markerId,markerRef",
            false, true, true},
    };
    const auto fieldArray = [](const char* csv) {
        QJsonArray fields;
        const auto value = QString::fromLatin1(csv);
        if (value.isEmpty()) return fields;
        for (const auto& field : value.split(u',')) {
            fields.append(field);
        }
        return fields;
    };
    QJsonArray operations;
    for (const auto& operation : operationCapabilities) {
        const auto name = QString::fromLatin1(operation.name);
        operations.append(QJsonObject{
            {QStringLiteral("name"), name},
            {QStringLiteral("category"),
             QString::fromLatin1(operation.category)},
            {QStringLiteral("schemaRef"),
             QStringLiteral("%1#/$defs/%2")
                 .arg(
                     QString::fromLatin1(
                         AutomationBatchJsonSchemaRef),
                     name)},
            {QStringLiteral("required"),
             fieldArray(operation.required)},
            {QStringLiteral("optional"),
             fieldArray(operation.optional)},
            {QStringLiteral("idempotent"),
             operation.idempotent},
            {QStringLiteral("dangerous"),
             operation.dangerous},
            {QStringLiteral("mutatesProject"),
             operation.mutatesProject},
        });
    }

    const QJsonArray namedSelector{
        QStringLiteral("stable-id"),
        QStringLiteral("unique-name-case-insensitive"),
    };
    const QJsonArray stableIdSelector{
        QStringLiteral("stable-id"),
    };
    result.json = {
        {QStringLiteral("schema"),
         QString::fromLatin1(AutomationCapabilitiesSchema)},
        {QStringLiteral("schemaRef"),
         QString::fromLatin1(
             AutomationCapabilitiesJsonSchemaRef)},
        {QStringLiteral("command"), QStringLiteral("capabilities")},
        {QStringLiteral("ok"), true},
        {QStringLiteral("cliVersion"), 1},
        {QStringLiteral("projectSchemaVersion"),
         Project::CurrentSchemaVersion},
        {QStringLiteral("reportSchema"),
         QString::fromLatin1(AutomationReportSchema)},
        {QStringLiteral("reportSchemaRef"),
         QString::fromLatin1(
             AutomationReportJsonSchemaRef)},
        {QStringLiteral("schemaCatalog"),
         QJsonObject{
             {QStringLiteral("version"), 1},
             {QStringLiteral("dialect"),
              QString::fromLatin1(
                  AutomationJsonSchemaDialect)},
             {QStringLiteral("capabilities"),
              QString::fromLatin1(
                  AutomationCapabilitiesJsonSchemaRef)},
             {QStringLiteral("report"),
              QString::fromLatin1(
                  AutomationReportJsonSchemaRef)},
             {QStringLiteral("operationBatch"),
              QString::fromLatin1(
                  AutomationBatchJsonSchemaRef)},
         }},
        {QStringLiteral("operationBatch"),
         QJsonObject{
             {QStringLiteral("schema"),
              QString::fromLatin1(AutomationBatchSchema)},
             {QStringLiteral("schemaRef"),
              QString::fromLatin1(
                  AutomationBatchJsonSchemaRef)},
             {QStringLiteral("atomic"), true},
             {QStringLiteral("supportsExpectedProjectId"), true},
             {QStringLiteral("supportsScenarioSelector"), true},
         }},
        {QStringLiteral("commandCount"), commands.size()},
        {QStringLiteral("commands"), commands},
        {QStringLiteral("globalOptions"),
         QJsonArray{
             QStringLiteral("--help"),
             QStringLiteral("--version"),
         }},
        {QStringLiteral("operationCount"), operations.size()},
        {QStringLiteral("operations"), operations},
        {QStringLiteral("selectors"),
         QJsonObject{
             {QStringLiteral("scenarioId"), namedSelector},
             {QStringLiteral("laneId"), namedSelector},
             {QStringLiteral("groupId"), namedSelector},
             {QStringLiteral("clockId"), namedSelector},
             {QStringLiteral("markerId"), namedSelector},
             {QStringLiteral("relationId"), stableIdSelector},
             {QStringLiteral("traceId"), stableIdSelector},
             {QStringLiteral("traceRef"),
              QJsonArray{
                  QStringLiteral(
                      "validation-reference"),
              }},
         }},
        {QStringLiteral("time"),
         QJsonObject{
             {QStringLiteral("exactIntegerConversion"), true},
             {QStringLiteral("formats"),
              QJsonArray{
                  QStringLiteral("tick"),
                  QStringLiteral("physical"),
                  QStringLiteral("cycle"),
              }},
             {QStringLiteral("physicalUnits"),
              QJsonArray{
                  QStringLiteral("ps"),
                  QStringLiteral("ns"),
                  QStringLiteral("us"),
                  QStringLiteral("ms"),
              }},
             {QStringLiteral("fractionalPhysical"), true},
             {QStringLiteral("cycleUsesClockActiveEdge"), true},
         }},
        {QStringLiteral("values"),
         QJsonObject{
             {QStringLiteral("readableLaneKinds"),
              QJsonArray{
                  QStringLiteral("clock"),
                  QStringLiteral("bit"),
                  QStringLiteral("bus"),
                  QStringLiteral("enum"),
                  QStringLiteral("transaction"),
                  QStringLiteral("event"),
                  QStringLiteral("group"),
              }},
             {QStringLiteral("creatableLaneKinds"),
              QJsonArray{
                  QStringLiteral("clock"),
                  QStringLiteral("bit"),
                  QStringLiteral("bus"),
                  QStringLiteral("enum"),
                  QStringLiteral("group"),
              }},
             {QStringLiteral("markerKinds"),
              QJsonArray{
                  QStringLiteral("point"),
                  QStringLiteral("interval"),
                  QStringLiteral("phase"),
                  QStringLiteral("error"),
                  QStringLiteral("note"),
              }},
             {QStringLiteral("relationSeverities"),
              QJsonArray{
                  QStringLiteral("information"),
                  QStringLiteral("warning"),
                  QStringLiteral("error"),
              }},
             {QStringLiteral("edgeKinds"),
              QJsonArray{
                  QStringLiteral("initial"),
                  QStringLiteral("rising"),
                  QStringLiteral("falling"),
                  QStringLiteral("change"),
              }},
         }},
        {QStringLiteral("features"),
         QJsonObject{
             {QStringLiteral("operationsFromStdin"), true},
             {QStringLiteral("dryRun"), true},
             {QStringLiteral("atomicOutput"), true},
             {QStringLiteral("sourceSha256Guard"), true},
             {QStringLiteral("inPlaceBackup"), true},
             {QStringLiteral("deterministicGeneratedIds"), true},
             {QStringLiteral("structuredErrors"), true},
             {QStringLiteral("structuralIdentityValidation"), true},
             {QStringLiteral("markerIntegrityValidation"), true},
             {QStringLiteral("actionableValidationReferences"), true},
             {QStringLiteral("structuredRelationValidation"), true},
             {QStringLiteral("structuredWaveformValidation"), true},
             {QStringLiteral("structuredEventValidation"), true},
             {QStringLiteral("eventDeletion"), true},
             {QStringLiteral("eventLinkRepair"), true},
             {QStringLiteral("eventCycleRepair"), true},
             {QStringLiteral("eventClockRepair"), true},
             {QStringLiteral("relationClockRepair"), true},
             {QStringLiteral("traceIdentityValidation"), true},
             {QStringLiteral("traceIdentityRepair"), true},
             {QStringLiteral("traceReferenceValidation"), true},
             {QStringLiteral("traceReferenceRepair"), true},
             {QStringLiteral("traceMappingValidation"), true},
             {QStringLiteral("traceMappingRepair"), true},
             {QStringLiteral("traceRepairReference"), true},
             {QStringLiteral("laneClockRepair"), true},
             {QStringLiteral("laneGroupRepair"), true},
             {QStringLiteral("nonRegressiveApplyValidation"), true},
             {QStringLiteral("relationReadyEdgeQuery"), true},
             {QStringLiteral("relationEndpointRepair"), true},
             {QStringLiteral("relationRepairReference"), true},
             {QStringLiteral("compactMarkerQuery"), true},
             {QStringLiteral("markerRepairReference"), true},
             {QStringLiteral("compactRelationQuery"), true},
         }},
        {QStringLiteral("scenarioDuration"),
         QJsonObject{
             {QStringLiteral("supportsExtension"), true},
             {QStringLiteral("supportsSafeShrink"), true},
             {QStringLiteral("destructiveShrinkOptInField"),
              QStringLiteral("truncate")},
             {QStringLiteral("reportsTruncationCounts"), true},
         }},
    };
    return result;
}

AutomationDocument inspectProjectForAutomation(
    const Project& project,
    const std::optional<std::string>& scenarioId,
    const AutomationInspectDetail detail)
{
    AutomationDocument result;
    auto selectedId = scenarioId;
    if (scenarioId) {
        selectedId = resolveScenarioSelector(
            project, *scenarioId, result.error);
        if (!selectedId) return result;
    }

    QJsonArray clocks;
    for (const auto& clock : project.clockDomains) {
        clocks.append(clockObject(clock));
    }
    QJsonArray scenarios;
    for (const auto& scenario : project.scenarios) {
        if (!selectedId || scenario.id == *selectedId) {
            scenarios.append(
                detail == AutomationInspectDetail::Summary
                    ? scenarioSummaryObject(scenario, project.timeBase)
                    : scenarioObject(scenario, project.timeBase));
        }
    }
    QJsonArray importedTraces;
    for (std::size_t traceIndex = 0;
         traceIndex < project.importedTraces.size();
         ++traceIndex) {
        const auto& trace =
            project.importedTraces.at(traceIndex);
        const auto traceContext =
            traceAutomationObject(
                project, traceIndex);
        QJsonObject traceObject{
            {QStringLiteral("id"), QString::fromStdString(trace.id)},
            {QStringLiteral("traceIndex"),
             static_cast<qint64>(traceIndex)},
            {QStringLiteral("traceRef"),
             traceContext.value(
                 QStringLiteral("traceRef"))},
            {QStringLiteral("idMatchCount"),
             traceContext.value(
                 QStringLiteral(
                     "traceIdMatchCount"))},
            {QStringLiteral("path"), QString::fromStdString(trace.path)},
            {QStringLiteral("format"), QString::fromStdString(trace.format)},
            {QStringLiteral("offsetTick"), integerValue(trace.offset)},
            {QStringLiteral("mappingCount"),
             static_cast<qint64>(trace.signalMapping.size())},
        };
        if (detail == AutomationInspectDetail::Full) {
            QJsonObject mapping;
            for (const auto& [laneId, signalId] :
                 trace.signalMapping) {
                mapping.insert(
                    QString::fromStdString(laneId),
                    QString::fromStdString(signalId));
            }
            traceObject.insert(
                QStringLiteral("signalMapping"),
                mapping);
            traceObject.insert(
                QStringLiteral("extensions"),
                extensionsObject(trace.extensions));
        }
        importedTraces.append(traceObject);
    }
    QJsonArray linkedResources;
    for (const auto& resource : project.linkedResources) {
        linkedResources.append(QJsonObject{
            {QStringLiteral("kind"), QString::fromStdString(resource.kind)},
            {QStringLiteral("path"), QString::fromStdString(resource.path)},
            {QStringLiteral("stableId"),
             QString::fromStdString(resource.stableId)},
            {QStringLiteral("contentHash"),
             QString::fromStdString(resource.contentHash)},
            {QStringLiteral("summary"), QString::fromStdString(resource.summary)},
        });
    }

    result.json = {
        {QStringLiteral("schema"), QString::fromLatin1(AutomationReportSchema)},
        {QStringLiteral("command"), QStringLiteral("inspect")},
        {QStringLiteral("ok"), true},
        {QStringLiteral("detail"),
         detail == AutomationInspectDetail::Summary
             ? QStringLiteral("summary")
             : QStringLiteral("full")},
        {QStringLiteral("project"),
         QJsonObject{
             {QStringLiteral("schemaVersion"), project.schemaVersion},
             {QStringLiteral("id"), QString::fromStdString(project.id)},
             {QStringLiteral("name"), QString::fromStdString(project.name)},
             {QStringLiteral("timebase"),
              QJsonObject{
                  {QStringLiteral("picosecondsPerTick"),
                   integerValue(project.timeBase.picosecondsPerTick)},
              }},
             {QStringLiteral("clockDomainCount"),
              static_cast<qint64>(project.clockDomains.size())},
             {QStringLiteral("scenarioCount"),
              static_cast<qint64>(project.scenarios.size())},
             {QStringLiteral("clockDomains"), clocks},
             {QStringLiteral("scenarios"), scenarios},
             {QStringLiteral("importedTraces"), importedTraces},
             {QStringLiteral("linkedResources"), linkedResources},
             {QStringLiteral("extensions"), extensionsObject(project.extensions)},
         }},
    };
    if (selectedId) {
        result.json.insert(
            QStringLiteral("scenarioId"),
            QString::fromStdString(*selectedId));
    }
    return result;
}

AutomationDocument validateProjectForAutomation(
    const Project& project,
    const std::optional<std::string>& scenarioId)
{
    AutomationDocument result;
    auto selectedId = scenarioId;
    if (scenarioId) {
        selectedId = resolveScenarioSelector(
            project, *scenarioId, result.error);
        if (!selectedId) return result;
    }

    const auto identityAudit =
        auditStableIdentities(project, selectedId);
    const auto markerAudit =
        auditMarkerIntegrity(project, selectedId);
    const auto traceReferenceAudit =
        auditTraceReferences(project);
    const auto traceMappingAudit =
        auditTraceMappings(project);
    QJsonArray issues;
    std::size_t repairableIssueCount = 0;
    std::set<QString> repairTargets;
    const auto appendIssue =
        [&project,
         &issues,
         &repairableIssueCount,
         &repairTargets](
            QJsonObject issue,
            const std::optional<std::size_t> scenarioIndex =
                std::nullopt) {
            if (addTraceReferenceValidationContext(
                    project, issue)
                || addTraceMappingValidationContext(
                    project, issue)
                || addTraceIdentityValidationContext(
                    project, issue)
                || addRelationClockValidationContext(
                    project, issue, scenarioIndex)
                || addValidationRepairReference(
                    project, issue, scenarioIndex)
                || addLaneClockValidationContext(
                    project, issue, scenarioIndex)
                || addLaneGroupValidationContext(
                    project, issue, scenarioIndex)
                || addWaveformValidationContext(
                    project, issue, scenarioIndex)
                || addEventValidationContext(
                    project, issue, scenarioIndex)) {
                ++repairableIssueCount;
                const auto markerRef =
                    issue.value(
                        QStringLiteral("markerRef")).toString();
                const auto relationRef =
                    issue.value(
                        QStringLiteral("relationRef")).toString();
                const auto traceRef =
                    issue.value(
                        QStringLiteral("traceRef")).toString();
                const auto repairRange =
                    issue.value(
                        QStringLiteral("repairRange")).toObject();
                const auto eventId =
                    issue.value(
                        QStringLiteral("eventId")).toString();
                const auto laneId =
                    issue.value(
                        QStringLiteral("laneId")).toString();
                const auto objectKind =
                    issue.value(
                        QStringLiteral("objectKind")).toString();
                const auto issueScenarioId =
                    issue.value(
                        QStringLiteral("scenarioId")).toString();
                if (!traceRef.isEmpty()) {
                    if (objectKind
                        == QStringLiteral(
                            "imported-trace")) {
                        repairTargets.insert(
                            QStringLiteral(
                                "trace-identity:%1")
                                .arg(traceRef));
                    } else if (
                        objectKind
                        == QStringLiteral(
                            "imported-trace-reference")) {
                        repairTargets.insert(
                            QStringLiteral(
                                "trace-reference:%1")
                                .arg(traceRef));
                    } else {
                        repairTargets.insert(
                            QStringLiteral(
                                "trace-mapping:%1:%2")
                                .arg(
                                    traceRef,
                                    issue.value(
                                        QStringLiteral(
                                            "laneId"))
                                        .toString()));
                    }
                } else if (!markerRef.isEmpty()) {
                    repairTargets.insert(
                        QStringLiteral("marker:%1")
                            .arg(markerRef));
                } else if (!relationRef.isEmpty()) {
                    repairTargets.insert(
                        QStringLiteral("relation:%1")
                            .arg(relationRef));
                } else if (!eventId.isEmpty()) {
                    repairTargets.insert(
                        QStringLiteral("event:%1")
                            .arg(eventId));
                } else if (objectKind
                               == QStringLiteral("lane")
                           && !laneId.isEmpty()) {
                    repairTargets.insert(
                        QStringLiteral(
                            "lane:%1:%2")
                            .arg(
                                issueScenarioId,
                                laneId));
                } else {
                    repairTargets.insert(
                        QStringLiteral(
                            "waveform:%1:%2:%3")
                            .arg(
                                repairRange.value(
                                    QStringLiteral(
                                        "laneId")).toString(),
                                repairRange.value(
                                    QStringLiteral(
                                        "startTick")).toString(),
                                repairRange.value(
                                    QStringLiteral(
                                        "endTick")).toString()));
                }
            }
            issues.append(issue);
        };
    for (const auto& issue : identityAudit.issues) {
        appendIssue(issue.toObject());
    }
    for (const auto& issue : markerAudit.issues) {
        appendIssue(issue.toObject());
    }
    for (const auto& issue :
         traceReferenceAudit.issues) {
        appendIssue(issue.toObject());
    }
    for (const auto& issue :
         traceMappingAudit.issues) {
        appendIssue(issue.toObject());
    }
    std::size_t informationCount = 0;
    std::size_t warningCount = 0;
    std::size_t errorCount =
        static_cast<std::size_t>(
            identityAudit.issues.size()
            + markerAudit.issues.size()
            + traceReferenceAudit.issues.size()
            + traceMappingAudit.issues.size());
    std::size_t semanticIssueCount =
        static_cast<std::size_t>(
            markerAudit.issues.size()
            + traceReferenceAudit.issues.size()
            + traceMappingAudit.issues.size());
    for (std::size_t scenarioIndex = 0;
         scenarioIndex < project.scenarios.size();
         ++scenarioIndex) {
        const auto& scenario =
            project.scenarios.at(scenarioIndex);
        if (selectedId && scenario.id != *selectedId) continue;
        for (const auto& issue : validateScenario(project, scenario)) {
            appendIssue(
                issueObject(issue, scenario.id),
                scenarioIndex);
            ++semanticIssueCount;
            if (issue.severity == Severity::Error) {
                ++errorCount;
            } else if (issue.severity == Severity::Warning) {
                ++warningCount;
            } else {
                ++informationCount;
            }
        }
    }

    result.json = {
        {QStringLiteral("schema"), QString::fromLatin1(AutomationReportSchema)},
        {QStringLiteral("command"), QStringLiteral("validate")},
        {QStringLiteral("ok"), errorCount == 0},
        {QStringLiteral("valid"), errorCount == 0},
        {QStringLiteral("identityIssueCount"),
         unsignedIntegerValue(
             static_cast<std::size_t>(
                 identityAudit.issues.size()))},
        {QStringLiteral("semanticIssueCount"),
         unsignedIntegerValue(semanticIssueCount)},
        {QStringLiteral("markerIssueCount"),
         unsignedIntegerValue(
             static_cast<std::size_t>(
                 markerAudit.issues.size()))},
        {QStringLiteral("traceReferenceIssueCount"),
         unsignedIntegerValue(
             static_cast<std::size_t>(
                 traceReferenceAudit.issues.size()))},
        {QStringLiteral("traceMappingIssueCount"),
         unsignedIntegerValue(
             static_cast<std::size_t>(
                 traceMappingAudit.issues.size()))},
        {QStringLiteral("repairableIssueCount"),
         unsignedIntegerValue(repairableIssueCount)},
        {QStringLiteral("repairTargetCount"),
         unsignedIntegerValue(repairTargets.size())},
        {QStringLiteral("identitySummary"),
         QJsonObject{
             {QStringLiteral("valid"),
              identityAudit.issues.isEmpty()},
             {QStringLiteral("issueCount"),
              unsignedIntegerValue(
                  static_cast<std::size_t>(
                      identityAudit.issues.size()))},
             {QStringLiteral("affectedObjectCount"),
              unsignedIntegerValue(
                  identityAudit.affectedObjectCount)},
             {QStringLiteral("missingStableIdCount"),
              unsignedIntegerValue(
                  identityAudit.missingStableIdCount)},
             {QStringLiteral("duplicateStableIdCount"),
              unsignedIntegerValue(
                  identityAudit.duplicateStableIdCount)},
         }},
        {QStringLiteral("markerSummary"),
         QJsonObject{
             {QStringLiteral("valid"),
              markerAudit.issues.isEmpty()},
             {QStringLiteral("issueCount"),
              unsignedIntegerValue(
                  static_cast<std::size_t>(
                      markerAudit.issues.size()))},
             {QStringLiteral("affectedMarkerCount"),
              unsignedIntegerValue(
                  markerAudit.affectedMarkerCount)},
             {QStringLiteral("nameIssueCount"),
              unsignedIntegerValue(
                  markerAudit.nameIssueCount)},
             {QStringLiteral("geometryIssueCount"),
              unsignedIntegerValue(
                  markerAudit.geometryIssueCount)},
         }},
        {QStringLiteral("traceReferenceSummary"),
         QJsonObject{
             {QStringLiteral("valid"),
              traceReferenceAudit.issues.isEmpty()},
             {QStringLiteral("issueCount"),
              unsignedIntegerValue(
                  static_cast<std::size_t>(
                      traceReferenceAudit
                          .issues.size()))},
             {QStringLiteral("affectedTraceCount"),
              unsignedIntegerValue(
                  traceReferenceAudit
                      .affectedTraceCount)},
             {QStringLiteral("emptyPathCount"),
              unsignedIntegerValue(
                  traceReferenceAudit
                      .emptyPathCount)},
             {QStringLiteral(
                  "unsupportedFormatCount"),
              unsignedIntegerValue(
                  traceReferenceAudit
                      .unsupportedFormatCount)},
         }},
        {QStringLiteral("traceMappingSummary"),
         QJsonObject{
             {QStringLiteral("valid"),
              traceMappingAudit.issues.isEmpty()},
             {QStringLiteral("issueCount"),
              unsignedIntegerValue(
                  static_cast<std::size_t>(
                      traceMappingAudit.issues.size()))},
             {QStringLiteral("affectedTraceCount"),
              unsignedIntegerValue(
                  traceMappingAudit.affectedTraceCount)},
             {QStringLiteral("invalidMappingCount"),
              unsignedIntegerValue(
                  traceMappingAudit.invalidMappingCount)},
             {QStringLiteral(
                  "missingLaneReferenceCount"),
              unsignedIntegerValue(
                  traceMappingAudit
                      .missingLaneReferenceCount)},
             {QStringLiteral(
                  "emptyActualSignalIdCount"),
              unsignedIntegerValue(
                  traceMappingAudit
                      .emptyActualSignalIdCount)},
         }},
        {QStringLiteral("summary"),
         QJsonObject{
             {QStringLiteral("information"),
              unsignedIntegerValue(informationCount)},
             {QStringLiteral("warnings"), unsignedIntegerValue(warningCount)},
             {QStringLiteral("errors"), unsignedIntegerValue(errorCount)},
             {QStringLiteral("total"),
              unsignedIntegerValue(
                  informationCount + warningCount + errorCount)},
         }},
        {QStringLiteral("issues"), issues},
    };
    if (selectedId) {
        result.json.insert(
            QStringLiteral("scenarioId"),
            QString::fromStdString(*selectedId));
    }
    return result;
}

AutomationTimeResult parseAutomationTime(
    const Project& project,
    const QString& input,
    const std::optional<std::string>& clockId)
{
    AutomationTimeResult result;
    const auto text = input.trimmed().toLower();
    const QRegularExpression cycleExpression(
        QStringLiteral(R"(^cycle\s+(-?\d+)$)"));
    const auto cycleMatch = cycleExpression.match(text);
    if (cycleMatch.hasMatch()) {
        const ClockDomain* clock = nullptr;
        if (clockId) {
            const auto resolved =
                resolveClockSelector(project, *clockId, result.error);
            if (!resolved) return result;
            clock = findClock(project, *resolved);
        } else if (project.clockDomains.size() == 1) {
            clock = &project.clockDomains.front();
        }
        if (!clock) {
            result.error = QStringLiteral(
                "A cycle-based time requires one unambiguous clock domain.");
            return result;
        }
        bool valid = false;
        const auto cycle = cycleMatch.captured(1).toLongLong(&valid);
        if (!valid) {
            result.error = QStringLiteral("Invalid cycle index.");
            return result;
        }
        result.tick = tickAtCycle(*clock, cycle, clock->activeEdge);
        if (!result.tick) {
            result.error = QStringLiteral(
                "Cycle time is outside the integer tick range.");
        }
        return result;
    }

    const QRegularExpression absoluteExpression(
        QStringLiteral(
            R"(^(-?(?:\d+(?:\.\d*)?|\.\d+))\s*(ps|ns|us|ms|ticks?)?$)"));
    const auto match = absoluteExpression.match(text);
    if (!match.hasMatch()) {
        result.error = QStringLiteral(
            "Use a number followed by ps, ns, us, ms, tick, or 'cycle N'.");
        return result;
    }
    const auto valueText = match.captured(1);
    const auto suffix = match.captured(2);
    if (suffix.isEmpty() || suffix.startsWith(QStringLiteral("tick"))) {
        bool valid = false;
        const auto tick = valueText.toLongLong(&valid);
        if (!valid) {
            result.error = QStringLiteral("Tick values must be integers.");
            return result;
        }
        result.tick = tick;
        return result;
    }
    const auto unit = suffix == QStringLiteral("ps")
        ? TimeUnit::Picosecond
        : suffix == QStringLiteral("ns")
            ? TimeUnit::Nanosecond
            : suffix == QStringLiteral("us")
                ? TimeUnit::Microsecond
                : TimeUnit::Millisecond;
    const auto bytes = valueText.toLatin1();
    result.tick = toTicks(
        std::string_view{
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())},
        unit,
        project.timeBase);
    if (!result.tick) {
        result.error = QStringLiteral(
            "The time cannot be represented exactly in the project timebase.");
    }
    return result;
}

AutomationDocument sampleProjectForAutomation(
    const Project& project,
    const Tick tick,
    const std::optional<std::string>& scenarioId,
    const std::vector<std::string>& laneIds)
{
    AutomationDocument result;
    const auto* scenario =
        automationScenario(project, scenarioId, result.error);
    if (!scenario) return result;
    if (tick < 0 || tick >= scenario->duration) {
        result.error = QStringLiteral(
            "Sample time %1 is outside Scenario [0, %2).")
                           .arg(tick)
                           .arg(scenario->duration);
        return result;
    }

    std::set<std::string> requested;
    for (const auto& selector : laneIds) {
        const auto laneId =
            resolveLaneSelector(*scenario, selector, result.error);
        if (!laneId) return result;
        if (!requested.insert(*laneId).second) {
            result.error = QStringLiteral(
                "Lane filters resolve to duplicate Lane IDs.");
            return result;
        }
        const auto* lane = findLane(*scenario, *laneId);
        if (lane->kind == LaneKind::Group) {
            result.error = QStringLiteral("Group '%1' cannot be sampled.")
                               .arg(QString::fromStdString(*laneId));
            return result;
        }
    }

    QJsonArray samples;
    for (const auto& lane : scenario->lanes) {
        if (lane.kind == LaneKind::Group) continue;
        if (!requested.empty() && !requested.contains(lane.id)) continue;
        samples.append(sampleObject(project, lane, tick));
    }
    result.json = {
        {QStringLiteral("schema"), QString::fromLatin1(AutomationReportSchema)},
        {QStringLiteral("command"), QStringLiteral("sample")},
        {QStringLiteral("ok"), true},
        {QStringLiteral("projectId"), QString::fromStdString(project.id)},
        {QStringLiteral("scenarioId"), QString::fromStdString(scenario->id)},
        {QStringLiteral("atTick"), integerValue(tick)},
        {QStringLiteral("at"),
         QString::fromStdString(formatTick(tick, project.timeBase))},
        {QStringLiteral("sampleCount"), samples.size()},
        {QStringLiteral("samples"), samples},
    };
    return result;
}

AutomationDocument findSignalsForAutomation(
    const Project& project,
    const AutomationSignalQueryOptions& options,
    const std::optional<std::string>& scenarioId)
{
    AutomationDocument result;
    const auto* scenario =
        automationScenario(project, scenarioId, result.error);
    if (!scenario) return result;
    if (options.limit == 0 || options.limit > 1'000) {
        result.error = QStringLiteral(
            "Signal query limit must be from 1 to 1000.");
        return result;
    }

    const auto query =
        QString::fromStdString(options.match).trimmed();
    QJsonArray signalResults;
    std::size_t matchCount = 0;
    for (std::size_t index = 0; index < scenario->lanes.size(); ++index) {
        const auto& lane = scenario->lanes.at(index);
        if (options.kind && lane.kind != *options.kind) continue;
        const auto id = QString::fromStdString(lane.id);
        const auto name = QString::fromStdString(lane.name);
        const auto matches = query.isEmpty()
            || (options.exact
                    ? id.compare(query, Qt::CaseInsensitive) == 0
                        || name.compare(query, Qt::CaseInsensitive) == 0
                    : id.contains(query, Qt::CaseInsensitive)
                        || name.contains(query, Qt::CaseInsensitive));
        if (!matches) continue;
        ++matchCount;
        if (signalResults.size()
            >= static_cast<qsizetype>(options.limit)) {
            continue;
        }

        auto object = laneObject(lane);
        object.remove(QStringLiteral("segments"));
        object.remove(QStringLiteral("extensions"));
        object.insert(
            QStringLiteral("laneIndex"),
            static_cast<qint64>(index));
        if (!lane.clockDomainId.empty()) {
            const auto* clock = findClock(project, lane.clockDomainId);
            object.insert(
                QStringLiteral("clockDomainName"),
                clock ? QString::fromStdString(clock->name) : QString{});
        }
        if (!lane.groupId.empty()) {
            const auto* group = findLane(*scenario, lane.groupId);
            object.insert(
                QStringLiteral("groupName"),
                group ? QString::fromStdString(group->name) : QString{});
        }
        signalResults.append(object);
    }

    result.json = {
        {QStringLiteral("schema"), QString::fromLatin1(AutomationReportSchema)},
        {QStringLiteral("command"), QStringLiteral("signals")},
        {QStringLiteral("ok"), true},
        {QStringLiteral("projectId"), QString::fromStdString(project.id)},
        {QStringLiteral("scenarioId"), QString::fromStdString(scenario->id)},
        {QStringLiteral("query"), query},
        {QStringLiteral("exact"), options.exact},
        {QStringLiteral("kind"),
         options.kind
             ? QJsonValue{QString::fromLatin1(toString(*options.kind).data())}
             : QJsonValue{QJsonValue::Null}},
        {QStringLiteral("limit"), static_cast<qint64>(options.limit)},
        {QStringLiteral("laneCount"),
         static_cast<qint64>(scenario->lanes.size())},
        {QStringLiteral("matchCount"),
         static_cast<qint64>(matchCount)},
        {QStringLiteral("returnedCount"), signalResults.size()},
        {QStringLiteral("truncated"),
         matchCount > static_cast<std::size_t>(signalResults.size())},
        {QStringLiteral("signals"), signalResults},
    };
    return result;
}

AutomationDocument inspectProjectWindowForAutomation(
    const Project& project,
    const Tick start,
    const Tick end,
    const std::optional<std::string>& scenarioId,
    const std::vector<std::string>& laneIds)
{
    AutomationDocument result;
    const auto* scenario =
        automationScenario(project, scenarioId, result.error);
    if (!scenario) return result;
    if (start < 0 || end <= start || end > scenario->duration) {
        result.error = QStringLiteral(
            "Window [%1, %2) is outside Scenario [0, %3).")
                           .arg(start)
                           .arg(end)
                           .arg(scenario->duration);
        return result;
    }

    std::set<std::string> requested;
    for (const auto& selector : laneIds) {
        const auto laneId =
            resolveLaneSelector(*scenario, selector, result.error);
        if (!laneId) return result;
        if (!requested.insert(*laneId).second) {
            result.error = QStringLiteral(
                "Lane filters resolve to duplicate Lane IDs.");
            return result;
        }
        const auto* lane = findLane(*scenario, *laneId);
        if (lane->kind == LaneKind::Group) {
            result.error = QStringLiteral("Group '%1' has no waveform window.")
                               .arg(QString::fromStdString(*laneId));
            return result;
        }
    }

    QJsonArray lanes;
    std::set<std::string> includedLaneIds;
    std::set<std::string> referencedClockIds;
    for (const auto& lane : scenario->lanes) {
        if (lane.kind == LaneKind::Group) continue;
        if (!requested.empty() && !requested.contains(lane.id)) continue;
        lanes.append(windowLaneObject(project, lane, start, end));
        includedLaneIds.insert(lane.id);
        if (!lane.clockDomainId.empty()) {
            referencedClockIds.insert(lane.clockDomainId);
        }
    }

    QJsonArray clocks;
    for (const auto& clock : project.clockDomains) {
        if (referencedClockIds.contains(clock.id)) {
            clocks.append(clockObject(clock));
        }
    }

    std::set<std::string> windowEventIds;
    for (const auto& event : scenario->events) {
        if (event.tick < start || event.tick >= end
            || !includedLaneIds.contains(event.laneId)) {
            continue;
        }
        windowEventIds.insert(event.id);
    }

    QJsonArray relations;
    auto contextEventIds = windowEventIds;
    for (const auto& relation : scenario->relations) {
        if (windowEventIds.contains(relation.sourceEventId)
            || windowEventIds.contains(relation.targetEventId)) {
            relations.append(relationObject(relation));
            contextEventIds.insert(relation.sourceEventId);
            contextEventIds.insert(relation.targetEventId);
        }
    }

    QJsonArray events;
    for (const auto& event : scenario->events) {
        if (!contextEventIds.contains(event.id)) continue;
        auto object = eventObject(event);
        object.insert(
            QStringLiteral("inWindow"),
            event.tick >= start && event.tick < end);
        object.insert(
            QStringLiteral("laneSelected"),
            includedLaneIds.contains(event.laneId));
        events.append(object);
    }
    for (const auto& eventId : contextEventIds) {
        const auto exists = std::any_of(
            scenario->events.begin(),
            scenario->events.end(),
            [&eventId](const Event& event) {
                return event.id == eventId;
            });
        if (!exists) {
            QJsonObject missing{
                {QStringLiteral("id"), QString::fromStdString(eventId)},
                {QStringLiteral("missing"), true},
                {QStringLiteral("inWindow"), false},
                {QStringLiteral("laneSelected"), false},
            };
            events.append(missing);
        }
    }

    QJsonArray markers;
    for (const auto& marker : scenario->markers) {
        const auto startsInside = marker.start >= start && marker.start < end;
        const auto overlaps =
            marker.start < end && marker.end > start;
        if (startsInside || overlaps) markers.append(markerObject(marker));
    }

    result.json = {
        {QStringLiteral("schema"), QString::fromLatin1(AutomationReportSchema)},
        {QStringLiteral("command"), QStringLiteral("window")},
        {QStringLiteral("ok"), true},
        {QStringLiteral("projectId"), QString::fromStdString(project.id)},
        {QStringLiteral("scenarioId"), QString::fromStdString(scenario->id)},
        {QStringLiteral("startTick"), integerValue(start)},
        {QStringLiteral("endTick"), integerValue(end)},
        {QStringLiteral("start"),
         QString::fromStdString(formatTick(start, project.timeBase))},
        {QStringLiteral("end"),
         QString::fromStdString(formatTick(end, project.timeBase))},
        {QStringLiteral("laneCount"), lanes.size()},
        {QStringLiteral("clockCount"), clocks.size()},
        {QStringLiteral("eventCount"), events.size()},
        {QStringLiteral("windowEventCount"),
         static_cast<qint64>(windowEventIds.size())},
        {QStringLiteral("relationCount"), relations.size()},
        {QStringLiteral("markerCount"), markers.size()},
        {QStringLiteral("clockDomains"), clocks},
        {QStringLiteral("lanes"), lanes},
        {QStringLiteral("events"), events},
        {QStringLiteral("relations"), relations},
        {QStringLiteral("markers"), markers},
    };
    return result;
}

AutomationDocument findWaveformEdgesForAutomation(
    const Project& project,
    const AutomationEdgeQueryOptions& options,
    const std::optional<std::string>& scenarioId,
    const std::vector<std::string>& laneIds)
{
    AutomationDocument result;
    const auto* scenario =
        automationScenario(project, scenarioId, result.error);
    if (!scenario) return result;
    if (options.limit == 0 || options.limit > 10'000) {
        result.error = QStringLiteral(
            "Edge query limit must be from 1 to 10000.");
        return result;
    }

    const auto start = options.start.value_or(0);
    const auto end = options.end.value_or(scenario->duration);
    if (start < 0 || end <= start || end > scenario->duration) {
        result.error = QStringLiteral(
            "Edge range [%1, %2) is outside Scenario [0, %3).")
                           .arg(start)
                           .arg(end)
                           .arg(scenario->duration);
        return result;
    }

    std::set<std::string> requested;
    for (const auto& selector : laneIds) {
        const auto laneId =
            resolveLaneSelector(*scenario, selector, result.error);
        if (!laneId) return result;
        if (!requested.insert(*laneId).second) {
            result.error = QStringLiteral(
                "Lane filters resolve to duplicate Lane IDs.");
            return result;
        }
        const auto* lane = findLane(*scenario, *laneId);
        if (lane->kind == LaneKind::Clock
            || lane->kind == LaneKind::Group) {
            result.error = QStringLiteral(
                "%1 '%2' has no relation-addressable waveform edges.")
                               .arg(
                                   lane->kind == LaneKind::Clock
                                       ? QStringLiteral("Clock Lane")
                                       : QStringLiteral("Group"),
                                   QString::fromStdString(*laneId));
            return result;
        }
    }

    std::map<std::string, std::size_t> laneIndexes;
    std::size_t selectedLaneCount = 0;
    for (std::size_t index = 0; index < scenario->lanes.size(); ++index) {
        const auto& lane = scenario->lanes.at(index);
        if (lane.kind == LaneKind::Clock || lane.kind == LaneKind::Group) {
            continue;
        }
        if (!requested.empty() && !requested.contains(lane.id)) continue;
        laneIndexes.emplace(lane.id, index);
        ++selectedLaneCount;
    }

    std::map<
        std::pair<Tick, std::size_t>,
        std::vector<const Event*>>
        groupedEvents;
    for (const auto& event : scenario->events) {
        if (!event.waveformLinked
            || event.tick < start
            || event.tick >= end) {
            continue;
        }
        const auto laneIndex = laneIndexes.find(event.laneId);
        if (laneIndex == laneIndexes.end()) continue;
        groupedEvents[{event.tick, laneIndex->second}].push_back(&event);
    }

    QJsonArray edges;
    std::size_t matchCount = 0;
    std::size_t ambiguousEndpointCount = 0;
    for (auto& [location, candidates] : groupedEvents) {
        const auto tick = location.first;
        const auto& lane = scenario->lanes.at(location.second);
        std::sort(
            candidates.begin(),
            candidates.end(),
            [](const Event* left, const Event* right) {
                return left->id < right->id;
            });
        const auto* representative = candidates.front();
        const auto value =
            sampleObject(project, lane, tick).value(QStringLiteral("value"));
        const auto previousValue = tick == 0
            ? QJsonValue{QJsonValue::Null}
            : sampleObject(project, lane, tick - 1)
                  .value(QStringLiteral("value"));
        const auto edgeKind =
            classifyAutomationEdge(lane, tick, previousValue, value);
        if (options.edge && edgeKind != *options.edge) continue;

        ++matchCount;
        const auto endpointResolution =
            resolveRelationEndpoint(*scenario, representative->id);
        const auto relationEndpoint =
            candidates.size() == 1
            && !representative->id.empty()
            && endpointResolution.eventIdCount == 1;
        if (!relationEndpoint) ++ambiguousEndpointCount;
        if (edges.size() >= static_cast<qsizetype>(options.limit)) continue;

        const auto effectiveClockId =
            !representative->clockDomainId.empty()
            ? representative->clockDomainId
            : lane.clockDomainId;
        edges.append(QJsonObject{
            {QStringLiteral("laneId"),
             QString::fromStdString(lane.id)},
            {QStringLiteral("name"),
             QString::fromStdString(lane.name)},
            {QStringLiteral("kind"),
             QString::fromLatin1(toString(lane.kind).data())},
            {QStringLiteral("width"),
             static_cast<qint64>(lane.width)},
            {QStringLiteral("timeTick"), integerValue(tick)},
            {QStringLiteral("time"),
             QString::fromStdString(formatTick(tick, project.timeBase))},
            {QStringLiteral("edge"), automationEdgeKindName(edgeKind)},
            {QStringLiteral("previousValue"), previousValue},
            {QStringLiteral("value"), value},
            {QStringLiteral("action"),
             QString::fromLatin1(
                 toString(representative->action).data())},
            {QStringLiteral("clockDomainId"),
             QString::fromStdString(effectiveClockId)},
            {QStringLiteral("description"),
             QString::fromStdString(representative->description)},
            {QStringLiteral("candidateCount"),
             static_cast<qint64>(candidates.size())},
            {QStringLiteral("eventIdCount"),
             static_cast<qint64>(endpointResolution.eventIdCount)},
            {QStringLiteral("relationEndpoint"),
             relationEndpoint},
        });
    }

    result.json = {
        {QStringLiteral("schema"), QString::fromLatin1(AutomationReportSchema)},
        {QStringLiteral("command"), QStringLiteral("edges")},
        {QStringLiteral("ok"), true},
        {QStringLiteral("projectId"), QString::fromStdString(project.id)},
        {QStringLiteral("scenarioId"), QString::fromStdString(scenario->id)},
        {QStringLiteral("startTick"), integerValue(start)},
        {QStringLiteral("endTick"), integerValue(end)},
        {QStringLiteral("start"),
         QString::fromStdString(formatTick(start, project.timeBase))},
        {QStringLiteral("end"),
         QString::fromStdString(formatTick(end, project.timeBase))},
        {QStringLiteral("edge"),
         options.edge
             ? QJsonValue{automationEdgeKindName(*options.edge)}
             : QJsonValue{QJsonValue::Null}},
        {QStringLiteral("limit"), static_cast<qint64>(options.limit)},
        {QStringLiteral("laneCount"),
         static_cast<qint64>(selectedLaneCount)},
        {QStringLiteral("matchCount"),
         static_cast<qint64>(matchCount)},
        {QStringLiteral("returnedCount"), edges.size()},
        {QStringLiteral("truncated"),
         matchCount > static_cast<std::size_t>(edges.size())},
        {QStringLiteral("ambiguousEndpointCount"),
         static_cast<qint64>(ambiguousEndpointCount)},
        {QStringLiteral("edges"), edges},
    };
    return result;
}

AutomationDocument findRelationsForAutomation(
    const Project& project,
    const AutomationRelationQueryOptions& options,
    const std::optional<std::string>& scenarioId,
    const std::vector<std::string>& laneIds)
{
    AutomationDocument result;
    const auto* scenario =
        automationScenario(project, scenarioId, result.error);
    if (!scenario) return result;
    if (options.limit == 0 || options.limit > 1'000) {
        result.error = QStringLiteral(
            "Relation query limit must be from 1 to 1000.");
        return result;
    }

    const auto query =
        QString::fromStdString(options.match).trimmed();
    if (options.exact && query.isEmpty()) {
        result.error = QStringLiteral(
            "Exact Relation query requires non-empty match text.");
        return result;
    }
    const auto rangeFiltered =
        options.start.has_value() || options.end.has_value();
    const auto start = options.start.value_or(0);
    const auto end = options.end.value_or(scenario->duration);
    if (start < 0 || end <= start || end > scenario->duration) {
        result.error = QStringLiteral(
            "Relation range [%1, %2) is outside Scenario [0, %3).")
                           .arg(start)
                           .arg(end)
                           .arg(scenario->duration);
        return result;
    }

    std::set<std::string> requested;
    for (const auto& selector : laneIds) {
        const auto laneId =
            resolveLaneSelector(*scenario, selector, result.error);
        if (!laneId) return result;
        if (!requested.insert(*laneId).second) {
            result.error = QStringLiteral(
                "Lane filters resolve to duplicate Lane IDs.");
            return result;
        }
    }

    std::map<std::string, std::size_t> idCounts;
    for (const auto& relation : scenario->relations) {
        ++idCounts[relation.id];
    }

    std::vector<const Relation*> orderedRelations;
    orderedRelations.reserve(scenario->relations.size());
    for (const auto& relation : scenario->relations) {
        orderedRelations.push_back(&relation);
    }
    const auto firstEndpointTick =
        [scenario](const Relation& relation) {
        std::optional<Tick> tick;
        for (const auto& eventId :
             {relation.sourceEventId, relation.targetEventId}) {
            const auto endpoint =
                resolveRelationEndpoint(*scenario, eventId);
            if (endpoint.event
                && (!tick || endpoint.event->tick < *tick)) {
                tick = endpoint.event->tick;
            }
        }
        return tick.value_or(std::numeric_limits<Tick>::max());
    };
    std::stable_sort(
        orderedRelations.begin(),
        orderedRelations.end(),
        [&firstEndpointTick](
            const Relation* left,
            const Relation* right) {
            const auto leftTick = firstEndpointTick(*left);
            const auto rightTick = firstEndpointTick(*right);
            return leftTick < rightTick
                || (leftTick == rightTick && left->id < right->id);
        });

    QJsonArray relationResults;
    std::size_t matchCount = 0;
    std::size_t readyCount = 0;
    std::size_t addressableCount = 0;
    std::size_t identityIssueCount = 0;
    std::size_t endpointIssueCount = 0;
    for (const auto* relation : orderedRelations) {
        if (options.severity
            && relation->severity != *options.severity) {
            continue;
        }
        if (!query.isEmpty()) {
            const auto matches =
                [&query, &options](const std::string& value) {
                const auto text = QString::fromStdString(value);
                return options.exact
                    ? text.compare(query, Qt::CaseInsensitive) == 0
                    : text.contains(query, Qt::CaseInsensitive);
            };
            if (!matches(relation->id)
                && !matches(relation->condition)
                && !matches(relation->description)) {
                continue;
            }
        }

        const auto sourceResolution =
            resolveRelationEndpoint(
                *scenario, relation->sourceEventId);
        const auto targetResolution =
            resolveRelationEndpoint(
                *scenario, relation->targetEventId);
        if (!requested.empty()) {
            const auto sourceMatches =
                sourceResolution.event
                && requested.contains(
                    sourceResolution.event->laneId);
            const auto targetMatches =
                targetResolution.event
                && requested.contains(
                    targetResolution.event->laneId);
            if (!sourceMatches && !targetMatches) continue;
        }
        if (rangeFiltered) {
            const auto inside =
                [start, end](const RelationEndpointResolution& endpoint) {
                return endpoint.event
                    && endpoint.event->tick >= start
                    && endpoint.event->tick < end;
            };
            if (!inside(sourceResolution)
                && !inside(targetResolution)) {
                continue;
            }
        }

        const auto relationIndex =
            static_cast<std::size_t>(
                relation - scenario->relations.data());
        const auto relationIdCount =
            idCounts.at(relation->id);
        const auto relationObject =
            relationAutomationObject(
                project,
                *scenario,
                *relation,
                relationIndex,
                relationIdCount);
        const auto sourceReady =
            relationObject
                .value(QStringLiteral("source"))
                .toObject()
                .value(QStringLiteral("relationEndpoint"))
                .toBool();
        const auto targetReady =
            relationObject
                .value(QStringLiteral("target"))
                .toObject()
                .value(QStringLiteral("relationEndpoint"))
                .toBool();
        const auto endpointsReady =
            relationObject
                .value(QStringLiteral("endpointsReady"))
                .toBool();
        const auto addressable =
            relationObject
                .value(QStringLiteral("addressable"))
                .toBool();
        const auto identityIssues =
            relationObject
                .value(QStringLiteral("identityIssues"))
                .toArray();
        ++matchCount;
        if (endpointsReady) ++readyCount;
        if (addressable) ++addressableCount;
        identityIssueCount += static_cast<std::size_t>(
            identityIssues.size());
        endpointIssueCount +=
            static_cast<std::size_t>(!sourceReady)
            + static_cast<std::size_t>(!targetReady);
        if (relationResults.size()
            >= static_cast<qsizetype>(options.limit)) {
            continue;
        }

        relationResults.append(relationObject);
    }

    result.json = {
        {QStringLiteral("schema"), QString::fromLatin1(AutomationReportSchema)},
        {QStringLiteral("command"), QStringLiteral("relations")},
        {QStringLiteral("ok"), true},
        {QStringLiteral("projectId"), QString::fromStdString(project.id)},
        {QStringLiteral("scenarioId"), QString::fromStdString(scenario->id)},
        {QStringLiteral("query"), query},
        {QStringLiteral("exact"), options.exact},
        {QStringLiteral("severity"),
         options.severity
             ? QJsonValue{
                   QString::fromLatin1(
                       toString(*options.severity).data())}
             : QJsonValue{QJsonValue::Null}},
        {QStringLiteral("rangeFiltered"), rangeFiltered},
        {QStringLiteral("startTick"), integerValue(start)},
        {QStringLiteral("endTick"), integerValue(end)},
        {QStringLiteral("start"),
         QString::fromStdString(formatTick(start, project.timeBase))},
        {QStringLiteral("end"),
         QString::fromStdString(formatTick(end, project.timeBase))},
        {QStringLiteral("limit"), static_cast<qint64>(options.limit)},
        {QStringLiteral("laneFilterCount"),
         static_cast<qint64>(requested.size())},
        {QStringLiteral("scenarioRelationCount"),
         static_cast<qint64>(scenario->relations.size())},
        {QStringLiteral("matchCount"),
         static_cast<qint64>(matchCount)},
        {QStringLiteral("returnedCount"), relationResults.size()},
        {QStringLiteral("truncated"),
         matchCount > static_cast<std::size_t>(relationResults.size())},
        {QStringLiteral("readyCount"),
         static_cast<qint64>(readyCount)},
        {QStringLiteral("addressableCount"),
         static_cast<qint64>(addressableCount)},
        {QStringLiteral("identityIssueCount"),
         static_cast<qint64>(identityIssueCount)},
        {QStringLiteral("endpointIssueCount"),
         static_cast<qint64>(endpointIssueCount)},
        {QStringLiteral("relations"), relationResults},
    };
    return result;
}

AutomationDocument findMarkersForAutomation(
    const Project& project,
    const AutomationMarkerQueryOptions& options,
    const std::optional<std::string>& scenarioId)
{
    AutomationDocument result;
    const auto* scenario =
        automationScenario(project, scenarioId, result.error);
    if (!scenario) return result;
    if (options.limit == 0 || options.limit > 1'000) {
        result.error = QStringLiteral(
            "Marker query limit must be from 1 to 1000.");
        return result;
    }

    const auto query =
        QString::fromStdString(options.match).trimmed();
    if (options.exact && query.isEmpty()) {
        result.error = QStringLiteral(
            "Exact Marker query requires non-empty match text.");
        return result;
    }
    const auto rangeFiltered =
        options.start.has_value() || options.end.has_value();
    const auto start = options.start.value_or(0);
    const auto end = options.end.value_or(scenario->duration);
    if (start < 0 || end <= start || end > scenario->duration) {
        result.error = QStringLiteral(
            "Marker range [%1, %2) is outside Scenario [0, %3).")
                           .arg(start)
                           .arg(end)
                           .arg(scenario->duration);
        return result;
    }

    std::map<std::string, std::size_t> idCounts;
    std::map<QString, std::size_t> nameCounts;
    for (const auto& marker : scenario->markers) {
        ++idCounts[marker.id];
        ++nameCounts[
            QString::fromStdString(marker.name)
                .trimmed()
                .toCaseFolded()];
    }

    std::vector<const Marker*> orderedMarkers;
    orderedMarkers.reserve(scenario->markers.size());
    for (const auto& marker : scenario->markers) {
        orderedMarkers.push_back(&marker);
    }
    std::stable_sort(
        orderedMarkers.begin(),
        orderedMarkers.end(),
        [](const Marker* left, const Marker* right) {
            return left->start < right->start
                || (left->start == right->start
                    && (left->end < right->end
                        || (left->end == right->end
                            && left->id < right->id)));
        });

    QJsonArray markerResults;
    std::size_t matchCount = 0;
    std::size_t validCount = 0;
    std::size_t addressableCount = 0;
    std::size_t issueCount = 0;
    for (const auto* marker : orderedMarkers) {
        if (options.kind && marker->kind != *options.kind) continue;
        if (!query.isEmpty()) {
            const auto matches =
                [&query, &options](const std::string& value) {
                const auto text = QString::fromStdString(value);
                return options.exact
                    ? text.compare(query, Qt::CaseInsensitive) == 0
                    : text.contains(query, Qt::CaseInsensitive);
            };
            if (!matches(marker->id)
                && !matches(marker->name)
                && !matches(marker->note)) {
                continue;
            }
        }
        if (rangeFiltered) {
            const auto point = marker->start == marker->end;
            const auto intersects = point
                ? marker->start >= start && marker->start < end
                : marker->start < end && marker->end > start;
            if (!intersects) continue;
        }

        QJsonArray issues;
        const auto appendIssue =
            [&issues](const char* issue) {
                issues.append(QString::fromLatin1(issue));
            };
        const auto markerIdCount = idCounts.at(marker->id);
        const auto markerName =
            QString::fromStdString(marker->name).trimmed();
        const auto markerNameCount =
            nameCounts.at(markerName.toCaseFolded());
        if (marker->id.empty()) appendIssue("missing-id");
        if (markerIdCount > 1) appendIssue("duplicate-id");
        for (const auto& problem :
             markerIntegrityProblems(
                 *scenario, *marker, markerNameCount)) {
            issues.append(problem.code);
        }

        const auto addressable =
            !marker->id.empty() && markerIdCount == 1;
        const auto valid = issues.isEmpty();
        ++matchCount;
        if (valid) ++validCount;
        if (addressable) ++addressableCount;
        issueCount += static_cast<std::size_t>(issues.size());
        if (markerResults.size()
            >= static_cast<qsizetype>(options.limit)) {
            continue;
        }

        const auto geometryValid =
            marker->start >= 0
            && marker->end >= marker->start
            && marker->end <= scenario->duration;
        const auto duration = geometryValid
            ? std::optional<Tick>{marker->end - marker->start}
            : std::nullopt;
        const auto markerIndex = static_cast<std::size_t>(
            marker - scenario->markers.data());
        markerResults.append(QJsonObject{
            {QStringLiteral("markerId"),
             QString::fromStdString(marker->id)},
            {QStringLiteral("markerRef"),
             QString::fromStdString(
                 markerRepairRef(*scenario, markerIndex))},
            {QStringLiteral("name"),
             QString::fromStdString(marker->name)},
            {QStringLiteral("kind"),
             QString::fromLatin1(toString(marker->kind).data())},
            {QStringLiteral("note"),
             QString::fromStdString(marker->note)},
            {QStringLiteral("startTick"), integerValue(marker->start)},
            {QStringLiteral("endTick"), integerValue(marker->end)},
            {QStringLiteral("durationTick"),
             duration
                 ? integerValue(*duration)
                 : QJsonValue{QJsonValue::Null}},
            {QStringLiteral("start"),
             QString::fromStdString(
                 formatTick(marker->start, project.timeBase))},
            {QStringLiteral("end"),
             QString::fromStdString(
                 formatTick(marker->end, project.timeBase))},
            {QStringLiteral("duration"),
             duration
                 ? QJsonValue{QString::fromStdString(
                       formatTick(*duration, project.timeBase))}
                 : QJsonValue{QJsonValue::Null}},
            {QStringLiteral("point"), marker->start == marker->end},
            {QStringLiteral("idCount"),
             static_cast<qint64>(markerIdCount)},
            {QStringLiteral("nameCount"),
             static_cast<qint64>(markerNameCount)},
            {QStringLiteral("addressable"), addressable},
            {QStringLiteral("valid"), valid},
            {QStringLiteral("issues"), issues},
        });
    }

    result.json = {
        {QStringLiteral("schema"), QString::fromLatin1(AutomationReportSchema)},
        {QStringLiteral("command"), QStringLiteral("markers")},
        {QStringLiteral("ok"), true},
        {QStringLiteral("projectId"), QString::fromStdString(project.id)},
        {QStringLiteral("scenarioId"), QString::fromStdString(scenario->id)},
        {QStringLiteral("query"), query},
        {QStringLiteral("exact"), options.exact},
        {QStringLiteral("kind"),
         options.kind
             ? QJsonValue{
                   QString::fromLatin1(toString(*options.kind).data())}
             : QJsonValue{QJsonValue::Null}},
        {QStringLiteral("rangeFiltered"), rangeFiltered},
        {QStringLiteral("startTick"), integerValue(start)},
        {QStringLiteral("endTick"), integerValue(end)},
        {QStringLiteral("start"),
         QString::fromStdString(formatTick(start, project.timeBase))},
        {QStringLiteral("end"),
         QString::fromStdString(formatTick(end, project.timeBase))},
        {QStringLiteral("limit"), static_cast<qint64>(options.limit)},
        {QStringLiteral("scenarioMarkerCount"),
         static_cast<qint64>(scenario->markers.size())},
        {QStringLiteral("matchCount"),
         static_cast<qint64>(matchCount)},
        {QStringLiteral("returnedCount"), markerResults.size()},
        {QStringLiteral("truncated"),
         matchCount > static_cast<std::size_t>(markerResults.size())},
        {QStringLiteral("validCount"),
         static_cast<qint64>(validCount)},
        {QStringLiteral("addressableCount"),
         static_cast<qint64>(addressableCount)},
        {QStringLiteral("issueCount"),
         static_cast<qint64>(issueCount)},
        {QStringLiteral("markers"), markerResults},
    };
    return result;
}

AutomationProjectResult createProjectForAutomation(
    const AutomationNewProjectOptions& options)
{
    AutomationProjectResult result;
    const auto projectName =
        QString::fromStdString(options.projectName).trimmed();
    const auto scenarioName =
        QString::fromStdString(options.scenarioName).trimmed();
    if (projectName.isEmpty() || scenarioName.isEmpty()) {
        result.error = QStringLiteral(
            "Project and Scenario names must be non-empty.");
        return result;
    }
    if (!options.timeBase.isValid()) {
        result.error = QStringLiteral("Project timebase is invalid.");
        return result;
    }
    if (options.duration <= 0) {
        result.error = QStringLiteral("Scenario duration must be positive.");
        return result;
    }

    Project project;
    project.name = projectName.toStdString();
    project.timeBase = options.timeBase;
    project.id = QString::fromStdString(options.projectId)
                     .trimmed()
                     .toStdString();
    if (project.id.empty()) {
        project.id = deterministicStableId(
            "project",
            project.name + ":"
                + std::to_string(project.timeBase.picosecondsPerTick),
            [](const std::string&) {
                return false;
            });
    }

    Scenario scenario;
    scenario.name = scenarioName.toStdString();
    scenario.duration = options.duration;
    scenario.id = QString::fromStdString(options.scenarioId)
                      .trimmed()
                      .toStdString();
    if (scenario.id.empty()) {
        scenario.id = deterministicStableId(
            "scenario",
            project.id + ":" + scenario.name + ":"
                + std::to_string(scenario.duration),
            [](const std::string&) {
                return false;
            });
    }
    project.scenarios.push_back(std::move(scenario));

    const auto inspection = inspectProjectForAutomation(
        project,
        project.scenarios.front().id,
        AutomationInspectDetail::Summary);
    const auto validation = validateProjectForAutomation(
        project, project.scenarios.front().id);
    if (!inspection.ok() || !validation.ok()) {
        result.error = !inspection.ok() ? inspection.error : validation.error;
        return result;
    }
    result.json = {
        {QStringLiteral("schema"), QString::fromLatin1(AutomationReportSchema)},
        {QStringLiteral("command"), QStringLiteral("new")},
        {QStringLiteral("ok"), true},
        {QStringLiteral("projectId"), QString::fromStdString(project.id)},
        {QStringLiteral("scenarioId"),
         QString::fromStdString(project.scenarios.front().id)},
        {QStringLiteral("project"),
         inspection.json.value(QStringLiteral("project"))},
        {QStringLiteral("validation"), validation.json},
    };
    result.project = std::move(project);
    return result;
}

AutomationApplyResult applyAutomationBatch(
    const Project& source,
    const QJsonObject& batch,
    const std::optional<std::string>& scenarioOverride)
{
    AutomationApplyResult result;
    QString error;
    if (!containsOnly(
            batch,
            {QStringLiteral("schema"),
             QStringLiteral("expectedProjectId"),
             QStringLiteral("scenarioId"),
             QStringLiteral("operations")},
            error)) {
        result.error = error;
        return result;
    }
    if (batch.value(QStringLiteral("schema")).toString()
        != QString::fromLatin1(AutomationBatchSchema)) {
        result.error = QStringLiteral(
            "Field 'schema' must be '%1'.")
                           .arg(QString::fromLatin1(AutomationBatchSchema));
        return result;
    }
    if (batch.contains(QStringLiteral("expectedProjectId"))) {
        const auto expected = requiredString(
            batch, QStringLiteral("expectedProjectId"), error);
        if (!expected) {
            result.error = error;
            return result;
        }
        if (*expected != source.id) {
            result.error = QStringLiteral(
                "expectedProjectId does not match the loaded project.");
            return result;
        }
    }
    const auto scenarioId = scenarioIdFromBatch(
        source, batch, scenarioOverride, error);
    if (!scenarioId) {
        result.error = error;
        return result;
    }
    const auto operationsValue = batch.value(QStringLiteral("operations"));
    if (!operationsValue.isArray() || operationsValue.toArray().isEmpty()) {
        result.error = QStringLiteral(
            "Field 'operations' must be a non-empty array.");
        return result;
    }

    auto candidate = source;
    auto activeScenarioId = *scenarioId;
    if (!scenarioIndexByStableId(candidate, activeScenarioId)) {
        result.error = QStringLiteral("Scenario '%1' does not exist.")
                           .arg(QString::fromStdString(activeScenarioId));
        return result;
    }
    CommandStack stack;
    QJsonArray operationResults;
    const auto operations = operationsValue.toArray();
    for (qsizetype index = 0; index < operations.size(); ++index) {
        const auto value = operations.at(index);
        result.failedOperation = static_cast<int>(index);
        if (!value.isObject()) {
            result.error = QStringLiteral("Operation %1 must be an object.")
                               .arg(index);
            return result;
        }
        auto operation = value.toObject();
        const auto operationName = operation.value(QStringLiteral("op"));
        if (!operationName.isString()
            || operationName.toString().trimmed().isEmpty()) {
            result.error = QStringLiteral(
                "Operation %1 requires a non-empty 'op'.")
                               .arg(index);
            return result;
        }
        const auto name = operationName.toString();
        const auto activeScenarioIndex = scenarioIndexByStableId(
            candidate, activeScenarioId);
        if (!activeScenarioIndex) {
            result.error = QStringLiteral(
                "Operation %1 (%2): the active Scenario no longer exists.")
                               .arg(index)
                               .arg(name);
            return result;
        }
        auto* scenario = &candidate.scenarios.at(*activeScenarioIndex);
        if (!canonicalizeOperationSelectors(
                candidate,
                *scenario,
                name,
                operation,
                error)) {
            result.error = QStringLiteral("Operation %1 (%2): %3")
                               .arg(index)
                               .arg(name, error);
            return result;
        }
        auto operationReport = operationResult(
            static_cast<int>(index), name, false);
        try {
            auto applied = false;
            if (name == QStringLiteral("create-scenario")) {
                applied = applyCreateScenario(
                    candidate,
                    activeScenarioId,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("duplicate-scenario")) {
                applied = applyDuplicateScenario(
                    candidate,
                    activeScenarioId,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("rename-scenario")) {
                applied = applyRenameScenario(
                    candidate,
                    activeScenarioId,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("delete-scenario")) {
                applied = applyDeleteScenario(
                    candidate,
                    activeScenarioId,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("reorder-scenario")) {
                applied = applyReorderScenario(
                    candidate,
                    activeScenarioId,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("set-range")) {
                applied = applySetRange(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("set-sequence")) {
                applied = applySetSequence(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("assert-value")) {
                applied = applyAssertValue(
                    candidate,
                    *scenario,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("clear-range")) {
                applied = applyClearRange(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("transfer-range")) {
                applied = applyTransferRange(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("rename-lane")) {
                applied = applyRenameLane(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("set-duration")) {
                applied = applySetDuration(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("add-signal")) {
                applied = applyAddSignal(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("duplicate-signal")) {
                applied = applyDuplicateSignal(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("add-group")) {
                applied = applyAddGroup(
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("update-group")) {
                applied = applyUpdateGroup(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("move-group")) {
                applied = applyMoveGroup(
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("delete-group")) {
                applied = applyDeleteGroup(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("delete-signal")) {
                applied = applyDeleteSignal(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("move-signal")) {
                applied = applyMoveSignal(
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("update-signal")) {
                applied = applyUpdateSignal(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("update-clock")) {
                applied = applyUpdateClock(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("add-relation")) {
                applied = applyAddRelation(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("update-relation")) {
                applied = applyUpdateRelation(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("delete-relation")) {
                applied = applyDeleteRelation(
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("add-marker")) {
                applied = applyAddMarker(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("update-marker")) {
                applied = applyUpdateMarker(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("delete-marker")) {
                applied = applyDeleteMarker(
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name == QStringLiteral("delete-event")) {
                applied = applyDeleteEvent(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name
                       == QStringLiteral(
                           "repair-event-link")) {
                applied = applyRepairEventLink(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name
                       == QStringLiteral(
                           "clear-event-cycle")) {
                applied = applyClearEventCycle(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name
                       == QStringLiteral(
                           "repair-event-clock")) {
                applied = applyRepairEventClock(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name
                       == QStringLiteral(
                           "repair-relation-clock")) {
                applied = applyRepairRelationClock(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name
                       == QStringLiteral(
                           "repair-trace-identity")) {
                applied = applyRepairTraceIdentity(
                    candidate,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name
                       == QStringLiteral(
                           "repair-trace-reference")) {
                applied = applyRepairTraceReference(
                    candidate,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name
                       == QStringLiteral(
                           "repair-trace-mapping")) {
                applied = applyRepairTraceMapping(
                    candidate,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name
                       == QStringLiteral(
                           "repair-lane-clock")) {
                applied = applyRepairLaneClock(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            } else if (name
                       == QStringLiteral(
                           "repair-lane-group")) {
                applied = applyRepairLaneGroup(
                    candidate,
                    *scenario,
                    stack,
                    operation,
                    operationReport,
                    error);
            }
            if (!applied) {
                if (error.isEmpty()) {
                    error = QStringLiteral("Unknown operation '%1'.").arg(name);
                }
                result.error = QStringLiteral("Operation %1 (%2): %3")
                                   .arg(index)
                                   .arg(name, error);
                return result;
            }
        } catch (const std::exception& exception) {
            result.error = QStringLiteral("Operation %1 (%2): %3")
                               .arg(index)
                               .arg(name, QString::fromUtf8(exception.what()));
            return result;
        }
        operationResults.append(operationReport);
    }

    canonicalizeGeneratedIdentities(source, candidate);
    result.failedOperation = -1;
    const auto candidateChanged = candidate != source;
    const auto sourceValidation =
        validateProjectForAutomation(source);
    const auto validation =
        validateProjectForAutomation(candidate);
    const auto countFromValidation =
        [](const AutomationDocument& document,
           const QString& container,
           const QString& field) {
            if (!document.ok()) return std::uint64_t{0};
            const auto value = document.json
                .value(container)
                .toObject()
                .value(field);
            bool converted = false;
            const auto count = value.toString().toULongLong(&converted);
            return converted ? static_cast<std::uint64_t>(count)
                             : static_cast<std::uint64_t>(
                                   value.toInteger());
        };
    const auto directCount =
        [](const AutomationDocument& document,
           const QString& field) {
            if (!document.ok()) return std::uint64_t{0};
            const auto value = document.json.value(field);
            bool converted = false;
            const auto count = value.toString().toULongLong(&converted);
            return converted ? static_cast<std::uint64_t>(count)
                             : static_cast<std::uint64_t>(
                                   value.toInteger());
        };
    const auto sourceErrorCount =
        countFromValidation(
            sourceValidation,
            QStringLiteral("summary"),
            QStringLiteral("errors"));
    const auto candidateErrorCount =
        countFromValidation(
            validation,
            QStringLiteral("summary"),
            QStringLiteral("errors"));
    const auto sourceIdentityIssueCount =
        directCount(
            sourceValidation,
            QStringLiteral("identityIssueCount"));
    const auto candidateIdentityIssueCount =
        directCount(
            validation,
            QStringLiteral("identityIssueCount"));
    const auto repairOperationReported =
        std::any_of(
            operationResults.begin(),
            operationResults.end(),
            [](const QJsonValue& value) {
                const auto operation = value.toObject();
                return operation.value(
                           QStringLiteral("repairedIdentity")).toBool()
                    || operation.value(
                           QStringLiteral("repairedGeometry")).toBool()
                    || operation.value(
                           QStringLiteral("repairedSourceEndpoint")).toBool()
                    || operation.value(
                           QStringLiteral("repairedTargetEndpoint")).toBool();
            });
    const auto errorFingerprints =
        [](const AutomationDocument& document) {
            std::map<QString, std::uint64_t> fingerprints;
            if (!document.ok()) return fingerprints;
            for (const auto& value :
                 document.json.value(
                     QStringLiteral("issues")).toArray()) {
                const auto issue = value.toObject();
                if (issue.value(
                        QStringLiteral("severity")).toString()
                    != QStringLiteral("error")) {
                    continue;
                }
                const auto fingerprint = QStringLiteral(
                    "%1|%2|%3|%4|%5|%6|%7|%8")
                    .arg(
                        issue.value(
                            QStringLiteral("code")).toString(),
                        issue.value(
                            QStringLiteral("objectKind")).toString(),
                        issue.value(
                            QStringLiteral("stableId")).toString(),
                        issue.value(
                            QStringLiteral("scenarioId")).toString(),
                        issue.value(
                            QStringLiteral("laneId")).toString(),
                        issue.value(
                            QStringLiteral("tick")).toString(),
                        issue.value(
                            QStringLiteral("eventId")).toString(),
                        issue.value(
                            QStringLiteral("relationId")).toString());
                ++fingerprints[fingerprint];
            }
            return fingerprints;
        };
    const auto sourceErrorFingerprints =
        errorFingerprints(sourceValidation);
    const auto candidateErrorFingerprints =
        errorFingerprints(validation);
    std::uint64_t newErrorCount = 0;
    std::uint64_t resolvedErrorCount = 0;
    for (const auto& [fingerprint, count] :
         candidateErrorFingerprints) {
        const auto sourceIterator =
            sourceErrorFingerprints.find(fingerprint);
        const auto sourceCount =
            sourceIterator == sourceErrorFingerprints.end()
            ? std::uint64_t{0}
            : sourceIterator->second;
        if (count > sourceCount) {
            newErrorCount += count - sourceCount;
        }
    }
    for (const auto& [fingerprint, count] :
         sourceErrorFingerprints) {
        const auto candidateIterator =
            candidateErrorFingerprints.find(fingerprint);
        const auto candidateCount =
            candidateIterator == candidateErrorFingerprints.end()
            ? std::uint64_t{0}
            : candidateIterator->second;
        if (count > candidateCount) {
            resolvedErrorCount += count - candidateCount;
        }
    }
    const auto noNewErrors = newErrorCount == 0;
    const auto candidateValid =
        validation.ok()
        && validation.json.value(QStringLiteral("valid")).toBool();
    const auto progressiveRepair =
        !candidateValid
        && sourceValidation.ok()
        && sourceErrorCount > 0
        && candidateErrorCount <= sourceErrorCount
        && candidateIdentityIssueCount <= sourceIdentityIssueCount
        && noNewErrors
        && (candidateErrorCount < sourceErrorCount
            || repairOperationReported);
    const auto validationAccepted =
        candidateValid || progressiveRepair;
    const QJsonObject validationGuard{
        {QStringLiteral("scope"), QStringLiteral("project")},
        {QStringLiteral("accepted"), validationAccepted},
        {QStringLiteral("sourceValid"),
         sourceValidation.ok()
             && sourceValidation.json
                    .value(QStringLiteral("valid")).toBool()},
        {QStringLiteral("candidateValid"), candidateValid},
        {QStringLiteral("progressiveRepair"), progressiveRepair},
        {QStringLiteral("repairOperationReported"),
         repairOperationReported},
        {QStringLiteral("noNewErrors"), noNewErrors},
        {QStringLiteral("newErrorCount"),
         unsignedIntegerValue(newErrorCount)},
        {QStringLiteral("resolvedErrorCount"),
         unsignedIntegerValue(resolvedErrorCount)},
        {QStringLiteral("sourceErrorCount"),
         unsignedIntegerValue(sourceErrorCount)},
        {QStringLiteral("candidateErrorCount"),
         unsignedIntegerValue(candidateErrorCount)},
        {QStringLiteral("sourceIdentityIssueCount"),
         unsignedIntegerValue(sourceIdentityIssueCount)},
        {QStringLiteral("candidateIdentityIssueCount"),
         unsignedIntegerValue(candidateIdentityIssueCount)},
        {QStringLiteral("reason"),
         candidateValid
             ? QStringLiteral("valid-candidate")
             : progressiveRepair
                 ? QStringLiteral("progressive-repair")
                 : QStringLiteral("invalid-candidate")},
    };
    if (!validationAccepted) {
        if (validation.ok()) {
            result.error = QStringLiteral(
                "Post-edit validation rejected a non-improving invalid candidate; no project result was produced.");
            const auto issues =
                validation.json.value(QStringLiteral("issues")).toArray();
            if (!issues.isEmpty()) {
                const auto firstIssue = issues.at(0).toObject();
                const auto code =
                    firstIssue.value(QStringLiteral("code")).toString();
                const auto path =
                    firstIssue.value(QStringLiteral("path")).toString();
                const auto message =
                    firstIssue.value(QStringLiteral("message")).toString();
                result.error += QStringLiteral(" First issue: %1%2%3")
                    .arg(
                        code,
                        path.isEmpty()
                            ? QString{}
                            : QStringLiteral(" at %1").arg(path),
                        message.isEmpty()
                            ? QStringLiteral(".")
                            : QStringLiteral(": %1").arg(message));
            }
        } else {
            result.error = QStringLiteral("Post-edit validation failed: %1")
                               .arg(validation.error);
        }
        result.json = {
            {QStringLiteral("schema"),
             QString::fromLatin1(AutomationReportSchema)},
            {QStringLiteral("command"), QStringLiteral("apply")},
            {QStringLiteral("ok"), false},
            {QStringLiteral("candidateChanged"), candidateChanged},
            {QStringLiteral("projectId"),
             QString::fromStdString(candidate.id)},
            {QStringLiteral("scenarioId"),
             QString::fromStdString(activeScenarioId)},
            {QStringLiteral("operationCount"),
             static_cast<qint64>(operations.size())},
            {QStringLiteral("operations"), operationResults},
            {QStringLiteral("validation"), validation.json},
            {QStringLiteral("validationGuard"), validationGuard},
            {QStringLiteral("error"),
             QJsonObject{
                 {QStringLiteral("code"),
                  QStringLiteral("post-validation-failed")},
                 {QStringLiteral("message"), result.error},
             }},
        };
        return result;
    }

    result.changed = candidateChanged;
    result.json = {
        {QStringLiteral("schema"), QString::fromLatin1(AutomationReportSchema)},
        {QStringLiteral("command"), QStringLiteral("apply")},
        {QStringLiteral("ok"), true},
        {QStringLiteral("changed"), result.changed},
        {QStringLiteral("projectId"), QString::fromStdString(candidate.id)},
        {QStringLiteral("scenarioId"), QString::fromStdString(activeScenarioId)},
        {QStringLiteral("operationCount"),
         static_cast<qint64>(operations.size())},
        {QStringLiteral("operations"), operationResults},
        {QStringLiteral("changes"),
         projectChangeSummary(source, candidate)},
        {QStringLiteral("validation"), validation.json},
        {QStringLiteral("validationGuard"), validationGuard},
    };
    result.project = std::move(candidate);
    return result;
}

} // namespace wave
