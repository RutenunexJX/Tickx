#include "wave/simulation_scenario_store.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <algorithm>
#include <set>

namespace wave {
namespace {

QString normalizedDirectory(const QString& directory)
{
    return QDir(directory).absolutePath();
}

bool writeAtomic(const QString& path, const QByteArray& data, QString& error)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        error = QStringLiteral("Cannot create simulation scenario directory: %1")
                    .arg(info.absolutePath());
        return false;
    }
    QSaveFile file(info.absoluteFilePath());
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(data) != data.size()
        || !file.commit()) {
        error = QStringLiteral("Cannot save simulation scenario %1: %2")
                    .arg(info.absoluteFilePath(), file.errorString());
        return false;
    }
    return true;
}

} // namespace

QString simulationScenarioFileName(
    const std::string& scenarioId,
    const bool isDefault)
{
    if (isDefault) return QString::fromLatin1(DefaultSimulationScenarioFileName);
    const auto digest = QCryptographicHash::hash(
        QByteArray::fromStdString(scenarioId),
        QCryptographicHash::Sha256).toHex().left(24);
    return QStringLiteral("scenario-%1.json").arg(QString::fromLatin1(digest));
}

QString defaultSimulationScenarioPath(const QString& directory)
{
    return QDir(normalizedDirectory(directory)).filePath(
        QString::fromLatin1(DefaultSimulationScenarioFileName));
}

SimulationScenarioStoreLoadResult loadSimulationScenarioStore(
    const QString& directory)
{
    SimulationScenarioStoreLoadResult result;
    if (directory.trimmed().isEmpty()) {
        result.error = QStringLiteral("Simulation scenario directory is empty.");
        return result;
    }
    const QDir root(normalizedDirectory(directory));
    if (!root.exists()) return result;

    auto files = root.entryInfoList(
        {QStringLiteral("*.json")},
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);
    std::stable_sort(files.begin(), files.end(), [](const QFileInfo& left,
                                                     const QFileInfo& right) {
        const auto leftDefault = left.fileName().compare(
            QString::fromLatin1(DefaultSimulationScenarioFileName),
            Qt::CaseInsensitive) == 0;
        const auto rightDefault = right.fileName().compare(
            QString::fromLatin1(DefaultSimulationScenarioFileName),
            Qt::CaseInsensitive) == 0;
        return leftDefault != rightDefault ? leftDefault : left.fileName() < right.fileName();
    });

    std::set<std::string> ids;
    std::set<QString> names;
    for (const auto& info : files) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            result.diagnostics.append(
                QStringLiteral("Cannot read simulation scenario %1: %2")
                    .arg(info.fileName(), file.errorString()));
            continue;
        }
        const auto parsed = parseZeroSlackStimulusScenario(file.readAll());
        if (!parsed.ok()) {
            result.diagnostics.append(
                QStringLiteral("Ignored invalid simulation scenario %1: %2")
                    .arg(info.fileName(), parsed.error));
            continue;
        }
        const auto foldedName = QString::fromStdString(parsed.scenario->name)
                                    .trimmed().toCaseFolded();
        if (!ids.insert(parsed.scenario->scenarioId).second
            || !names.insert(foldedName).second) {
            result.diagnostics.append(
                QStringLiteral("Ignored duplicate simulation scenario %1")
                    .arg(info.fileName()));
            continue;
        }
        result.scenarios.push_back({
            *parsed.scenario,
            info.absoluteFilePath(),
            info.fileName().compare(
                QString::fromLatin1(DefaultSimulationScenarioFileName),
                Qt::CaseInsensitive) == 0,
        });
    }
    return result;
}

SimulationScenarioStoreWriteResult saveSimulationScenario(
    const QString& directory,
    const ZeroSlackStimulusScenario& scenario,
    const bool isDefault)
{
    SimulationScenarioStoreWriteResult result;
    if (directory.trimmed().isEmpty() || scenario.scenarioId.empty()) {
        result.error = QStringLiteral("Simulation scenario directory or identity is empty.");
        return result;
    }
    result.filePath = QDir(normalizedDirectory(directory)).filePath(
        simulationScenarioFileName(scenario.scenarioId, isDefault));
    const auto document = serializeZeroSlackStimulusScenario(scenario);
    const auto parsed = parseZeroSlackStimulusScenario(document);
    if (!parsed.ok()) {
        result.error = QStringLiteral("Cannot verify simulation scenario before save: %1")
                           .arg(parsed.error);
        return result;
    }
    writeAtomic(result.filePath, document, result.error);
    return result;
}

bool removeSimulationScenario(
    const QString& directory,
    const std::string& scenarioId,
    QString* error)
{
    if (directory.trimmed().isEmpty() || scenarioId.empty()) {
        if (error) *error = QStringLiteral("Simulation scenario directory or identity is empty.");
        return false;
    }
    const auto path = QDir(normalizedDirectory(directory)).filePath(
        simulationScenarioFileName(scenarioId, false));
    if (!QFileInfo::exists(path) || QFile::remove(path)) return true;
    if (error) {
        *error = QStringLiteral("Cannot remove simulation scenario: %1").arg(path);
    }
    return false;
}

} // namespace wave
