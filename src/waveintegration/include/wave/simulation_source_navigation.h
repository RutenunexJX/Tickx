#pragma once

#include "wave/model.h"
#include "wave/trace.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wave {

enum class SimulationSourceLinkKind {
    Declaration,
    Driver,
};

struct SimulationSourceLink {
    SimulationSourceLinkKind kind{SimulationSourceLinkKind::Declaration};
    std::string sourceFile;
    int sourceLine{0};
    int sourceColumn{0};
    std::string label;
};

struct SimulationSourceBinding {
    std::string laneId;
    std::string traceSignalId;
    std::string semanticId;
    std::string accessPath;
    std::string name;
    std::vector<SimulationSourceLink> links;

    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] const SimulationSourceLink* declaration() const noexcept;
    [[nodiscard]] std::vector<SimulationSourceLink> drivers() const;
};

struct SimulationSourceQuery {
    std::string semanticId;
    std::string sourceFile;
    int sourceLine{0};
    int sourceColumn{0};
    std::string symbolName;
    std::string accessPath;
};

[[nodiscard]] SimulationSourceBinding simulationSourceBindingForTraceSignal(
    const Scenario& scenario,
    const ImportedTrace& trace,
    const TraceIndex& traceIndex,
    std::string_view traceSignalId);

[[nodiscard]] std::optional<SimulationSourceBinding>
resolveSimulationSourceObject(
    const Scenario& scenario,
    const ImportedTrace& trace,
    const TraceIndex& traceIndex,
    const SimulationSourceQuery& query);

} // namespace wave
