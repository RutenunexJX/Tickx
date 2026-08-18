#pragma once

#include "wave/stimulus_scenario.h"

#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace wave {

inline constexpr auto DefaultSimulationScenarioFileName = "default.json";

struct StoredSimulationScenario {
    ZeroSlackStimulusScenario scenario;
    QString filePath;
    bool isDefault{false};
};

struct SimulationScenarioStoreLoadResult {
    std::vector<StoredSimulationScenario> scenarios;
    QStringList diagnostics;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return error.isEmpty(); }
};

struct SimulationScenarioStoreWriteResult {
    QString filePath;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return error.isEmpty(); }
};

[[nodiscard]] QString simulationScenarioFileName(
    const std::string& scenarioId,
    bool isDefault);
[[nodiscard]] QString defaultSimulationScenarioPath(const QString& directory);
[[nodiscard]] SimulationScenarioStoreLoadResult loadSimulationScenarioStore(
    const QString& directory);
[[nodiscard]] SimulationScenarioStoreWriteResult saveSimulationScenario(
    const QString& directory,
    const ZeroSlackStimulusScenario& scenario,
    bool isDefault);
[[nodiscard]] bool removeSimulationScenario(
    const QString& directory,
    const std::string& scenarioId,
    QString* error = nullptr);

} // namespace wave
