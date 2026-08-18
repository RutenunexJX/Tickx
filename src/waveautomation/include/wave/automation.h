#pragma once

#include "wave/model.h"

#include <QJsonObject>
#include <QString>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace wave {

inline constexpr auto AutomationReportSchema = "wave-workbench.cli/v1";
inline constexpr auto AutomationBatchSchema = "wave-workbench.operations/v1";
inline constexpr auto AutomationCapabilitiesSchema =
    "wave-workbench.capabilities/v1";
inline constexpr auto AutomationJsonSchemaDialect =
    "https://json-schema.org/draft/2020-12/schema";
inline constexpr auto AutomationCapabilitiesJsonSchemaRef =
    "schemas/automation/v1/capabilities.schema.json";
inline constexpr auto AutomationReportJsonSchemaRef =
    "schemas/automation/v1/report.schema.json";
inline constexpr auto AutomationBatchJsonSchemaRef =
    "schemas/automation/v1/operation-batch.schema.json";

struct AutomationDocument {
    QJsonObject json;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return error.isEmpty(); }
};

struct AutomationApplyResult {
    QJsonObject json;
    std::optional<Project> project;
    QString error;
    int failedOperation{-1};
    bool changed{false};

    [[nodiscard]] bool ok() const noexcept
    {
        return error.isEmpty() && project.has_value();
    }
};

struct AutomationNewProjectOptions {
    std::string projectId;
    std::string projectName{"Untitled"};
    std::string scenarioId;
    std::string scenarioName{"Waveform"};
    TimeBase timeBase{1};
    Tick duration{200'000};
};

struct AutomationProjectResult {
    QJsonObject json;
    std::optional<Project> project;
    QString error;

    [[nodiscard]] bool ok() const noexcept
    {
        return error.isEmpty() && project.has_value();
    }
};

struct AutomationSignalQueryOptions {
    std::string match;
    std::optional<LaneKind> kind;
    std::size_t limit{50};
    bool exact{false};
};

enum class AutomationEdgeKind {
    Initial,
    Rising,
    Falling,
    Change,
};

struct AutomationEdgeQueryOptions {
    std::optional<Tick> start;
    std::optional<Tick> end;
    std::optional<AutomationEdgeKind> edge;
    std::size_t limit{200};
};

struct AutomationRelationQueryOptions {
    std::string match;
    std::optional<Severity> severity;
    std::optional<Tick> start;
    std::optional<Tick> end;
    std::size_t limit{100};
    bool exact{false};
};

struct AutomationMarkerQueryOptions {
    std::string match;
    std::optional<MarkerKind> kind;
    std::optional<Tick> start;
    std::optional<Tick> end;
    std::size_t limit{100};
    bool exact{false};
};

enum class AutomationInspectDetail {
    Full,
    Summary,
};

struct AutomationTimeResult {
    std::optional<Tick> tick;
    QString error;

    [[nodiscard]] bool ok() const noexcept
    {
        return error.isEmpty() && tick.has_value();
    }
};

[[nodiscard]] AutomationDocument describeAutomationCapabilities();

[[nodiscard]] AutomationDocument inspectProjectForAutomation(
    const Project& project,
    const std::optional<std::string>& scenarioId = std::nullopt,
    AutomationInspectDetail detail = AutomationInspectDetail::Full);

[[nodiscard]] AutomationDocument validateProjectForAutomation(
    const Project& project,
    const std::optional<std::string>& scenarioId = std::nullopt);

[[nodiscard]] AutomationTimeResult parseAutomationTime(
    const Project& project,
    const QString& text,
    const std::optional<std::string>& clockId = std::nullopt);

[[nodiscard]] AutomationDocument sampleProjectForAutomation(
    const Project& project,
    Tick tick,
    const std::optional<std::string>& scenarioId = std::nullopt,
    const std::vector<std::string>& laneIds = {});

[[nodiscard]] AutomationDocument findSignalsForAutomation(
    const Project& project,
    const AutomationSignalQueryOptions& options = {},
    const std::optional<std::string>& scenarioId = std::nullopt);

[[nodiscard]] AutomationDocument inspectProjectWindowForAutomation(
    const Project& project,
    Tick start,
    Tick end,
    const std::optional<std::string>& scenarioId = std::nullopt,
    const std::vector<std::string>& laneIds = {});

[[nodiscard]] AutomationDocument findWaveformEdgesForAutomation(
    const Project& project,
    const AutomationEdgeQueryOptions& options = {},
    const std::optional<std::string>& scenarioId = std::nullopt,
    const std::vector<std::string>& laneIds = {});

[[nodiscard]] AutomationDocument findRelationsForAutomation(
    const Project& project,
    const AutomationRelationQueryOptions& options = {},
    const std::optional<std::string>& scenarioId = std::nullopt,
    const std::vector<std::string>& laneIds = {});

[[nodiscard]] AutomationDocument findMarkersForAutomation(
    const Project& project,
    const AutomationMarkerQueryOptions& options = {},
    const std::optional<std::string>& scenarioId = std::nullopt);

[[nodiscard]] AutomationProjectResult createProjectForAutomation(
    const AutomationNewProjectOptions& options);

[[nodiscard]] AutomationApplyResult applyAutomationBatch(
    const Project& source,
    const QJsonObject& batch,
    const std::optional<std::string>& scenarioOverride = std::nullopt);

} // namespace wave
