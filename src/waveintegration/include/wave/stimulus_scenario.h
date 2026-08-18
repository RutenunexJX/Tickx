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
    std::optional<StimulusClockConfiguration> clock;
    std::optional<StimulusResetConfiguration> reset;
};

struct ZeroSlackStimulusScenario {
    static constexpr int CurrentSchemaVersion = 2;
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
    StimulusScenarioViewState view;
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

[[nodiscard]] std::string_view toString(StimulusPortRole role) noexcept;
[[nodiscard]] std::string_view toString(
    StimulusResetActiveLevel activeLevel) noexcept;
[[nodiscard]] std::string_view toString(
    StimulusResetSynchronization synchronization) noexcept;

} // namespace wave
