#include "wave/integration.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrlQuery>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace wave {
namespace {

QString requiredString(
    const QJsonObject& object,
    const QString& name,
    QString& error)
{
    const auto value = object.value(name);
    if (!value.isString() || value.toString().isEmpty()) {
        error = QStringLiteral("%1 must be a non-empty string").arg(name);
        return {};
    }
    return value.toString();
}

std::string stdString(const QString& value)
{
    return value.toUtf8().toStdString();
}

QString qString(const std::string& value)
{
    return QString::fromUtf8(value);
}

QString optionalString(const QJsonObject& object, const QString& name, QString& error)
{
    const auto value = object.value(name);
    if (value.isUndefined() || value.isNull()) return {};
    if (!value.isString()) {
        error = QStringLiteral("%1 must be a string").arg(name);
        return {};
    }
    return value.toString();
}

bool validEncodedBytes(const std::string& value)
{
    auto text = value;
    if (text.starts_with("0x") || text.starts_with("0X")) text.erase(0, 2);
    text.erase(
        std::remove_if(text.begin(), text.end(), [](const unsigned char character) {
            return std::isspace(character) != 0 || character == '_';
        }),
        text.end());
    return !text.empty() && (text.size() % 2) == 0
        && std::all_of(text.begin(), text.end(), [](const unsigned char character) {
            return std::isxdigit(character) != 0;
        });
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

QJsonObject laneDefinitionJson(const Lane& lane)
{
    QJsonObject object;
    object.insert(QStringLiteral("stableId"), qString(lane.id));
    object.insert(QStringLiteral("name"), qString(lane.name));
    object.insert(QStringLiteral("kind"), QString::fromLatin1(toString(lane.kind).data()));
    object.insert(QStringLiteral("width"), static_cast<qint64>(lane.width));
    object.insert(QStringLiteral("signed"), lane.isSigned);
    object.insert(QStringLiteral("clockDomainId"), qString(lane.clockDomainId));
    return object;
}

} // namespace

SignalListParseResult parseZeroSlackSignalList(const QByteArray& document)
{
    SignalListParseResult result;
    QJsonParseError parseError;
    const auto parsed = QJsonDocument::fromJson(document, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        result.error = QStringLiteral("Invalid ZeroSlack signal-list JSON: %1")
                           .arg(parseError.errorString());
        return result;
    }
    const auto object = parsed.object();
    if (object.value(QStringLiteral("schemaVersion")).toInt(-1)
        != ZeroSlackSignalList::CurrentSchemaVersion) {
        result.error = QStringLiteral("Unsupported ZeroSlack signal-list schemaVersion");
        return result;
    }
    QString error;
    ZeroSlackSignalList signalList;
    signalList.sourceProjectId = stdString(
        requiredString(object, QStringLiteral("sourceProjectId"), error));
    if (!error.isEmpty()) {
        result.error = error;
        return result;
    }
    const auto definitions = object.value(QStringLiteral("signals"));
    if (!definitions.isArray()) {
        result.error = QStringLiteral("signals must be an array");
        return result;
    }
    for (const auto& value : definitions.toArray()) {
        if (!value.isObject()) {
            result.error = QStringLiteral("Each signals entry must be an object");
            return result;
        }
        const auto definitionObject = value.toObject();
        ExternalSignalDefinition definition;
        definition.stableId = stdString(
            requiredString(definitionObject, QStringLiteral("stableId"), error));
        definition.name = stdString(
            requiredString(definitionObject, QStringLiteral("name"), error));
        const auto kindText = requiredString(
            definitionObject,
            QStringLiteral("kind"),
            error);
        if (!error.isEmpty()) {
            result.error = error;
            return result;
        }
        const auto kind = laneKindFromString(stdString(kindText));
        if (!kind) {
            result.error = QStringLiteral("Unknown signal kind: %1").arg(kindText);
            return result;
        }
        definition.kind = *kind;
        const auto widthValue = definitionObject.value(QStringLiteral("width"));
        if (!widthValue.isDouble()
            || widthValue.toDouble() < 1
            || widthValue.toDouble() > std::numeric_limits<std::uint32_t>::max()
            || widthValue.toDouble() != std::floor(widthValue.toDouble())) {
            result.error = QStringLiteral("Signal width must be a positive integer");
            return result;
        }
        definition.width = static_cast<std::uint32_t>(widthValue.toDouble());
        const auto signedValue = definitionObject.value(QStringLiteral("signed"));
        if (!signedValue.isUndefined() && !signedValue.isBool()) {
            result.error = QStringLiteral("Signal signed must be boolean");
            return result;
        }
        definition.isSigned = signedValue.toBool(false);
        definition.clockDomainId = stdString(optionalString(
            definitionObject,
            QStringLiteral("clockDomainId"),
            error));
        if (!error.isEmpty()) {
            result.error = error;
            return result;
        }
        signalList.signalDefinitions.push_back(std::move(definition));
    }
    result.signalList = std::move(signalList);
    return result;
}

SignalImportResult applyZeroSlackSignalList(
    Project& project,
    Scenario& scenario,
    const ZeroSlackSignalList& signalList)
{
    SignalImportResult result;
    for (const auto& definition : signalList.signalDefinitions) {
        const auto existing = findLane(scenario, definition.stableId);
        if (existing) {
            ++result.existing;
            if (existing->width != definition.width || existing->kind != definition.kind) {
                result.diagnostics.append(
                    QStringLiteral("Existing lane %1 has a different kind or width; preserved unchanged.")
                        .arg(qString(definition.stableId)));
            }
            continue;
        }
        Lane lane;
        lane.id = definition.stableId;
        lane.name = definition.name;
        lane.kind = definition.kind;
        lane.width = definition.width;
        lane.isSigned = definition.isSigned;
        lane.clockDomainId = definition.clockDomainId;
        lane.color = "#4fc3f7";
        lane.height = 56;
        lane.visible = true;
        lane.extensions.emplace(
            "sourceApplication",
            jsonStringValue("ZeroSlack"));
        lane.extensions.emplace(
            "sourceProjectId",
            jsonStringValue(signalList.sourceProjectId));
        scenario.lanes.push_back(std::move(lane));
        ++result.added;
    }
    project.linkedResources.erase(
        std::remove_if(
            project.linkedResources.begin(),
            project.linkedResources.end(),
            [](const LinkedResource& resource) {
                return resource.kind == "zeroslack-signal-list";
            }),
        project.linkedResources.end());
    project.linkedResources.push_back({
        "zeroslack-signal-list",
        {},
        signalList.sourceProjectId,
        {},
        "Imported ZeroSlack signal definitions",
        {},
    });
    return result;
}

FrameReferenceParseResult parseFrameSampleReference(const QByteArray& document)
{
    FrameReferenceParseResult result;
    QJsonParseError parseError;
    const auto parsed = QJsonDocument::fromJson(document, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        result.error = QStringLiteral("Invalid frame-reference JSON: %1")
                           .arg(parseError.errorString());
        return result;
    }
    const auto object = parsed.object();
    if (object.value(QStringLiteral("schemaVersion")).toInt(-1)
        != FrameSampleReference::CurrentSchemaVersion) {
        result.error = QStringLiteral("Unsupported frame-reference schemaVersion");
        return result;
    }
    QString error;
    FrameSampleReference reference;
    reference.frameProjectId = stdString(
        requiredString(object, QStringLiteral("frameProjectId"), error));
    reference.frameId = stdString(requiredString(object, QStringLiteral("frameId"), error));
    reference.sampleId = stdString(requiredString(object, QStringLiteral("sampleId"), error));
    reference.encodedBytes = stdString(
        requiredString(object, QStringLiteral("encodedBytes"), error));
    reference.contentHash = stdString(
        requiredString(object, QStringLiteral("contentHash"), error));
    reference.summary = stdString(requiredString(object, QStringLiteral("summary"), error));
    reference.sourcePath = stdString(optionalString(object, QStringLiteral("sourcePath"), error));
    reference.scenarioId = stdString(optionalString(object, QStringLiteral("scenarioId"), error));
    reference.laneId = stdString(optionalString(object, QStringLiteral("laneId"), error));
    reference.segmentId = stdString(optionalString(object, QStringLiteral("segmentId"), error));
    if (!error.isEmpty()) {
        result.error = error;
        return result;
    }
    if (!validEncodedBytes(reference.encodedBytes)) {
        result.error = QStringLiteral("encodedBytes must contain an even number of hexadecimal digits");
        return result;
    }
    const auto hasAnyTarget = !reference.scenarioId.empty()
        || !reference.laneId.empty()
        || !reference.segmentId.empty();
    const auto hasCompleteTarget = !reference.scenarioId.empty()
        && !reference.laneId.empty()
        && !reference.segmentId.empty();
    if (hasAnyTarget && !hasCompleteTarget) {
        result.error = QStringLiteral(
            "scenarioId, laneId, and segmentId must be supplied together");
        return result;
    }
    result.reference = std::move(reference);
    return result;
}

FrameLinkResult linkFrameSample(
    Project& project,
    const FrameSampleReference& reference)
{
    FrameLinkResult result;
    result.linkedResourceId = reference.frameProjectId + "/"
        + reference.frameId + "/" + reference.sampleId;
    auto iterator = std::find_if(
        project.linkedResources.begin(),
        project.linkedResources.end(),
        [&result](const LinkedResource& resource) {
            return resource.kind == "private-frame-sample"
                && resource.stableId == result.linkedResourceId;
        });
    if (iterator == project.linkedResources.end()) {
        project.linkedResources.push_back({});
        iterator = std::prev(project.linkedResources.end());
    } else {
        result.updated = true;
    }
    iterator->kind = "private-frame-sample";
    iterator->path = reference.sourcePath;
    iterator->stableId = result.linkedResourceId;
    iterator->contentHash = reference.contentHash;
    iterator->summary = reference.summary;
    iterator->extensions["frameProjectId"] = jsonStringValue(reference.frameProjectId);
    iterator->extensions["frameId"] = jsonStringValue(reference.frameId);
    iterator->extensions["sampleId"] = jsonStringValue(reference.sampleId);
    iterator->extensions["encodedBytes"] = jsonStringValue(reference.encodedBytes);

    if (!reference.scenarioId.empty()) {
        const auto scenario = std::find_if(
            project.scenarios.begin(),
            project.scenarios.end(),
            [&reference](const Scenario& candidate) {
                return candidate.id == reference.scenarioId;
            });
        auto* lane = scenario == project.scenarios.end()
            ? nullptr
            : findLane(*scenario, reference.laneId);
        const auto segment = lane
            ? std::find_if(
                lane->segments.begin(),
                lane->segments.end(),
                [&reference](const Segment& candidate) {
                    return candidate.id == reference.segmentId;
                })
            : std::vector<Segment>::iterator{};
        if (scenario == project.scenarios.end()
            || !lane
            || lane->kind != LaneKind::Transaction
            || segment == lane->segments.end()) {
            result.diagnostics.append(
                QStringLiteral("Transaction target is unresolved; resource summary was preserved."));
        } else {
            segment->extensions["linkedResourceStableId"] =
                jsonStringValue(result.linkedResourceId);
        }
    }
    return result;
}

QByteArray makeWorkspaceManifest(
    const Project& project,
    const QString& projectFilePath)
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("producer"), QStringLiteral("Wave Workbench"));
    root.insert(QStringLiteral("projectId"), qString(project.id));
    root.insert(QStringLiteral("projectName"), qString(project.name));
    root.insert(
        QStringLiteral("projectFile"),
        QDir::fromNativeSeparators(QFileInfo(projectFilePath).absoluteFilePath()));
    QJsonArray scenarios;
    for (const auto& scenario : project.scenarios) {
        QJsonObject scenarioObject;
        scenarioObject.insert(QStringLiteral("scenarioId"), qString(scenario.id));
        scenarioObject.insert(QStringLiteral("name"), qString(scenario.name));
        scenarioObject.insert(QStringLiteral("durationTick"), QString::number(scenario.duration));
        QJsonArray signalDefinitions;
        for (const auto& lane : scenario.lanes) {
            signalDefinitions.append(laneDefinitionJson(lane));
        }
        scenarioObject.insert(QStringLiteral("signals"), signalDefinitions);
        scenarios.append(scenarioObject);
    }
    root.insert(QStringLiteral("scenarios"), scenarios);
    QJsonObject commands;
    commands.insert(
        QStringLiteral("generate"),
        QStringLiteral("wave-generate <project.wave.json> <workspace>"));
    commands.insert(
        QStringLiteral("compare"),
        QStringLiteral("wave-compare <project.wave.json> <output-directory>"));
    root.insert(QStringLiteral("cli"), commands);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

PinloomEntry makePinloomEntry(
    const Project& project,
    const Scenario& scenario,
    const QString& projectFilePath,
    const QString& artifactDirectory,
    const QString& manifestPath)
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("producer"), QStringLiteral("Wave Workbench"));
    root.insert(QStringLiteral("projectId"), qString(project.id));
    root.insert(QStringLiteral("scenarioId"), qString(scenario.id));
    root.insert(QStringLiteral("title"), qString(project.name + " / " + scenario.name));
    root.insert(
        QStringLiteral("projectFile"),
        QDir::fromNativeSeparators(QFileInfo(projectFilePath).absoluteFilePath()));
    QJsonArray artifacts;
    const QDir directory(artifactDirectory);
    const auto files = directory.entryInfoList(QDir::Files, QDir::Name);
    for (const auto& file : files) {
        QJsonObject artifact;
        artifact.insert(QStringLiteral("name"), file.fileName());
        artifact.insert(
            QStringLiteral("path"),
            QDir::fromNativeSeparators(file.absoluteFilePath()));
        artifact.insert(QStringLiteral("bytes"), QString::number(file.size()));
        artifacts.append(artifact);
    }
    root.insert(QStringLiteral("artifacts"), artifacts);
    QUrl uri(QStringLiteral("pinloom://archive"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("source"), QStringLiteral("wave-workbench"));
    query.addQueryItem(QStringLiteral("projectId"), qString(project.id));
    query.addQueryItem(QStringLiteral("scenarioId"), qString(scenario.id));
    query.addQueryItem(
        QStringLiteral("manifest"),
        QDir::fromNativeSeparators(QFileInfo(manifestPath).absoluteFilePath()));
    uri.setQuery(query);
    root.insert(QStringLiteral("archiveUri"), uri.toString(QUrl::FullyEncoded));
    return {QJsonDocument(root).toJson(QJsonDocument::Indented), uri};
}

LaunchRequestResult parseWaveWorkbenchUri(const QUrl& uri)
{
    LaunchRequestResult result;
    if (!uri.isValid() || uri.scheme().compare(QStringLiteral("waveworkbench"), Qt::CaseInsensitive) != 0) {
        result.error = QStringLiteral("URI scheme must be waveworkbench");
        return result;
    }
    const auto action = uri.host().toLower();
    if (action != QStringLiteral("open") && action != QStringLiteral("compare")) {
        result.error = QStringLiteral("URI action must be open or compare");
        return result;
    }
    const QUrlQuery query(uri);
    LaunchRequest request;
    request.projectPath = query.queryItemValue(QStringLiteral("project"));
    request.scenarioId = query.queryItemValue(QStringLiteral("scenario"));
    request.laneId = query.queryItemValue(QStringLiteral("lane"));
    request.compareMode = action == QStringLiteral("compare")
        || query.queryItemValue(QStringLiteral("mode")).compare(
               QStringLiteral("compare"),
               Qt::CaseInsensitive)
            == 0;
    if (request.projectPath.isEmpty()) {
        result.error = QStringLiteral("URI project query item is required");
        return result;
    }
    const auto tickText = query.queryItemValue(QStringLiteral("tick"));
    if (!tickText.isEmpty()) {
        bool valid = false;
        const auto tick = tickText.toLongLong(&valid);
        if (!valid) {
            result.error = QStringLiteral("URI tick must be an integer");
            return result;
        }
        request.tick = tick;
    }
    result.request = std::move(request);
    return result;
}

} // namespace wave
