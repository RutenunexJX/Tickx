#pragma once

#include "wave/simulation_runner.h"

#include <QByteArray>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace wave {

inline constexpr auto SimulationBuildCacheSchema =
    "wave-workbench.simulation-build-cache/v1";
inline constexpr auto SimulationRuntimeHarnessVersion =
    "wave-runtime-harness/v1";

struct SimulationBuildSourceInput {
    QString path;
    QString role;
    QByteArray content;
};

struct SimulationBuildFingerprintInput {
    QByteArray manifestDocument;
    std::vector<SimulationBuildSourceInput> sources;
    QByteArray harnessDocument;
    QString verilatorProgram;
    QString verilatorVersion;
    QString cxxProgram;
    QString cxxVersion;
    CxxCompilerFamily cxxFamily{CxxCompilerFamily::Unknown};
    QStringList verilatorArguments;
    QStringList cxxArguments;
    QProcessEnvironment environment;
};

struct SimulationBuildFingerprint {
    QString value;
    QJsonObject evidence;

    [[nodiscard]] bool valid() const noexcept
    {
        return value.startsWith(QStringLiteral("sha256:"))
            && value.size() == 71 && !evidence.isEmpty();
    }
};

struct SimulationBuildCacheRecord {
    SimulationBuildFingerprint build;
    QString executableSha256;
};

struct SimulationBuildCacheRecordParseResult {
    std::optional<SimulationBuildCacheRecord> record;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return record.has_value(); }
};

[[nodiscard]] QString simulationSha256(const QByteArray& document);
[[nodiscard]] SimulationBuildFingerprint computeSimulationBuildFingerprint(
    const SimulationBuildFingerprintInput& input);
[[nodiscard]] QByteArray serializeSimulationBuildCacheRecord(
    const SimulationBuildCacheRecord& record);
[[nodiscard]] SimulationBuildCacheRecordParseResult
parseSimulationBuildCacheRecord(const QByteArray& document);

} // namespace wave
