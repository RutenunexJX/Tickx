#pragma once

#include "wave/model.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <istream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wave {

enum class TraceFormat {
    Vcd,
    Csv,
};

enum class CsvTimeUnit {
    ProjectTick,
    Picosecond,
    Nanosecond,
    Microsecond,
    Millisecond,
};

enum class TraceDiagnosticSeverity {
    Information,
    Warning,
    Error,
};

struct TraceIdentity {
    std::string projectId;
    std::string traceId;
    std::uint64_t generation{0};

    [[nodiscard]] bool operator==(const TraceIdentity&) const = default;
};

struct TraceDiagnostic {
    TraceDiagnosticSeverity severity{TraceDiagnosticSeverity::Error};
    std::size_t line{0};
    std::string message;

    [[nodiscard]] bool operator==(const TraceDiagnostic&) const = default;
};

struct TraceTransition {
    Tick tick{0};
    std::string value;

    [[nodiscard]] bool operator==(const TraceTransition&) const = default;
};

struct TraceSignal {
    std::string id;
    std::string identifierCode;
    std::string scope;
    std::string reference;
    std::string fullName;
    std::uint32_t width{1};
    std::vector<TraceTransition> transitions;

    [[nodiscard]] std::pair<std::size_t, std::size_t> visibleRange(
        Tick start,
        Tick end,
        bool includePreceding = true) const noexcept;
    [[nodiscard]] std::span<const TraceTransition> visibleTransitions(
        Tick start,
        Tick end,
        bool includePreceding = true) const noexcept;
    [[nodiscard]] const TraceTransition* valueAt(Tick tick) const noexcept;

    [[nodiscard]] bool operator==(const TraceSignal&) const = default;
};

struct TraceIndex {
    TraceIdentity identity;
    TraceFormat format{TraceFormat::Vcd};
    TimeBase projectTimeBase;
    Tick startTick{0};
    Tick endTick{0};
    std::vector<TraceSignal> traceSignals;
    std::uint64_t transitionCount{0};

    [[nodiscard]] const TraceSignal* findSignal(std::string_view signalId) const noexcept;
    [[nodiscard]] TraceSignal* findSignal(std::string_view signalId) noexcept;
    [[nodiscard]] bool shift(Tick delta) noexcept;
};

struct TraceParseOptions {
    TimeBase projectTimeBase;
    TraceIdentity identity;
    Tick offset{0};
    CsvTimeUnit csvTimeUnit{CsvTimeUnit::ProjectTick};
    std::function<bool()> isCancelled;
    std::function<void(std::uint64_t, std::uint64_t)> progress;
};

struct TraceParseResult {
    std::optional<TraceIndex> index;
    std::vector<TraceDiagnostic> diagnostics;
    bool cancelled{false};

    [[nodiscard]] bool ok() const noexcept;
    [[nodiscard]] std::string errorSummary() const;
};

[[nodiscard]] TraceParseResult parseVcd(
    std::istream& input,
    const TraceParseOptions& options);
[[nodiscard]] TraceParseResult parseCsv(
    std::istream& input,
    const TraceParseOptions& options);
[[nodiscard]] TraceParseResult parseVcdFile(
    const std::filesystem::path& path,
    const TraceParseOptions& options);
[[nodiscard]] TraceParseResult parseCsvFile(
    const std::filesystem::path& path,
    const TraceParseOptions& options);

[[nodiscard]] std::map<std::string, std::string> suggestSignalMapping(
    const Scenario& scenario,
    const TraceIndex& trace);

[[nodiscard]] std::string_view toString(TraceFormat format) noexcept;

} // namespace wave
