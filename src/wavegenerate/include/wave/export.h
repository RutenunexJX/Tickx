#pragma once

#include "wave/generation.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace wave {

struct ExportOptions {
    std::optional<Tick> start;
    std::optional<Tick> end;
    int width{1600};
    int laneHeight{52};
    int pngDpi{192};
    Tick pdfPageSpanTicks{0};
    bool includeRelations{true};
    bool includeMarkers{true};
    bool includeAnnotations{true};
};

struct ArtifactBundle {
    QByteArray systemVerilog;
    QByteArray assertions;
    QByteArray cocotb;
    QByteArray svg;
    QByteArray png;
    QByteArray pdf;
    QByteArray waveDromJson;
};

struct ArtifactBundleResult {
    std::optional<ArtifactBundle> artifacts;
    std::vector<GenerationDiagnostic> diagnostics;
    QString error;

    [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] QByteArray renderWaveformSvg(
    const Project& project,
    const Scenario& scenario,
    const ExportOptions& options,
    QString* error = nullptr);

[[nodiscard]] QByteArray renderWaveformPng(
    const Project& project,
    const Scenario& scenario,
    const ExportOptions& options,
    QString* error = nullptr);

[[nodiscard]] QByteArray renderWaveformPdf(
    const Project& project,
    const Scenario& scenario,
    const ExportOptions& options,
    QString* error = nullptr);

[[nodiscard]] QByteArray generateWaveDromJson(
    const Project& project,
    const Scenario& scenario,
    const ExportOptions& options,
    QString* error = nullptr);

[[nodiscard]] ArtifactBundleResult generateArtifactBundle(
    const Project& project,
    const Scenario& scenario,
    const ExportOptions& options = {});

[[nodiscard]] bool writeArtifactBundleAtomic(
    const ArtifactBundle& artifacts,
    const QString& directory,
    const QString& baseName,
    QString* error = nullptr);

} // namespace wave
