#include "wave/simulation_source_navigation.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <charconv>
#include <limits>

namespace wave {
namespace {

std::string extensionString(
    const JsonExtensions& extensions,
    const std::string_view key)
{
    const auto iterator = extensions.find(std::string(key));
    if (iterator == extensions.end()) return {};
    const auto bytes = QByteArray::fromStdString(iterator->second);
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(
        QByteArrayLiteral("[") + bytes + QByteArrayLiteral("]"), &error);
    if (error.error == QJsonParseError::NoError && document.isArray()
        && document.array().size() == 1 && document.array().first().isString()) {
        return document.array().first().toString().toUtf8().toStdString();
    }
    return iterator->second;
}

int extensionInt(
    const JsonExtensions& extensions,
    const std::string_view key,
    const int fallback)
{
    const auto value = extensionString(extensions, key);
    int parsed = 0;
    const auto result = std::from_chars(
        value.data(), value.data() + value.size(), parsed);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size()
        ? parsed
        : fallback;
}

std::string normalizedPath(const std::string& path)
{
    auto normalized = QDir::cleanPath(QString::fromUtf8(path));
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (normalized.startsWith(QStringLiteral("./"))) normalized.remove(0, 2);
    return normalized.toUtf8().toStdString();
}

std::vector<SimulationSourceLink> sourceLinks(const Lane& lane)
{
    std::vector<SimulationSourceLink> links;
    const auto iterator = lane.extensions.find("waveSimulation.sourceLinks");
    if (iterator != lane.extensions.end()) {
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(
            QByteArray::fromStdString(iterator->second), &error);
        if (error.error == QJsonParseError::NoError && document.isArray()) {
            for (const auto& value : document.array()) {
                if (!value.isObject()) continue;
                const auto object = value.toObject();
                const auto kind = object.value(QStringLiteral("kind")).toString();
                const auto file = object.value(QStringLiteral("sourceFile")).toString();
                const int line = object.value(QStringLiteral("sourceLine")).toInt();
                const int column = object.value(QStringLiteral("sourceColumn")).toInt();
                if ((kind != QStringLiteral("declaration")
                     && kind != QStringLiteral("driver"))
                    || file.isEmpty() || line <= 0 || column <= 0) {
                    continue;
                }
                links.push_back({
                    kind == QStringLiteral("driver")
                        ? SimulationSourceLinkKind::Driver
                        : SimulationSourceLinkKind::Declaration,
                    normalizedPath(file.toUtf8().toStdString()),
                    line,
                    column,
                    object.value(QStringLiteral("label"))
                        .toString()
                        .toUtf8()
                        .toStdString(),
                });
            }
        }
    }
    if (std::none_of(links.cbegin(), links.cend(), [](const auto& link) {
            return link.kind == SimulationSourceLinkKind::Declaration;
        })) {
        const auto file = extensionString(
            lane.extensions, "waveSimulation.sourceFile");
        const int line = extensionInt(
            lane.extensions, "waveSimulation.sourceLine", 0);
        const int column = extensionInt(
            lane.extensions, "waveSimulation.sourceColumn", 1);
        if (!file.empty() && line > 0) {
            links.insert(links.begin(), {
                SimulationSourceLinkKind::Declaration,
                normalizedPath(file), line, std::max(1, column), "Declaration"});
        }
    }
    return links;
}

const Lane* laneForId(const Scenario& scenario, const std::string_view id)
{
    const auto iterator = std::find_if(
        scenario.lanes.cbegin(), scenario.lanes.cend(), [&](const Lane& lane) {
            return lane.id == id;
        });
    return iterator == scenario.lanes.cend() ? nullptr : &*iterator;
}

SimulationSourceBinding bindingForLane(
    const Lane& lane,
    const std::string& traceSignalId)
{
    SimulationSourceBinding binding;
    binding.laneId = lane.id;
    binding.traceSignalId = traceSignalId;
    binding.semanticId = extensionString(
        lane.extensions, "waveSimulation.semanticId");
    binding.accessPath = extensionString(
        lane.extensions, "waveSimulation.accessPath");
    binding.name = extensionString(
        lane.extensions, "waveSimulation.rootPortName");
    if (binding.name.empty()) binding.name = lane.name;
    binding.links = sourceLinks(lane);
    return binding;
}

int matchScore(
    const SimulationSourceBinding& binding,
    const SimulationSourceQuery& query)
{
    int score = 0;
    if (!query.semanticId.empty() && binding.semanticId == query.semanticId) {
        score = 1000;
    } else if (!query.sourceFile.empty() && query.sourceLine > 0) {
        const auto* declaration = binding.declaration();
        if (!declaration
            || normalizedPath(declaration->sourceFile)
                != normalizedPath(query.sourceFile)
            || declaration->sourceLine != query.sourceLine) {
            return -1;
        }
        score = 500;
        if (query.sourceColumn > 0
            && declaration->sourceColumn == query.sourceColumn) {
            score += 20;
        }
    } else {
        return -1;
    }
    if (!query.accessPath.empty() && binding.accessPath == query.accessPath)
        score += 100;
    if (!query.symbolName.empty()
        && (binding.name == query.symbolName
            || binding.accessPath == query.symbolName)) {
        score += 50;
    }
    return score;
}

} // namespace

bool SimulationSourceBinding::available() const noexcept
{
    return !laneId.empty() && !traceSignalId.empty() && declaration() != nullptr;
}

const SimulationSourceLink* SimulationSourceBinding::declaration() const noexcept
{
    const auto iterator = std::find_if(
        links.cbegin(), links.cend(), [](const SimulationSourceLink& link) {
            return link.kind == SimulationSourceLinkKind::Declaration;
        });
    return iterator == links.cend() ? nullptr : &*iterator;
}

std::vector<SimulationSourceLink> SimulationSourceBinding::drivers() const
{
    std::vector<SimulationSourceLink> result;
    std::copy_if(
        links.cbegin(), links.cend(), std::back_inserter(result),
        [](const SimulationSourceLink& link) {
            return link.kind == SimulationSourceLinkKind::Driver;
        });
    return result;
}

SimulationSourceBinding simulationSourceBindingForTraceSignal(
    const Scenario& scenario,
    const ImportedTrace& trace,
    const TraceIndex& traceIndex,
    const std::string_view traceSignalId)
{
    if (!traceIndex.findSignal(traceSignalId)) return {};
    for (const auto& [laneId, mappedSignalId] : trace.signalMapping) {
        if (mappedSignalId != traceSignalId) continue;
        const auto* lane = laneForId(scenario, laneId);
        if (!lane) continue;
        return bindingForLane(*lane, mappedSignalId);
    }
    return {};
}

std::optional<SimulationSourceBinding> resolveSimulationSourceObject(
    const Scenario& scenario,
    const ImportedTrace& trace,
    const TraceIndex& traceIndex,
    const SimulationSourceQuery& query)
{
    int bestScore = -1;
    bool ambiguous = false;
    std::optional<SimulationSourceBinding> best;
    for (const auto& [laneId, traceSignalId] : trace.signalMapping) {
        if (!traceIndex.findSignal(traceSignalId)) continue;
        const auto* lane = laneForId(scenario, laneId);
        if (!lane) continue;
        auto binding = bindingForLane(*lane, traceSignalId);
        if (!binding.available()) continue;
        const int score = matchScore(binding, query);
        if (score < 0) continue;
        if (score > bestScore) {
            bestScore = score;
            ambiguous = false;
            best = std::move(binding);
        } else if (score == bestScore
                   && best && best->traceSignalId != binding.traceSignalId) {
            ambiguous = true;
        }
    }
    return ambiguous ? std::nullopt : best;
}

} // namespace wave
