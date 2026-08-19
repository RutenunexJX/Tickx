#pragma once

#include "wave/module_manifest.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wave {

enum class StimulusPortRole {
    Stimulus,
    Watch,
    StimulusWatch,
};

enum class StimulusResetActiveLevel {
    Unspecified,
    Low,
    High,
};

enum class StimulusResetSynchronization {
    Unspecified,
    Synchronous,
    Asynchronous,
};

struct StimulusScenarioTarget {
    ModuleManifestTargetMode mode{ModuleManifestTargetMode::ModuleDefinition};
    std::string module;
    std::string instancePath;
};

struct StimulusPortBinding {
    std::string name;
    std::string rootPortName;
    std::string relativePath;
    std::vector<ModuleManifestStructuredSelector> selectors;
    bool structured{false};
    bool packedBitOffsetValid{false};
    std::uint64_t packedBitOffset{0};
    std::string interfaceName;
    std::string modportName;
    ModulePortDirection direction{ModulePortDirection::Unknown};
    std::string canonicalTypeId;
    std::string declarationShapeId;
    std::uint32_t width{1};
    bool isSigned{false};
    std::size_t sourceOrder{0};
};

struct StimulusClockConfiguration {
    Tick period{1};
    Tick phase{0};
    Rational dutyCycle{1, 2};
    ClockEdge activeEdge{ClockEdge::Rising};
    char initialValue{'0'};
};

struct StimulusResetConfiguration {
    StimulusResetActiveLevel activeLevel{StimulusResetActiveLevel::Unspecified};
    StimulusResetSynchronization synchronization{
        StimulusResetSynchronization::Unspecified};
};

struct StimulusRange {
    Tick start{0};
    Tick end{0};
    std::string value;
};

struct StimulusScenarioGroup {
    std::string id;
    std::string name;
    std::size_t displayOrder{0};
    bool visible{true};
};

struct StimulusScenarioMarker {
    std::string id;
    std::string name;
    Tick start{0};
    Tick end{0};
    MarkerKind kind{MarkerKind::Point};
    std::string note;
};

struct StimulusScenarioViewState {
    std::string selectedPortName;
    Tick cursorTick{0};
    Tick visibleSpanTicks{0};
};

struct StimulusScenarioPort {
    StimulusPortBinding binding;
    std::string laneId;
    std::size_t displayOrder{0};
    StimulusPortRole role{StimulusPortRole::Stimulus};
    LaneKind kind{LaneKind::Bit};
    Radix radix{Radix::Hexadecimal};
    bool visible{true};
    std::string groupId;
    std::map<std::string, std::string> enumMap;
    std::vector<StimulusRange> segments;
    std::vector<StimulusRange> expectedSegments;
    std::optional<StimulusClockConfiguration> clock;
    std::optional<StimulusResetConfiguration> reset;
};

enum class SimulationCheckKind {
    ValueAtTick,
    StableRange,
    EdgeResponse,
};

enum class SimulationCheckEdge {
    Rising,
    Falling,
    AnyChange,
};

struct SimulationCheckDefinition {
    std::string id;
    std::string name;
    bool enabled{true};
    SimulationCheckKind kind{SimulationCheckKind::ValueAtTick};
    std::string laneId;
    Tick tick{0};
    Tick start{0};
    Tick end{0};
    std::string expectedValue;
    std::string sourceLaneId;
    SimulationCheckEdge sourceEdge{SimulationCheckEdge::Rising};
    std::string targetLaneId;
    SimulationCheckEdge targetEdge{SimulationCheckEdge::Rising};
    Tick minimumDelay{0};
    Tick maximumDelay{0};

    [[nodiscard]] bool operator==(const SimulationCheckDefinition&) const = default;
};

struct ZeroSlackStimulusScenario {
    static constexpr int CurrentSchemaVersion = 5;
    static constexpr int MinimumSupportedSchemaVersion = 1;

    int schemaVersion{CurrentSchemaVersion};
    int manifestSchemaVersion{ZeroSlackModuleManifest::CurrentSchemaVersion};
    std::string identity;
    std::string manifestIdentity;
    std::string workspaceId;
    StimulusScenarioTarget target;
    std::string scenarioId;
    std::string name;
    TimeBase timeBase;
    Tick duration{0};
    std::vector<StimulusScenarioGroup> groups;
    std::vector<StimulusScenarioPort> ports;
    std::vector<StimulusScenarioMarker> markers;
    std::vector<SimulationCheckDefinition> checks;
    StimulusScenarioViewState view;
};

struct SimulationCheckLoadResult {
    std::vector<SimulationCheckDefinition> checks;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return error.isEmpty(); }
};

struct StimulusScenarioParseResult {
    std::optional<ZeroSlackStimulusScenario> scenario;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return scenario.has_value(); }
};

struct StimulusScenarioExportResult {
    std::optional<ZeroSlackStimulusScenario> scenario;
    QStringList diagnostics;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return scenario.has_value(); }
};

struct StimulusScenarioRestoreResult {
    std::optional<Project> project;
    bool manifestChanged{false};
    std::size_t restoredPortCount{0};
    std::size_t missingSavedPortCount{0};
    std::size_t incompatiblePortCount{0};
    std::size_t newPortCount{0};
    std::size_t renamedPortCount{0};
    std::size_t widthChangedPortCount{0};
    StimulusScenarioViewState view;
    QStringList diagnostics;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return project.has_value(); }
};

[[nodiscard]] QByteArray serializeZeroSlackStimulusScenario(
    const ZeroSlackStimulusScenario& scenario);
[[nodiscard]] StimulusScenarioParseResult parseZeroSlackStimulusScenario(
    const QByteArray& document);
[[nodiscard]] StimulusScenarioExportResult exportZeroSlackStimulusScenario(
    const Project& project,
    const Scenario& scenario,
    const StimulusScenarioViewState& view = {});
[[nodiscard]] StimulusScenarioRestoreResult restoreZeroSlackStimulusScenario(
    const ZeroSlackModuleManifest& manifest,
    const ZeroSlackStimulusScenario& scenario);
[[nodiscard]] SimulationCheckLoadResult loadSimulationChecks(
    const Scenario& scenario);
[[nodiscard]] bool storeSimulationChecks(
    Scenario& scenario,
    const std::vector<SimulationCheckDefinition>& checks,
    QString* error = nullptr);

[[nodiscard]] std::string_view toString(StimulusPortRole role) noexcept;
[[nodiscard]] std::string_view toString(
    StimulusResetActiveLevel activeLevel) noexcept;
[[nodiscard]] std::string_view toString(
    StimulusResetSynchronization synchronization) noexcept;
[[nodiscard]] std::string_view toString(SimulationCheckKind kind) noexcept;
[[nodiscard]] std::string_view toString(SimulationCheckEdge edge) noexcept;

} // namespace wave
