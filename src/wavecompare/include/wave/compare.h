#pragma once

#include "wave/model.h"
#include "wave/trace.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace wave {

enum class XHandling {
    Exact,
    IgnoreAnyX,
    ExpectedXWildcard,
};

enum class CompareDifferenceKind {
    ValueMismatch,
    UnmappedSignal,
    MissingSignal,
    WidthMismatch,
    RelationViolation,
    MissingEvent,
    ConditionEvaluationError,
};

struct CompareRule {
    XHandling xHandling{XHandling::Exact};
    Tick edgeTolerance{0};
    std::string busMask;
    std::map<std::string, std::string> enumEquivalence;

    [[nodiscard]] bool operator==(const CompareRule&) const = default;
};

struct CompareOptions {
    CompareRule defaultRule;
    std::map<std::string, CompareRule> laneRules;
    std::optional<Tick> start;
    std::optional<Tick> end;
    std::set<std::string> includedLaneIds;
    bool relationOnly{false};
    bool compareUndefinedExpected{false};
    std::size_t maximumDifferences{100'000};
};

struct CompareDifference {
    std::string id;
    CompareDifferenceKind kind{CompareDifferenceKind::ValueMismatch};
    std::string laneId;
    std::string traceSignalId;
    Tick start{0};
    Tick end{0};
    std::string expected;
    std::string actual;
    std::string message;

    [[nodiscard]] bool operator==(const CompareDifference&) const = default;
};

struct LaneCompareSummary {
    std::string laneId;
    std::string traceSignalId;
    std::uint64_t differenceCount{0};
    Tick mismatchDuration{0};
    std::optional<Tick> firstMismatch;

    [[nodiscard]] bool operator==(const LaneCompareSummary&) const = default;
};

struct CompareResult {
    std::string projectId;
    std::string scenarioId;
    std::string traceId;
    Tick start{0};
    Tick end{0};
    Tick traceOffset{0};
    std::vector<CompareDifference> differences;
    std::vector<LaneCompareSummary> lanes;
    std::vector<std::string> diagnostics;
    std::optional<Tick> firstMismatch;
    std::uint64_t toleratedEdgeCount{0};
    bool truncated{false};

    [[nodiscard]] bool matches() const noexcept { return differences.empty(); }
};

[[nodiscard]] bool traceValueMatchesLane(
    const Lane& lane,
    std::string_view expectedProjectLiteral,
    std::string_view actualBinaryTraceValue,
    const CompareRule& rule = {});
[[nodiscard]] const TraceSignal* mappedTraceSignal(
    const ImportedTrace& reference,
    const TraceIndex& trace,
    std::string_view laneId) noexcept;

[[nodiscard]] CompareResult compareScenario(
    const Project& project,
    const Scenario& scenario,
    const TraceIndex& trace,
    const ImportedTrace& reference,
    const CompareOptions& options = {});

[[nodiscard]] std::string compareResultJson(const CompareResult& result);
[[nodiscard]] std::string compareResultCsv(const CompareResult& result);
[[nodiscard]] std::string compareResultHtml(
    const CompareResult& result,
    const Project& project,
    const Scenario& scenario);

[[nodiscard]] std::string_view toString(XHandling handling) noexcept;
[[nodiscard]] std::string_view toString(CompareDifferenceKind kind) noexcept;

} // namespace wave
