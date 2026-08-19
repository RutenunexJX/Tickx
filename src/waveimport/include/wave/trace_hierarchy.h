#pragma once

#include "wave/trace.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wave {

struct TraceHierarchySignal {
    std::size_t traceSignalIndex{0};
    std::string id;
    std::string name;
    std::string fullName;
    std::uint32_t width{1};

    [[nodiscard]] bool operator==(const TraceHierarchySignal&) const = default;
};

struct TraceHierarchyScope {
    std::string name;
    std::string fullName;
    std::vector<TraceHierarchyScope> scopes;
    std::vector<TraceHierarchySignal> leaves;

    [[nodiscard]] bool operator==(const TraceHierarchyScope&) const = default;
};

struct TraceHierarchy {
    std::vector<TraceHierarchyScope> scopes;
    std::vector<TraceHierarchySignal> unscopedSignals;
    std::size_t signalCount{0};

    [[nodiscard]] bool operator==(const TraceHierarchy&) const = default;
};

[[nodiscard]] TraceHierarchy buildTraceHierarchy(const TraceIndex& trace);

} // namespace wave
