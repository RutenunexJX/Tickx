#include "main_window.h"
#include "trace_canvas.h"
#include "wave_canvas.h"
#include "wave/project_io.h"
#include "wave/widgets.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QWidget>

#include <array>
#include <iostream>

namespace {
int failures = 0;

void check(const bool condition, const char* message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}
} // namespace

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    const QString projectPath = QDir(QString::fromUtf8(WAVE_SOURCE_DIR))
        .absoluteFilePath(QStringLiteral("examples/handshake/project.wave.json"));
    const auto loaded = wave::loadProjectFile(projectPath);
    check(loaded.ok(), "reference project contract loads");

    QWidget owner;
    QWidget* workspace = nullptr;
    std::array<char, 2048> error{};
    const int result = wavewidgets_create_simulation_workspace_v1(
        projectPath.toUtf8().constData(),
        &owner,
        &workspace,
        error.data(),
        error.size());
    check(wavewidgets_abi_version() == wave::kWaveWidgetsAbiVersion,
          "runtime ABI matches the public contract");
    check(result == 0 && workspace,
          error.front() ? error.data() : "embedded workspace is created");
    if (workspace) {
        check(!workspace->isWindow(),
              "embedded workspace is a child widget rather than a top-level window");
        check(workspace->property("wavewidgets.contract").toString()
                  == QString::fromLatin1(wave::kSimulationWorkspaceContract),
              "embedded workspace publishes its stable contract");
        check(!loaded.ok()
                  || workspace->property("wavewidgets.projectId").toString()
                         == QString::fromStdString(loaded.project->id),
              "standalone and embedded paths consume the same project identity");
        check(workspace->findChild<wave::WaveCanvas*>(
                  QStringLiteral("StimulusCanvas")),
              "embedded workspace exposes the shared stimulus canvas");
        check(workspace->findChild<wave::TraceCanvas*>(
                  QStringLiteral("ActualTraceCanvas")),
              "embedded workspace exposes the shared trace canvas");
        workspace->close();
        delete workspace;
    }

    std::cout << "wavewidgets ABI failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
