#include "wave/widgets.h"

#include "main_window.h"
#include "wave/project_io.h"

#include <QByteArray>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cstring>

namespace {

void writeError(
    const QString& message,
    char* destination,
    const std::size_t capacity) noexcept
{
    if (!destination || capacity == 0) return;
    const QByteArray utf8 = message.toUtf8();
    const auto count = std::min(capacity - 1, static_cast<std::size_t>(utf8.size()));
    if (count > 0) std::memcpy(destination, utf8.constData(), count);
    destination[count] = '\0';
}

} // namespace

int wavewidgets_abi_version() noexcept
{
    return wave::kWaveWidgetsAbiVersion;
}

int wavewidgets_create_simulation_workspace_v1(
    const char* projectPathUtf8,
    QWidget* parent,
    QWidget** workspace,
    char* errorUtf8,
    const std::size_t errorCapacity) noexcept
{
    if (workspace) *workspace = nullptr;
    if (!workspace || !projectPathUtf8 || *projectPathUtf8 == '\0') {
        writeError(QStringLiteral("A result project path is required."), errorUtf8, errorCapacity);
        return 1;
    }

    try {
        const QString requestedPath = QString::fromUtf8(projectPathUtf8);
        const QString projectPath = wave::preferredProjectLoadPath(requestedPath);
        const auto loaded = wave::loadProjectFile(projectPath);
        if (!loaded.ok()) {
            writeError(loaded.error, errorUtf8, errorCapacity);
            return 2;
        }

        const QString projectId = QString::fromStdString(loaded.project->id);
        const int scenarioCount = static_cast<int>(loaded.project->scenarios.size());
        auto* window = new wave::MainWindow(
            *loaded.project, projectPath, parent, std::nullopt, true);
        window->setWindowFlag(Qt::Window, false);
        window->setAttribute(Qt::WA_DeleteOnClose, false);
        window->setProperty(
            "wavewidgets.contract",
            QString::fromLatin1(wave::kSimulationWorkspaceContract));
        window->setProperty("wavewidgets.abiVersion", wave::kWaveWidgetsAbiVersion);
        window->setProperty(
            "wavewidgets.projectPath", QFileInfo(projectPath).absoluteFilePath());
        window->setProperty("wavewidgets.projectId", projectId);
        window->setProperty("wavewidgets.scenarioCount", scenarioCount);
        window->setProperty(
            "wavewidgets.capabilities",
            QStringList{
                QStringLiteral("internal-signal-hierarchy/v1"),
                QStringLiteral("multi-clock-async-events/v1"),
                QStringLiteral("expected-actual-compare/v1"),
                QStringLiteral("lightweight-trace-checks/v1")});
        *workspace = window;
        writeError(QString(), errorUtf8, errorCapacity);
        return 0;
    } catch (const std::exception& exception) {
        writeError(QString::fromUtf8(exception.what()), errorUtf8, errorCapacity);
        return 3;
    } catch (...) {
        writeError(QStringLiteral("The embedded workspace could not be created."), errorUtf8, errorCapacity);
        return 4;
    }
}
