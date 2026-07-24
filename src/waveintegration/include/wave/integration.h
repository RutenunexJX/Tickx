#pragma once

#include "wave/model.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace wave {

struct ExternalSignalDefinition {
    std::string stableId;
    std::string name;
    LaneKind kind{LaneKind::Bit};
    std::uint32_t width{1};
    bool isSigned{false};
    std::string clockDomainId;

    [[nodiscard]] bool operator==(const ExternalSignalDefinition&) const = default;
};

struct ZeroSlackSignalList {
    static constexpr int CurrentSchemaVersion = 1;

    int schemaVersion{CurrentSchemaVersion};
    std::string sourceProjectId;
    std::vector<ExternalSignalDefinition> signalDefinitions;
};

struct SignalListParseResult {
    std::optional<ZeroSlackSignalList> signalList;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return signalList.has_value(); }
};

struct SignalImportResult {
    std::size_t added{0};
    std::size_t existing{0};
    QStringList diagnostics;
};

struct FrameSampleReference {
    static constexpr int CurrentSchemaVersion = 1;

    int schemaVersion{CurrentSchemaVersion};
    std::string frameProjectId;
    std::string frameId;
    std::string sampleId;
    std::string encodedBytes;
    std::string contentHash;
    std::string summary;
    std::string sourcePath;
    std::string scenarioId;
    std::string laneId;
    std::string segmentId;
};

struct FrameReferenceParseResult {
    std::optional<FrameSampleReference> reference;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return reference.has_value(); }
};

struct FrameLinkResult {
    std::string linkedResourceId;
    bool updated{false};
    QStringList diagnostics;
};

struct LaunchRequest {
    QString projectPath;
    QString scenarioId;
    QString laneId;
    std::optional<Tick> tick;
    bool compareMode{false};
};

struct LaunchRequestResult {
    std::optional<LaunchRequest> request;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return request.has_value(); }
};

struct PinloomEntry {
    QByteArray document;
    QUrl archiveUri;
};

[[nodiscard]] SignalListParseResult parseZeroSlackSignalList(const QByteArray& document);
[[nodiscard]] SignalImportResult applyZeroSlackSignalList(
    Project& project,
    Scenario& scenario,
    const ZeroSlackSignalList& signalList);

[[nodiscard]] FrameReferenceParseResult parseFrameSampleReference(const QByteArray& document);
[[nodiscard]] FrameLinkResult linkFrameSample(
    Project& project,
    const FrameSampleReference& reference);

[[nodiscard]] QByteArray makeWorkspaceManifest(
    const Project& project,
    const QString& projectFilePath);
[[nodiscard]] PinloomEntry makePinloomEntry(
    const Project& project,
    const Scenario& scenario,
    const QString& projectFilePath,
    const QString& artifactDirectory,
    const QString& manifestPath);

[[nodiscard]] LaunchRequestResult parseWaveWorkbenchUri(const QUrl& uri);

} // namespace wave
