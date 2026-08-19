#include "wave/widgets.h"

#include "main_window.h"
#include "wave/project_io.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

int ModuleAnchor = 0;

QString adjacentWellenReader()
{
    auto directory = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    HMODULE module = nullptr;
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ModuleAnchor),
            &module)) {
        std::wstring path(32'768, L'\0');
        const auto length = GetModuleFileNameW(
            module, path.data(), static_cast<DWORD>(path.size()));
        if (length > 0 && length < path.size()) {
            path.resize(length);
            directory = QFileInfo(QString::fromStdWString(path))
                            .absolutePath();
        }
    }
    constexpr auto executable = "wave-wellen-reader.exe";
#else
    Dl_info moduleInfo{};
    if (dladdr(static_cast<const void*>(&ModuleAnchor), &moduleInfo) != 0
        && moduleInfo.dli_fname) {
        directory = QFileInfo(QString::fromLocal8Bit(moduleInfo.dli_fname))
                        .absolutePath();
    }
    constexpr auto executable = "wave-wellen-reader";
#endif
    return QDir(directory).filePath(QString::fromLatin1(executable));
}

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
        const auto wellenReader = adjacentWellenReader();
        auto* window = new wave::MainWindow(
            *loaded.project,
            projectPath,
            parent,
            std::nullopt,
            true,
            wellenReader);
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
        QStringList capabilities{
            QStringLiteral("internal-signal-hierarchy/v1"),
            QStringLiteral("multi-clock-async-events/v1"),
            QStringLiteral("expected-actual-compare/v1"),
            QStringLiteral("lightweight-trace-checks/v1"),
            QStringLiteral("explicit-unresolved-module-stubs/v1")};
        if (QFileInfo(wellenReader).isFile()) {
            capabilities.append(QStringLiteral("on-demand-fst-trace/v1"));
        }
        window->setProperty("wavewidgets.capabilities", capabilities);
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
