#pragma once

#include "wave/trace.h"

#include <QString>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace wave {

struct FstTraceReaderLimits {
    std::size_t maxMetadataSignals{1'000'000};
    std::size_t maxSignalsPerRequest{64};
    qint64 maxResponseBytes{256 * 1024 * 1024};
    int timeoutMs{120'000};
};

struct TraceSignalLoadResult {
    TraceIdentity identity;
    std::string sourceFingerprint;
    std::vector<TraceSignal> loadedSignals;
    std::vector<TraceDiagnostic> diagnostics;
    bool cancelled{false};

    [[nodiscard]] bool ok() const noexcept;
    [[nodiscard]] std::string errorSummary() const;
};

[[nodiscard]] TraceParseResult readFstMetadataFile(
    const QString& path,
    const TraceParseOptions& options,
    const QString& readerExecutable,
    const FstTraceReaderLimits& limits = {});

[[nodiscard]] TraceSignalLoadResult loadFstSignalsFile(
    const QString& path,
    const TraceParseOptions& options,
    const QString& readerExecutable,
    std::span<const std::string> signalIds,
    const FstTraceReaderLimits& limits = {});

[[nodiscard]] bool mergeLoadedTraceSignals(
    TraceIndex& index,
    const TraceSignalLoadResult& loaded,
    std::string* error = nullptr);

} // namespace wave
