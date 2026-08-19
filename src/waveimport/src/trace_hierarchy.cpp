#include "wave/trace_hierarchy.h"

#include <string_view>

namespace wave {
namespace {

std::vector<std::string> fallbackScopePath(const std::string_view scope)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start < scope.size()) {
        const auto separator = scope.find('.', start);
        const auto end = separator == std::string_view::npos
            ? scope.size()
            : separator;
        if (end > start) result.emplace_back(scope.substr(start, end - start));
        if (separator == std::string_view::npos) break;
        start = separator + 1;
    }
    return result;
}

TraceHierarchyScope& findOrAppendScope(
    std::vector<TraceHierarchyScope>& scopes,
    const std::string& name,
    const std::string& fullName)
{
    for (auto& scope : scopes) {
        if (scope.name == name) return scope;
    }
    scopes.push_back({name, fullName, {}, {}});
    return scopes.back();
}

} // namespace

TraceHierarchy buildTraceHierarchy(const TraceIndex& trace)
{
    TraceHierarchy result;
    result.signalCount = trace.traceSignals.size();
    for (std::size_t index = 0; index < trace.traceSignals.size(); ++index) {
        const auto& signal = trace.traceSignals[index];
        TraceHierarchySignal leaf{
            index,
            signal.id,
            signal.reference.empty() ? signal.fullName : signal.reference,
            signal.fullName,
            signal.width,
        };

        const auto path = signal.scopePath.empty()
            ? fallbackScopePath(signal.scope)
            : signal.scopePath;
        if (path.empty()) {
            result.unscopedSignals.push_back(std::move(leaf));
            continue;
        }

        auto* current = &result.scopes;
        std::string fullName;
        TraceHierarchyScope* scope = nullptr;
        for (const auto& component : path) {
            if (!fullName.empty()) fullName += '.';
            fullName += component;
            scope = &findOrAppendScope(*current, component, fullName);
            current = &scope->scopes;
        }
        scope->leaves.push_back(std::move(leaf));
    }
    return result;
}

} // namespace wave
