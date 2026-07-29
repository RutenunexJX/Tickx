#include "main_window.h"
#include "wave_canvas.h"

#include "wave/model.h"
#include "wave/integration.h"
#include "wave/project_io.h"

#include <QAbstractButton>
#include <QAction>
#include <QAbstractItemModel>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHelpEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>

#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <vector>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        application.setFont(QFont(QStringLiteral("Segoe UI"), 9));
        qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& message) {
            const auto utf8 = message.toUtf8();
            std::fprintf(stderr, "%s\n", utf8.constData());
            std::fflush(stderr);
        });
    }
    QCoreApplication::setOrganizationName(QStringLiteral("WaveWorkbench"));
    QCoreApplication::setApplicationName(QStringLiteral("Wave Workbench"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    const auto testSettingsDirectory =
        qEnvironmentVariable("WAVEWORKBENCH_SETTINGS_DIR");
    if (!testSettingsDirectory.isEmpty()) {
        QDir().mkpath(testSettingsDirectory);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(
            QSettings::IniFormat,
            QSettings::UserScope,
            QDir::cleanPath(testSettingsDirectory));
    }

    bool smokeTest = false;
    bool compareMode = false;
    QString projectPath;
    QString screenshotPath;
    QString laneDialogScreenshotPath;
    QString editMenuScreenshotPath;
    bool waveformOnlySmoke = false;
    QString waveformOnlyScreenshotPath;
    QString cursorModeScreenshotPath;
    bool cursorModeSmoke = false;
    QString waveEditScreenshotPath;
    bool waveEditSmoke = false;
    bool newProjectSmoke = false;
    QString uriText;
    QString autosaveSmokePath;
    bool laneRemovalSmoke = false;
    bool laneReorderSmoke = false;
    bool laneAutoScrollSmoke = false;
    QString laneAutoScrollScreenshotPath;
    bool waveEditAutoScrollSmoke = false;
    QString waveEditAutoScrollScreenshotPath;
    bool hiddenLaneSmoke = false;
    QString hiddenLaneScreenshotPath;
    bool groupHeaderSmoke = false;
    bool signalHeaderSmoke = false;
    bool canvasAddLaneSmoke = false;
    QString canvasAddLaneScreenshotPath;
    bool userJourneySmoke = false;
    QString userJourneySavePath;
    for (int index = 1; index < argc; ++index) {
        const auto argument = QString::fromLocal8Bit(argv[index]);
        if (argument == QStringLiteral("--smoke-test")) {
            smokeTest = true;
        } else if (argument == QStringLiteral("--mode=compare")) {
            compareMode = true;
        } else if (argument.startsWith(QStringLiteral("--screenshot="))) {
            screenshotPath = argument.mid(QStringLiteral("--screenshot=").size());
        } else if (argument.startsWith(QStringLiteral("--lane-dialog-screenshot="))) {
            laneDialogScreenshotPath = argument.mid(
                QStringLiteral("--lane-dialog-screenshot=").size());
        } else if (argument.startsWith(QStringLiteral("--edit-menu-screenshot="))) {
            editMenuScreenshotPath = argument.mid(
                QStringLiteral("--edit-menu-screenshot=").size());
        } else if (argument.startsWith(QStringLiteral("--waveform-only-smoke="))) {
            waveformOnlySmoke = true;
            waveformOnlyScreenshotPath = argument.mid(
                QStringLiteral("--waveform-only-smoke=").size());
        } else if (argument == QStringLiteral("--waveform-only-smoke")) {
            waveformOnlySmoke = true;
        } else if (argument == QStringLiteral("--lane-removal-smoke")) {
            laneRemovalSmoke = true;
        } else if (argument == QStringLiteral("--lane-reorder-smoke")) {
            laneReorderSmoke = true;
        } else if (argument.startsWith(QStringLiteral("--lane-autoscroll-smoke="))) {
            laneAutoScrollSmoke = true;
            laneAutoScrollScreenshotPath = argument.mid(
                QStringLiteral("--lane-autoscroll-smoke=").size());
        } else if (argument == QStringLiteral("--lane-autoscroll-smoke")) {
            laneAutoScrollSmoke = true;
        } else if (argument.startsWith(
                       QStringLiteral("--wave-edit-autoscroll-smoke="))) {
            waveEditAutoScrollSmoke = true;
            waveEditAutoScrollScreenshotPath = argument.mid(
                QStringLiteral("--wave-edit-autoscroll-smoke=").size());
        } else if (argument == QStringLiteral(
                       "--wave-edit-autoscroll-smoke")) {
            waveEditAutoScrollSmoke = true;
        } else if (argument.startsWith(QStringLiteral("--hidden-lane-smoke="))) {
            hiddenLaneSmoke = true;
            hiddenLaneScreenshotPath = argument.mid(
                QStringLiteral("--hidden-lane-smoke=").size());
        } else if (argument == QStringLiteral("--hidden-lane-smoke")) {
            hiddenLaneSmoke = true;
        } else if (argument == QStringLiteral("--group-header-smoke")) {
            groupHeaderSmoke = true;
        } else if (argument == QStringLiteral("--signal-header-smoke")) {
            signalHeaderSmoke = true;
        } else if (argument.startsWith(QStringLiteral("--canvas-add-lane-smoke="))) {
            canvasAddLaneSmoke = true;
            canvasAddLaneScreenshotPath = argument.mid(
                QStringLiteral("--canvas-add-lane-smoke=").size());
        } else if (argument == QStringLiteral("--canvas-add-lane-smoke")) {
            canvasAddLaneSmoke = true;
        } else if (argument.startsWith(QStringLiteral("--cursor-mode-smoke="))) {
            cursorModeSmoke = true;
            cursorModeScreenshotPath = argument.mid(
                QStringLiteral("--cursor-mode-smoke=").size());
        } else if (argument == QStringLiteral("--cursor-mode-smoke")) {
            cursorModeSmoke = true;
        } else if (argument.startsWith(QStringLiteral("--wave-edit-smoke="))) {
            waveEditSmoke = true;
            waveEditScreenshotPath = argument.mid(
                QStringLiteral("--wave-edit-smoke=").size());
        } else if (argument == QStringLiteral("--wave-edit-smoke")) {
            waveEditSmoke = true;
        } else if (argument == QStringLiteral("--new-project-smoke")) {
            newProjectSmoke = true;
        } else if (argument.startsWith(QStringLiteral("--user-journey-smoke="))) {
            userJourneySmoke = true;
            userJourneySavePath = argument.mid(
                QStringLiteral("--user-journey-smoke=").size());
        } else if (argument == QStringLiteral("--user-journey-smoke")) {
            userJourneySmoke = true;
        } else if (argument.startsWith(QStringLiteral("--uri="))) {
            uriText = argument.mid(QStringLiteral("--uri=").size());
        } else if (argument.startsWith(QStringLiteral("--autosave-smoke="))) {
            autosaveSmokePath = argument.mid(QStringLiteral("--autosave-smoke=").size());
        } else if (argument.startsWith(QStringLiteral("waveworkbench://"), Qt::CaseInsensitive)) {
            uriText = argument;
        } else if (!argument.startsWith(QLatin1Char('-'))) {
            projectPath = argument;
        }
    }

    std::optional<wave::LaunchRequest> launchRequest;
    if (!uriText.isEmpty()) {
        const auto parsed = wave::parseWaveWorkbenchUri(QUrl(uriText));
        if (!parsed.ok()) {
            if (smokeTest) {
                qCritical().noquote() << parsed.error;
                return 2;
            }
            QMessageBox::critical(nullptr, QObject::tr("Invalid Wave Workbench URI"), parsed.error);
            return 2;
        }
        launchRequest = *parsed.request;
        projectPath = launchRequest->projectPath;
        compareMode = compareMode || launchRequest->compareMode;
    }

    const auto automationMode = smokeTest
        || waveformOnlySmoke
        || cursorModeSmoke
        || waveEditSmoke
        || newProjectSmoke
        || userJourneySmoke
        || laneRemovalSmoke
        || laneReorderSmoke
        || laneAutoScrollSmoke
        || waveEditAutoScrollSmoke
        || hiddenLaneSmoke
        || groupHeaderSmoke
        || signalHeaderSmoke
        || canvasAddLaneSmoke
        || !screenshotPath.isEmpty()
        || !laneDialogScreenshotPath.isEmpty()
        || !editMenuScreenshotPath.isEmpty()
        || !autosaveSmokePath.isEmpty();
    if (automationMode && !testSettingsDirectory.isEmpty()) {
        QSettings settings;
        settings.clear();
        settings.sync();
    }
    if (projectPath.isEmpty()
        && uriText.isEmpty()
        && !compareMode
        && !automationMode) {
        projectPath = wave::preferredProjectLoadPath({});
    }
    auto project = wave::makeDemonstrationProject();
    if (projectPath.isEmpty()
        && uriText.isEmpty()
        && !compareMode
        && (!automationMode || newProjectSmoke || userJourneySmoke)) {
        project = {};
        project.id = wave::makeStableId("project");
        project.name = "Untitled";
        project.timeBase.picosecondsPerTick = 1;
        wave::Scenario scenario;
        scenario.id = wave::makeStableId("scenario");
        scenario.name = "Waveform";
        scenario.duration = 200'000;
        project.scenarios.push_back(std::move(scenario));
    }
    if (!projectPath.isEmpty()) {
        projectPath = wave::preferredProjectLoadPath(projectPath);
        const auto loadResult = wave::loadProjectFile(projectPath);
        if (!loadResult.ok()) {
            if (smokeTest) {
                qCritical().noquote() << loadResult.error;
                return 2;
            }
            QMessageBox::critical(nullptr, QObject::tr("Open failed"), loadResult.error);
            return 2;
        }
        project = *loadResult.project;
        if (launchRequest && !launchRequest->scenarioId.isEmpty()) {
            const auto scenarioId = launchRequest->scenarioId.toStdString();
            const auto selected = std::find_if(
                project.scenarios.begin(),
                project.scenarios.end(),
                [&scenarioId](const wave::Scenario& scenario) {
                    return scenario.id == scenarioId;
                });
            if (selected == project.scenarios.end()) {
                if (smokeTest) {
                    qCritical().noquote() << "URI scenario does not exist";
                    return 2;
                }
                QMessageBox::critical(
                    nullptr,
                    QObject::tr("Open failed"),
                    QObject::tr("The URI scenario ID does not exist in the project."));
                return 2;
            }
            std::rotate(project.scenarios.begin(), selected, std::next(selected));
        }
    }
    if (!autosaveSmokePath.isEmpty()) {
        project.importedTraces.clear();
    }
    if (signalHeaderSmoke && !project.scenarios.empty()) {
        for (auto& lane : project.scenarios.front().lanes) {
            if (lane.id == "lane-request") {
                lane.name = "soc_top.peripheral_cluster.handshake_controller.request_valid__distinguishing_suffix";
            } else if (lane.id == "lane-ack") {
                lane.name = "soc_top.peripheral_cluster.handshake_controller.acknowledge_ready__distinguishing_suffix";
            }
        }
    }
    if (waveEditAutoScrollSmoke && !project.scenarios.empty()) {
        auto& scenario = project.scenarios.front();
        scenario.name = "Long timeline editing";
        scenario.duration = 1'000'000;
        scenario.events.clear();
        scenario.relations.clear();
        scenario.markers.clear();
        scenario.lanes.clear();
        wave::Lane lane;
        lane.id = "lane-wave-edit-scroll";
        lane.name = "data[7:0]";
        lane.kind = wave::LaneKind::Bus;
        lane.width = 8;
        lane.color = "#64b5f6";
        lane.height = 72;
        wave::Segment segment;
        segment.id = "segment-wave-edit-scroll";
        segment.start = 50'000;
        segment.end = 100'000;
        segment.value = "0x35";
        lane.segments.push_back(std::move(segment));
        scenario.lanes.push_back(std::move(lane));

        project.clockDomains.clear();
        wave::ClockDomain clock;
        clock.id = "clock-wave-edit-scroll";
        clock.name = "navigation clock";
        clock.period = 10'000;
        clock.phase = 0;
        clock.dutyCycle = {1, 2};
        project.clockDomains.push_back(std::move(clock));
        wave::Lane enumLane;
        enumLane.id = "lane-wave-edit-enum";
        enumLane.name = "state";
        enumLane.kind = wave::LaneKind::Enum;
        enumLane.width = 2;
        enumLane.clockDomainId = "clock-wave-edit-scroll";
        enumLane.color = "#ef9a9a";
        enumLane.height = 56;
        enumLane.enumMap = {{"IDLE", "0"}, {"WAIT_ACK", "1"}, {"DONE", "2"}};
        wave::Segment idleState;
        idleState.id = "segment-wave-edit-enum-idle";
        idleState.start = 0;
        idleState.end = 50'000;
        idleState.value = "IDLE";
        enumLane.segments.push_back(std::move(idleState));
        wave::Segment waitingState;
        waitingState.id = "segment-wave-edit-enum-wait";
        waitingState.start = 50'000;
        waitingState.end = 100'000;
        waitingState.value = "WAIT_ACK";
        enumLane.segments.push_back(std::move(waitingState));
        wave::Segment doneState;
        doneState.id = "segment-wave-edit-enum-done";
        doneState.start = 100'000;
        doneState.end = scenario.duration;
        doneState.value = "DONE";
        enumLane.segments.push_back(std::move(doneState));
        scenario.lanes.push_back(std::move(enumLane));
        wave::Lane enumNextLane;
        enumNextLane.id = "lane-wave-edit-enum-next";
        enumNextLane.name = "state_next";
        enumNextLane.kind = wave::LaneKind::Enum;
        enumNextLane.width = 2;
        enumNextLane.clockDomainId = "clock-wave-edit-scroll";
        enumNextLane.color = "#ffcc80";
        enumNextLane.height = 56;
        enumNextLane.enumMap = {
            {"IDLE", "0"},
            {"DONE", "2"},
            {"ERROR", "3"},
        };
        wave::Segment nextIdleState;
        nextIdleState.id = "segment-wave-edit-enum-next-idle";
        nextIdleState.start = 0;
        nextIdleState.end = scenario.duration;
        nextIdleState.value = "IDLE";
        enumNextLane.segments.push_back(std::move(nextIdleState));
        scenario.lanes.push_back(std::move(enumNextLane));
        wave::Lane clockLane;
        clockLane.id = "lane-wave-edit-clock";
        clockLane.name = "clk";
        clockLane.kind = wave::LaneKind::Clock;
        clockLane.clockDomainId = "clock-wave-edit-scroll";
        clockLane.color = "#81c784";
        clockLane.height = 56;
        scenario.lanes.push_back(std::move(clockLane));

        project.name = "Wave Edit autoscroll";
        project.importedTraces.clear();
        project.linkedResources.clear();
    }
    if (laneAutoScrollSmoke && !project.scenarios.empty()) {
        auto& scenario = project.scenarios.front();
        scenario.name = "Long signal list";
        scenario.events.clear();
        scenario.relations.clear();
        scenario.markers.clear();
        scenario.lanes.clear();
        const std::array<std::string, 4> colors{
            "#64b5f6", "#81c784", "#ffb74d", "#ce93d8"};
        for (auto index = 0; index < 20; ++index) {
            const auto suffix = QStringLiteral("%1")
                                    .arg(index, 2, 10, QLatin1Char('0'))
                                    .toStdString();
            wave::Lane lane;
            lane.id = "lane-scroll-" + suffix;
            lane.name = "signal_" + suffix;
            lane.kind = index == 5
                ? wave::LaneKind::Group
                : wave::LaneKind::Bit;
            if (index == 5) lane.name = "group_05";
            lane.color = colors.at(static_cast<std::size_t>(index) % colors.size());
            lane.height = 56;
            scenario.lanes.push_back(std::move(lane));
        }
        project.name = "Lane autoscroll";
        project.clockDomains.clear();
        project.importedTraces.clear();
        project.linkedResources.clear();
    }

    wave::MainWindow window(
        std::move(project),
        autosaveSmokePath.isEmpty() ? projectPath : autosaveSmokePath);
    window.show();
    if (compareMode) window.requestCompareMode();
    if (launchRequest && launchRequest->tick) {
        window.revealLocation(launchRequest->laneId, *launchRequest->tick);
    }
    if (!autosaveSmokePath.isEmpty()) {
        QTimer::singleShot(0, &window, [&application, &window] {
            if (!QMetaObject::invokeMethod(&window, "markEdited", Qt::DirectConnection)) {
                qCritical().noquote() << "Cannot trigger autosave smoke edit";
                window.hide();
                application.exit(4);
            }
        });
        QTimer::singleShot(2'500, &application, [&application, &window, autosaveSmokePath] {
            const auto snapshotPath = autosaveSmokePath + QStringLiteral(".autosave");
            const auto loaded = wave::loadProjectFile(snapshotPath);
            auto* saveState = window.findChild<QLabel*>(QStringLiteral("SaveStateLabel"));
            const auto fail = [&application, &window](const QString& message) {
                qCritical().noquote() << message;
                window.hide();
                application.exit(4);
            };
            if (!QFileInfo::exists(snapshotPath) || !loaded.ok() || !saveState) {
                fail(QStringLiteral("Autosave recovery snapshot is missing or invalid"));
                return;
            }
            if (!QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection)) {
                fail(QStringLiteral("Cannot save the autosave smoke project"));
                return;
            }
            QCoreApplication::processEvents();
            const auto saved = wave::loadProjectFile(autosaveSmokePath);
            if (QFileInfo::exists(snapshotPath)
                || !saved.ok()
                || saveState->text() != QStringLiteral("Saved")) {
                fail(QStringLiteral("Formal save did not remove the completed recovery snapshot"));
                return;
            }

            if (!QMetaObject::invokeMethod(&window, "markEdited", Qt::DirectConnection)
                || !QMetaObject::invokeMethod(&window, "startAutosave", Qt::DirectConnection)
                || !QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection)) {
                fail(QStringLiteral("Cannot trigger the in-flight autosave cleanup scenario"));
                return;
            }
            QTimer::singleShot(
                750,
                &application,
                [&application,
                 &window,
                 autosaveSmokePath,
                 snapshotPath,
                 saveState,
                 fail] {
                    const auto savedAfterRace = wave::loadProjectFile(autosaveSmokePath);
                    if (QFileInfo::exists(snapshotPath)
                        || !savedAfterRace.ok()
                        || saveState->text() != QStringLiteral("Saved")
                        || !window.statusBar()->currentMessage().contains(
                            QStringLiteral("Saved"))) {
                        fail(QStringLiteral(
                            "Stale in-flight autosave survived or obscured the formal save"));
                        return;
                    }

                    auto recoveryProject = window.project();
                    const auto recoveredDuration =
                        recoveryProject.scenarios.front().duration + 1'234;
                    recoveryProject.scenarios.front().duration = recoveredDuration;
                    QString recoveryWriteError;
                    if (!wave::saveProjectFileAtomic(
                            recoveryProject,
                            snapshotPath,
                            &recoveryWriteError)) {
                        fail(QStringLiteral("Cannot create a newer recovery snapshot: %1")
                                 .arg(recoveryWriteError));
                        return;
                    }
                    QFile recoveryFile(snapshotPath);
                    const auto newerTime =
                        QFileInfo(autosaveSmokePath).lastModified().addSecs(2);
                    if (!recoveryFile.open(QIODevice::ReadWrite)
                        || !recoveryFile.setFileTime(
                            newerTime,
                            QFileDevice::FileModificationTime)) {
                        fail(QStringLiteral("Cannot make the recovery snapshot newer than the project"));
                        return;
                    }
                    recoveryFile.close();

                    const auto selectedPath =
                        wave::preferredProjectLoadPath(autosaveSmokePath);
                    const auto recoveryLoad = wave::loadProjectFile(selectedPath);
                    if (selectedPath != snapshotPath || !recoveryLoad.ok()) {
                        fail(QStringLiteral("A newer valid recovery snapshot was not selected"));
                        return;
                    }

                    window.hide();
                    wave::MainWindow recoveredWindow(
                        *recoveryLoad.project,
                        selectedPath);
                    recoveredWindow.show();
                    QCoreApplication::processEvents();
                    auto* recoveredState = recoveredWindow.findChild<QLabel*>(
                        QStringLiteral("SaveStateLabel"));
                    if (!recoveredState
                        || recoveredState->text()
                            != QStringLiteral("Recovery loaded · Save required")
                        || recoveredState->toolTip() != autosaveSmokePath
                        || recoveredWindow.project().scenarios.front().duration
                            != recoveredDuration
                        || !recoveredWindow.windowTitle().contains(QStringLiteral(" *"))
                        || !recoveredWindow.statusBar()->currentMessage().contains(
                            QStringLiteral("Recovery snapshot loaded"))) {
                        fail(QStringLiteral(
                            "Crash restart did not surface the newer recovery snapshot safely"));
                        return;
                    }
                    recoveredWindow.hide();

                    QFile olderRecovery(snapshotPath);
                    const auto olderTime =
                        QFileInfo(autosaveSmokePath).lastModified().addSecs(-2);
                    if (!olderRecovery.open(QIODevice::ReadWrite)
                        || !olderRecovery.setFileTime(
                            olderTime,
                            QFileDevice::FileModificationTime)) {
                        fail(QStringLiteral("Cannot age the recovery snapshot for fallback testing"));
                        return;
                    }
                    olderRecovery.close();
                    if (wave::preferredProjectLoadPath(autosaveSmokePath)
                        != autosaveSmokePath) {
                        fail(QStringLiteral("An older recovery snapshot replaced the saved project"));
                        return;
                    }

                    QFile invalidRecovery(snapshotPath);
                    if (!invalidRecovery.open(QIODevice::WriteOnly | QIODevice::Truncate)
                        || invalidRecovery.write("{invalid recovery") < 0
                        || !invalidRecovery.setFileTime(
                            newerTime.addSecs(2),
                            QFileDevice::FileModificationTime)) {
                        fail(QStringLiteral("Cannot create an invalid newer recovery snapshot"));
                        return;
                    }
                    invalidRecovery.close();
                    if (wave::preferredProjectLoadPath(autosaveSmokePath)
                        != autosaveSmokePath) {
                        fail(QStringLiteral("An invalid recovery snapshot replaced the saved project"));
                        return;
                    }

                    QString restoredRecoveryError;
                    if (!wave::saveProjectFileAtomic(
                            recoveryProject,
                            snapshotPath,
                            &restoredRecoveryError)) {
                        fail(QStringLiteral("Cannot restore the recovery snapshot for discard testing: %1")
                                 .arg(restoredRecoveryError));
                        return;
                    }
                    QFile restoredRecovery(snapshotPath);
                    if (!restoredRecovery.open(QIODevice::ReadWrite)
                        || !restoredRecovery.setFileTime(
                            newerTime.addSecs(4),
                            QFileDevice::FileModificationTime)) {
                        fail(QStringLiteral("Cannot timestamp the discard recovery snapshot"));
                        return;
                    }
                    restoredRecovery.close();

                    recoveredWindow.show();
                    if (!QMetaObject::invokeMethod(
                            &recoveredWindow,
                            "startAutosave",
                            Qt::DirectConnection)) {
                        fail(QStringLiteral("Cannot start the discard race autosave"));
                        return;
                    }
                    bool discardHandled = false;
                    QTimer::singleShot(
                        0,
                        &application,
                        [&discardHandled] {
                            auto* box = qobject_cast<QMessageBox*>(
                                QApplication::activeModalWidget());
                            auto* discard = box
                                ? box->button(QMessageBox::Discard)
                                : nullptr;
                            if (!discard) return;
                            discardHandled = true;
                            discard->click();
                        });
                    if (!QMetaObject::invokeMethod(
                            &recoveredWindow,
                            "newProject",
                            Qt::DirectConnection)) {
                        fail(QStringLiteral("Cannot trigger recovery discard through New"));
                        return;
                    }
                    QEventLoop settleLoop;
                    QTimer::singleShot(100, &settleLoop, &QEventLoop::quit);
                    settleLoop.exec();
                    auto* discardedState = recoveredWindow.findChild<QLabel*>(
                        QStringLiteral("SaveStateLabel"));
                    if (!discardHandled
                        || QApplication::activeModalWidget()
                        || QFileInfo::exists(snapshotPath)
                        || wave::preferredProjectLoadPath(autosaveSmokePath)
                            != autosaveSmokePath
                        || !discardedState
                        || discardedState->text() != QStringLiteral("Not saved")
                        || recoveredWindow.project().name != "Untitled"
                        || recoveredWindow.project().scenarios.size() != 1
                        || !recoveredWindow.project().scenarios.front().lanes.empty()
                        || recoveredWindow.windowTitle().contains(QStringLiteral(" *"))) {
                        fail(QStringLiteral(
                            "Discard did not durably remove recovered and in-flight changes"));
                        return;
                    }
                    recoveredWindow.hide();
                    application.exit(0);
                });
        });
    } else if (newProjectSmoke) {
        const auto recoveryPath = wave::untitledRecoveryPath();
        QFile::remove(recoveryPath);
        QTimer::singleShot(0, &window, [&application, &window, recoveryPath] {
            const auto fail = [&application, &window](const QString& message) {
                qCritical().noquote() << message;
                if (auto* modal = QApplication::activeModalWidget()) modal->close();
                window.hide();
                application.exit(4);
            };
            auto* action = window.findChild<QAction*>(QStringLiteral("NewProjectAction"));
            auto* canvas = window.findChild<wave::WaveCanvas*>();
            auto* durationEdit = window.findChild<QLineEdit*>(
                QStringLiteral("TimelineDurationEdit"));
            auto* saveState = window.findChild<QLabel*>(QStringLiteral("SaveStateLabel"));
            auto* addClock = window.findChild<QToolButton*>(
                QStringLiteral("CanvasAddClockButton"));
            if (!action || !canvas || !durationEdit || !saveState || !addClock
                || action->shortcut().matches(QKeySequence::New) != QKeySequence::ExactMatch
                || action->text().contains(QChar(0x2026))) {
                fail(QStringLiteral(
                    "New project direct action or blank-state controls are missing"));
                return;
            }

            const auto verifyBlank = [&] {
                const auto& project = window.project();
                return project.name == "Untitled"
                    && project.timeBase.picosecondsPerTick == 1
                    && project.scenarios.size() == 1
                    && project.scenarios.front().duration == 200'000
                    && project.scenarios.front().lanes.empty()
                    && canvas->tool() == wave::WaveCanvas::Tool::WaveEdit
                    && durationEdit->text() == QStringLiteral("200 ns")
                    && saveState->text().startsWith(QStringLiteral("Not saved"))
                    && addClock->isVisible()
                    && addClock->geometry().left() > 190;
            };
            if (!verifyBlank()) {
                fail(QStringLiteral(
                    "Default startup did not expose the direct 200 ns blank waveform"));
                return;
            }

            durationEdit->setFocus(Qt::OtherFocusReason);
            durationEdit->setText(QStringLiteral("300 ns"));
            durationEdit->setModified(true);
            QKeyEvent cancelDuration(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QCoreApplication::sendEvent(durationEdit, &cancelDuration);
            QCoreApplication::processEvents();
            if (!verifyBlank()) {
                fail(QStringLiteral("Escape did not cancel the direct timeline edit"));
                return;
            }

            addClock->click();
            QCoreApplication::processEvents();
            auto* setupPanel = canvas->findChild<QWidget*>(
                QStringLiteral("QuickLaneSetupPanel"));
            auto* quickName = canvas->findChild<QLineEdit*>(
                QStringLiteral("QuickLaneNameEdit"));
            if (!setupPanel || !setupPanel->isVisible() || !quickName) {
                fail(QStringLiteral("Blank-state quick creation did not start inline"));
                return;
            }
            QKeyEvent cancelQuick(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QCoreApplication::sendEvent(quickName, &cancelQuick);
            QCoreApplication::processEvents();
            if (setupPanel->isVisible() || !verifyBlank()) {
                fail(QStringLiteral("Escape did not atomically cancel the quick signal"));
                return;
            }

            action->trigger();
            QCoreApplication::processEvents();
            if (QApplication::activeModalWidget()
                || window.findChild<QDialog*>(QStringLiteral("NewProjectDialog"))
                || !verifyBlank()) {
                fail(QStringLiteral(
                    "New unexpectedly opened configuration or changed defaults"));
                return;
            }

            durationEdit->setFocus(Qt::OtherFocusReason);
            durationEdit->setText(QStringLiteral("300 ns"));
            durationEdit->setModified(true);
            QKeyEvent commitDuration(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QCoreApplication::sendEvent(durationEdit, &commitDuration);
            QCoreApplication::processEvents();
            if (window.project().scenarios.front().duration != 300'000
                || saveState->text() != QStringLiteral("Not saved · changes")) {
                fail(QStringLiteral(
                    "An untitled waveform edit did not enter the unsaved state"));
                return;
            }

            QEventLoop autosaveWait;
            QTimer autosavePoll;
            autosavePoll.setInterval(25);
            QObject::connect(
                &autosavePoll,
                &QTimer::timeout,
                &autosaveWait,
                [&autosaveWait, recoveryPath] {
                    if (!QFileInfo::exists(recoveryPath)) return;
                    if (wave::loadProjectFile(recoveryPath).ok()) autosaveWait.quit();
                });
            QTimer::singleShot(4'000, &autosaveWait, &QEventLoop::quit);
            autosavePoll.start();
            autosaveWait.exec();
            autosavePoll.stop();

            const auto selectedPath = wave::preferredProjectLoadPath({});
            const auto recoveryLoad = wave::loadProjectFile(selectedPath);
            if (selectedPath != recoveryPath
                || !recoveryLoad.ok()
                || recoveryLoad.project->scenarios.front().duration != 300'000) {
                fail(QStringLiteral(
                    "The first unsaved waveform was not captured as an untitled recovery"));
                return;
            }

            window.hide();
            wave::MainWindow recoveredWindow(*recoveryLoad.project, selectedPath);
            recoveredWindow.show();
            QCoreApplication::processEvents();
            auto* recoveredState = recoveredWindow.findChild<QLabel*>(
                QStringLiteral("SaveStateLabel"));
            auto* recoveredDuration = recoveredWindow.findChild<QLineEdit*>(
                QStringLiteral("TimelineDurationEdit"));
            if (!recoveredState
                || !recoveredDuration
                || recoveredState->text()
                    != QStringLiteral("Recovery loaded · Save required")
                || !recoveredState->toolTip().contains(
                    QStringLiteral("not been saved"), Qt::CaseInsensitive)
                || recoveredDuration->text() != QStringLiteral("300 ns")
                || !recoveredWindow.windowTitle().contains(QStringLiteral(" *"))
                || !recoveredWindow.statusBar()->currentMessage().contains(
                    QStringLiteral("Untitled recovery snapshot loaded"))) {
                fail(QStringLiteral(
                    "No-argument restart did not expose the untitled recovery safely"));
                return;
            }

            bool saveAsPresented = false;
            QTimer::singleShot(
                0,
                &application,
                [&saveAsPresented] {
                    auto* dialog = qobject_cast<QFileDialog*>(
                        QApplication::activeModalWidget());
                    if (!dialog) return;
                    saveAsPresented = true;
                    dialog->reject();
                });
            if (!QMetaObject::invokeMethod(
                    &recoveredWindow,
                    "saveProject",
                    Qt::DirectConnection)) {
                fail(QStringLiteral("Cannot invoke Save for the untitled recovery"));
                return;
            }
            QCoreApplication::processEvents();
            if (!saveAsPresented
                || !QFileInfo::exists(recoveryPath)
                || recoveredState->text()
                    != QStringLiteral("Recovery loaded · Save required")) {
                fail(QStringLiteral(
                    "Recovered Untitled did not require a formal Save As destination"));
                return;
            }

            bool discardHandled = false;
            QTimer::singleShot(
                0,
                &application,
                [&discardHandled] {
                    auto* box = qobject_cast<QMessageBox*>(
                        QApplication::activeModalWidget());
                    auto* discard = box ? box->button(QMessageBox::Discard) : nullptr;
                    if (!discard) return;
                    discardHandled = true;
                    discard->click();
                });
            if (!QMetaObject::invokeMethod(
                    &recoveredWindow,
                    "newProject",
                    Qt::DirectConnection)) {
                fail(QStringLiteral("Cannot discard the untitled recovery through New"));
                return;
            }
            QEventLoop settleLoop;
            QTimer::singleShot(100, &settleLoop, &QEventLoop::quit);
            settleLoop.exec();
            auto* discardedState = recoveredWindow.findChild<QLabel*>(
                QStringLiteral("SaveStateLabel"));
            if (!discardHandled
                || QApplication::activeModalWidget()
                || QFileInfo::exists(recoveryPath)
                || !wave::preferredProjectLoadPath({}).isEmpty()
                || !discardedState
                || discardedState->text() != QStringLiteral("Not saved")
                || recoveredWindow.project().name != "Untitled"
                || recoveredWindow.project().scenarios.size() != 1
                || recoveredWindow.project().scenarios.front().duration != 200'000
                || !recoveredWindow.project().scenarios.front().lanes.empty()
                || recoveredWindow.windowTitle().contains(QStringLiteral(" *"))) {
                fail(QStringLiteral(
                    "Discard did not durably remove the untitled recovery"));
                return;
            }
            recoveredWindow.hide();
            application.exit(0);
        });
    } else if (userJourneySmoke) {
        auto userJourneyBareSavePath = userJourneySavePath;
        if (userJourneyBareSavePath.endsWith(
                QStringLiteral(".wave.json"),
                Qt::CaseInsensitive)) {
            userJourneyBareSavePath.chop(QStringLiteral(".wave.json").size());
        }
        const auto userJourneyDropPath =
            QFileInfo(userJourneySavePath).absolutePath()
            + QStringLiteral("/user-journey-dropped.wave.json");
        QFile::remove(userJourneySavePath);
        QFile::remove(userJourneyBareSavePath);
        QFile::remove(userJourneySavePath + QStringLiteral(".autosave"));
        QFile::remove(userJourneyDropPath);
        QFile::remove(userJourneyDropPath + QStringLiteral(".autosave"));
        QTimer::singleShot(
            0,
            &window,
            [&application,
             &window,
             userJourneySavePath,
             userJourneyBareSavePath,
             userJourneyDropPath] {
                const auto fail = [&application, &window](const QString& message) {
                    qCritical().noquote() << message;
                    if (auto* modal = QApplication::activeModalWidget()) modal->close();
                    window.hide();
                    application.exit(4);
                };
                if (userJourneySavePath.isEmpty()) {
                    fail(QStringLiteral("User journey requires an output project path"));
                    return;
                }
                auto* canvas = window.findChild<wave::WaveCanvas*>();
                auto* addClock = window.findChild<QToolButton*>(
                    QStringLiteral("CanvasAddClockButton"));
                auto* addBit = window.findChild<QToolButton*>(
                    QStringLiteral("CanvasAddBitButton"));
                auto* addBus = window.findChild<QToolButton*>(
                    QStringLiteral("CanvasAddBusButton"));
                auto* durationEdit = window.findChild<QLineEdit*>(
                    QStringLiteral("TimelineDurationEdit"));
                auto* measureAction = window.findChild<QAction*>(
                    QStringLiteral("MeasureToolAction"));
                auto* saveState = window.findChild<QLabel*>(
                    QStringLiteral("SaveStateLabel"));
                if (!canvas || !addClock || !addBit || !addBus
                    || !durationEdit || !measureAction || !saveState
                    || !window.project().scenarios.front().lanes.empty()
                    || !saveState->text().startsWith(QStringLiteral("Not saved"))) {
                    fail(QStringLiteral("User journey did not start from the understandable blank state"));
                    return;
                }

                const auto sendKey = [](QObject* target,
                                        const int key,
                                        const Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
                    QKeyEvent press(QEvent::KeyPress, key, modifiers);
                    QCoreApplication::sendEvent(target, &press);
                    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
                    QCoreApplication::sendEvent(target, &release);
                };
                const auto completeQuick = [canvas, &sendKey](
                                               QToolButton* button,
                                               const QString& name,
                                               const QString& parameter) {
                    button->click();
                    QCoreApplication::processEvents();
                    auto* panel = canvas->findChild<QWidget*>(
                        QStringLiteral("QuickLaneSetupPanel"));
                    auto* nameEdit = canvas->findChild<QLineEdit*>(
                        QStringLiteral("QuickLaneNameEdit"));
                    auto* parameterEdit = canvas->findChild<QLineEdit*>(
                        QStringLiteral("QuickLaneParameterEdit"));
                    if (!panel || !panel->isVisible() || !nameEdit || !parameterEdit
                        || QApplication::activeModalWidget()) {
                        return false;
                    }
                    nameEdit->setText(name);
                    if (parameterEdit->isVisible()) parameterEdit->setText(parameter);
                    sendKey(nameEdit, Qt::Key_Return);
                    QCoreApplication::processEvents();
                    return !panel->isVisible();
                };
                if (!completeQuick(addClock, QStringLiteral("sys_clk"), QStringLiteral("20 ns"))
                    || !completeQuick(addBit, QStringLiteral("valid"), {})
                    || !completeQuick(addBus, QStringLiteral("data"), QStringLiteral("16"))) {
                    fail(QStringLiteral("User could not create CLK, BIT and BUS inline"));
                    return;
                }

                const auto& createdProject = window.project();
                const auto& createdScenario = createdProject.scenarios.front();
                const auto* clockLane = std::find_if(
                    createdScenario.lanes.begin(),
                    createdScenario.lanes.end(),
                    [](const wave::Lane& lane) { return lane.name == "sys_clk"; })
                    == createdScenario.lanes.end()
                    ? nullptr
                    : &*std::find_if(
                          createdScenario.lanes.begin(),
                          createdScenario.lanes.end(),
                          [](const wave::Lane& lane) { return lane.name == "sys_clk"; });
                const auto* bitLane = std::find_if(
                    createdScenario.lanes.begin(),
                    createdScenario.lanes.end(),
                    [](const wave::Lane& lane) { return lane.name == "valid"; })
                    == createdScenario.lanes.end()
                    ? nullptr
                    : &*std::find_if(
                          createdScenario.lanes.begin(),
                          createdScenario.lanes.end(),
                          [](const wave::Lane& lane) { return lane.name == "valid"; });
                const auto* busLane = std::find_if(
                    createdScenario.lanes.begin(),
                    createdScenario.lanes.end(),
                    [](const wave::Lane& lane) { return lane.name == "data"; })
                    == createdScenario.lanes.end()
                    ? nullptr
                    : &*std::find_if(
                          createdScenario.lanes.begin(),
                          createdScenario.lanes.end(),
                          [](const wave::Lane& lane) { return lane.name == "data"; });
                const auto* clock = clockLane
                    ? wave::findClock(createdProject, clockLane->clockDomainId)
                    : nullptr;
                if (!clockLane || !bitLane || !busLane || !clock
                    || clock->period != 20'000
                    || bitLane->clockDomainId != clockLane->clockDomainId
                    || busLane->clockDomainId != clockLane->clockDomainId
                    || busLane->width != 16
                    || !saveState->text().contains(QStringLiteral("changes"), Qt::CaseInsensitive)) {
                    fail(QStringLiteral("Inline signal details or unsaved feedback were not applied"));
                    return;
                }
                const auto bitLaneId = bitLane->id;
                const auto busLaneId = busLane->id;

                canvas->fitScenario();
                QCoreApplication::processEvents();
                const auto laneCenter = [&window, canvas](const std::string& laneId) {
                    auto y = 40 - canvas->verticalScrollBar()->value();
                    for (const auto& lane : window.project().scenarios.front().lanes) {
                        if (!lane.visible) continue;
                        const auto height = std::clamp(lane.height, 30, 240);
                        if (lane.id == laneId) return y + height / 2;
                        y += height;
                    }
                    return -1;
                };
                const auto xAtTick = [&window, canvas](const wave::Tick tick) {
                    const auto duration = window.project().scenarios.front().duration;
                    return 190 + static_cast<int>(std::llround(
                        static_cast<double>(tick)
                        / static_cast<double>(duration)
                        * static_cast<double>(canvas->viewport()->width() - 190)))
                        - canvas->horizontalScrollBar()->value();
                };
                const auto sendMouse = [canvas](
                                           const QEvent::Type type,
                                           const QPoint position,
                                           const Qt::MouseButton button,
                                           const Qt::MouseButtons buttons,
                                           const Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
                    QMouseEvent event(
                        type,
                        QPointF(position),
                        QPointF(canvas->viewport()->mapToGlobal(position)),
                        button,
                        buttons,
                        modifiers);
                    QCoreApplication::sendEvent(canvas->viewport(), &event);
                };
                const auto click = [&sendMouse](const QPoint point) {
                    sendMouse(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
                    sendMouse(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
                    QCoreApplication::processEvents();
                };
                const auto valueAt = [](const wave::Lane& lane, const wave::Tick tick) {
                    const auto segment = std::find_if(
                        lane.segments.begin(),
                        lane.segments.end(),
                        [tick](const wave::Segment& candidate) {
                            return candidate.start <= tick && tick < candidate.end;
                        });
                    return segment == lane.segments.end()
                        ? std::string{}
                        : segment->value;
                };

                const auto bitY = laneCenter(bitLaneId);
                click(QPoint(72, bitY));
                canvas->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_F2);
                QCoreApplication::processEvents();
                auto* renameEdit = canvas->findChild<QLineEdit*>(
                    QStringLiteral("LaneRenameEdit"));
                if (!renameEdit
                    || !renameEdit->isVisible()
                    || !renameEdit->hasFocus()
                    || QApplication::activeModalWidget()) {
                    fail(QStringLiteral("User could not rename the selected signal inline"));
                    return;
                }
                renameEdit->setText(QStringLiteral("req_valid"));
                sendKey(renameEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                bitLane = wave::findLane(window.project().scenarios.front(), bitLaneId);
                if (!bitLane
                    || bitLane->name != "req_valid"
                    || renameEdit->isVisible()
                    || !window.statusBar()->currentMessage().contains(QStringLiteral("Ctrl+Z"))) {
                    fail(QStringLiteral("Inline rename gave no clear result or undo feedback"));
                    return;
                }

                click(QPoint(xAtTick(30'000), bitY));
                bitLane = wave::findLane(window.project().scenarios.front(), bitLaneId);
                if (!bitLane
                    || valueAt(*bitLane, 30'000) != "1"
                    || !window.statusBar()->currentMessage().contains(QStringLiteral("Ctrl+Z"))) {
                    fail(QStringLiteral("Single-beat Bit edit gave no clear result or undo feedback"));
                    return;
                }
                sendKey(canvas, Qt::Key_Z, Qt::ControlModifier);
                QCoreApplication::processEvents();
                bitLane = wave::findLane(window.project().scenarios.front(), bitLaneId);
                if (!bitLane
                    || !valueAt(*bitLane, 30'000).empty()
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Undid Toggle bit beat"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Y"))) {
                    fail(QStringLiteral("Ctrl+Z did not confirm the reverted Bit edit"));
                    return;
                }
                sendKey(canvas, Qt::Key_Y, Qt::ControlModifier);
                QCoreApplication::processEvents();
                bitLane = wave::findLane(window.project().scenarios.front(), bitLaneId);
                if (!bitLane
                    || valueAt(*bitLane, 30'000) != "1"
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Redid Toggle bit beat"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    fail(QStringLiteral("Ctrl+Y did not confirm the restored Bit edit"));
                    return;
                }

                const auto busY = laneCenter(busLaneId);
                click(QPoint(xAtTick(70'000), busY));
                auto* palette = window.findChild<QWidget*>(QStringLiteral("BusPresetPalette"));
                auto* busValue = window.findChild<QLineEdit*>(QStringLiteral("BusPresetValueEdit"));
                if (!palette || !palette->isVisible() || !busValue) {
                    fail(QStringLiteral("Bus click did not expose nearby direct controls"));
                    return;
                }
                busValue->setText(QStringLiteral("0x10000"));
                sendKey(busValue, Qt::Key_Return);
                QCoreApplication::processEvents();
                busLane = wave::findLane(window.project().scenarios.front(), busLaneId);
                if (!palette->isVisible()
                    || !busValue->hasFocus()
                    || !busLane
                    || !busLane->segments.empty()) {
                    fail(QStringLiteral("Invalid Bus value did not remain visible and recoverable"));
                    return;
                }
                sendKey(busValue, Qt::Key_S, Qt::ControlModifier);
                QCoreApplication::processEvents();
                if (QApplication::activeModalWidget()
                    || QFileInfo::exists(userJourneySavePath)
                    || !palette->isVisible()
                    || !busValue->hasFocus()
                    || saveState->text() == QStringLiteral("Saved")) {
                    fail(QStringLiteral("Invalid Bus draft did not block Save in place"));
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "exportArtifacts", Qt::DirectConnection)) {
                    fail(QStringLiteral("User journey could not test invalid Bus export gate"));
                    return;
                }
                QCoreApplication::processEvents();
                if (QApplication::activeModalWidget()
                    || !palette->isVisible()
                    || !busValue->hasFocus()) {
                    fail(QStringLiteral("Invalid Bus draft did not block Export in place"));
                    return;
                }
                durationEdit->setText(QStringLiteral("450 ns"));
                durationEdit->setModified(true);
                durationEdit->setFocus(Qt::OtherFocusReason);
                busValue->setFocus(Qt::MouseFocusReason);
                QCoreApplication::processEvents();
                if (!palette->isVisible()
                    || !busValue->hasFocus()
                    || !durationEdit->isModified()
                    || durationEdit->text() != QStringLiteral("450 ns")
                    || window.project().scenarios.front().duration != 200'000) {
                    fail(QStringLiteral("Invalid Bus draft did not preserve the pending End draft"));
                    return;
                }
                busValue->setText(QStringLiteral("0x1234"));
                busValue->setModified(true);
                sendKey(busValue, Qt::Key_Return);
                QCoreApplication::processEvents();
                busLane = wave::findLane(window.project().scenarios.front(), busLaneId);
                if (palette->isVisible()
                    || !busLane
                    || valueAt(*busLane, 70'000) != "0x1234"
                    || !durationEdit->isModified()
                    || durationEdit->text() != QStringLiteral("450 ns")
                    || !window.statusBar()->currentMessage().contains(QStringLiteral("Ctrl+Z"))) {
                    fail(QStringLiteral("Corrected Bus value discarded the pending End draft"));
                    return;
                }

                durationEdit->setText(QStringLiteral("50 ns"));
                sendKey(durationEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                if (window.project().scenarios.front().duration != 200'000
                    || !durationEdit->hasFocus()) {
                    fail(QStringLiteral("Unsafe timeline shortening was not explained in place"));
                    return;
                }
                sendKey(durationEdit, Qt::Key_S, Qt::ControlModifier);
                QCoreApplication::processEvents();
                if (QApplication::activeModalWidget()
                    || QFileInfo::exists(userJourneySavePath)
                    || window.project().scenarios.front().duration != 200'000
                    || !durationEdit->hasFocus()
                    || saveState->text() == QStringLiteral("Saved")) {
                    fail(QStringLiteral("Invalid End draft did not block Save in place"));
                    return;
                }
                durationEdit->setText(QStringLiteral("500 ns"));
                sendKey(durationEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                if (window.project().scenarios.front().duration != 500'000
                    || durationEdit->text() != QStringLiteral("500 ns")) {
                    fail(QStringLiteral("Direct timeline extension did not apply"));
                    return;
                }

                constexpr wave::Tick blurProbeTick = 90'000;
                bitLane = wave::findLane(window.project().scenarios.front(), bitLaneId);
                const auto blurProbeBefore = bitLane ? valueAt(*bitLane, blurProbeTick) : std::string{};
                durationEdit->setFocus(Qt::OtherFocusReason);
                durationEdit->setText(QStringLiteral("550 ns"));
                durationEdit->setModified(true);
                canvas->setFocus(Qt::MouseFocusReason);
                QCoreApplication::processEvents();
                if (window.project().scenarios.front().duration != 550'000) {
                    fail(QStringLiteral("Mouse blur did not submit the End draft"));
                    return;
                }
                const QPoint blurProbePoint(xAtTick(blurProbeTick), bitY);
                click(blurProbePoint);
                bitLane = wave::findLane(window.project().scenarios.front(), bitLaneId);
                if (!bitLane || valueAt(*bitLane, blurProbeTick) != blurProbeBefore) {
                    fail(QStringLiteral("End blur click was reinterpreted after the time scale changed"));
                    return;
                }
                click(blurProbePoint);
                bitLane = wave::findLane(window.project().scenarios.front(), bitLaneId);
                if (!bitLane || valueAt(*bitLane, blurProbeTick) != "1") {
                    fail(QStringLiteral("The click after the End blur guard did not edit normally"));
                    return;
                }
                click(blurProbePoint);

                durationEdit->setText(QStringLiteral("500 ns"));
                sendKey(durationEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                durationEdit->setFocus(Qt::OtherFocusReason);
                durationEdit->setText(QStringLiteral("550 ns"));
                durationEdit->setModified(true);
                canvas->setFocus(Qt::MouseFocusReason);
                QCoreApplication::processEvents();
                bool unexpectedContextMenu = false;
                QTimer::singleShot(0, &application, [&unexpectedContextMenu] {
                    if (auto* popup = QApplication::activePopupWidget()) {
                        unexpectedContextMenu = true;
                        popup->close();
                    }
                });
                QContextMenuEvent blurContext(
                    QContextMenuEvent::Mouse,
                    blurProbePoint,
                    canvas->viewport()->mapToGlobal(blurProbePoint));
                QCoreApplication::sendEvent(canvas->viewport(), &blurContext);
                QCoreApplication::processEvents();
                if (unexpectedContextMenu) {
                    fail(QStringLiteral("End blur right-click opened a menu at remapped coordinates"));
                    return;
                }
                durationEdit->setText(QStringLiteral("500 ns"));
                sendKey(durationEdit, Qt::Key_Return);
                QCoreApplication::processEvents();

                measureAction->trigger();
                QCoreApplication::processEvents();
                const QPoint measureStart(xAtTick(120'000), bitY);
                const QPoint measureEnd(xAtTick(220'000), bitY);
                sendMouse(
                    QEvent::MouseButtonPress,
                    measureStart,
                    Qt::LeftButton,
                    Qt::LeftButton);
                sendMouse(
                    QEvent::MouseMove,
                    measureEnd,
                    Qt::NoButton,
                    Qt::LeftButton);
                sendMouse(
                    QEvent::MouseButtonRelease,
                    measureEnd,
                    Qt::LeftButton,
                    Qt::NoButton);
                QCoreApplication::processEvents();
                if (!canvas->movableCursorTick() || !canvas->temporaryCursorTick()) {
                    fail(QStringLiteral("Measure drag did not show a temporary signed interval"));
                    return;
                }
                canvas->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (measureAction->isChecked()
                    || canvas->tool() != wave::WaveCanvas::Tool::WaveEdit) {
                    fail(QStringLiteral("Measure did not return to direct editing with Escape"));
                    return;
                }

                click(QPoint(xAtTick(470'000), busY));
                if (!palette->isVisible()) {
                    fail(QStringLiteral("Bus conflict draft controls were unavailable"));
                    return;
                }
                busValue->setText(QStringLiteral("0xbeef"));
                busValue->setModified(true);
                durationEdit->setText(QStringLiteral("300 ns"));
                durationEdit->setModified(true);
                durationEdit->setFocus(Qt::OtherFocusReason);
                sendKey(durationEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                sendKey(durationEdit, Qt::Key_S, Qt::ControlModifier);
                QCoreApplication::processEvents();
                busLane = wave::findLane(window.project().scenarios.front(), busLaneId);
                if (QApplication::activeModalWidget()
                    || QFileInfo::exists(userJourneySavePath)
                    || window.project().scenarios.front().duration != 500'000
                    || !busLane
                    || valueAt(*busLane, 470'000) != "0xbeef"
                    || !durationEdit->hasFocus()) {
                    fail(QStringLiteral("Bus draft was not preserved before rejecting a conflicting End"));
                    return;
                }

                durationEdit->setText(QStringLiteral("500 ns"));
                durationEdit->setModified(true);
                sendKey(durationEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                click(QPoint(xAtTick(480'000), busY));
                if (!palette->isVisible()) {
                    fail(QStringLiteral("Bus mouse-conflict draft controls were unavailable"));
                    return;
                }
                busValue->setText(QStringLiteral("0xcafe"));
                busValue->setModified(true);
                durationEdit->setText(QStringLiteral("300 ns"));
                durationEdit->setModified(true);
                durationEdit->setFocus(Qt::OtherFocusReason);
                canvas->setFocus(Qt::MouseFocusReason);
                QCoreApplication::processEvents();
                busLane = wave::findLane(window.project().scenarios.front(), busLaneId);
                if (window.project().scenarios.front().duration != 500'000
                    || !busLane
                    || valueAt(*busLane, 480'000) != "0xcafe"
                    || !durationEdit->isModified()
                    || durationEdit->text() != QStringLiteral("300 ns")
                    || !durationEdit->hasFocus()) {
                    fail(QStringLiteral("Mouse blur did not preserve Bus-before-End draft ordering"));
                    return;
                }

                durationEdit->setText(QStringLiteral("600 ns"));
                durationEdit->setModified(true);
                click(QPoint(xAtTick(270'000), busY));
                QCoreApplication::processEvents();
                if (window.project().scenarios.front().duration != 600'000
                    || palette->isVisible()) {
                    fail(QStringLiteral("Corrected End did not consume only the first canvas click"));
                    return;
                }
                click(QPoint(xAtTick(270'000), busY));
                if (!palette->isVisible()) {
                    fail(QStringLiteral("Bus draft controls were unavailable before focus handoff"));
                    return;
                }
                durationEdit->setText(QStringLiteral("620 ns"));
                durationEdit->setModified(true);
                durationEdit->setFocus(Qt::OtherFocusReason);
                busValue->setFocus(Qt::MouseFocusReason);
                QCoreApplication::processEvents();
                if (window.project().scenarios.front().duration != 620'000
                    || !busValue->hasFocus()) {
                    fail(QStringLiteral("End-to-Bus focus handoff did not preserve the intended target"));
                    return;
                }
                busValue->setText(QStringLiteral("0x55aa"));
                busValue->setModified(true);
                click(QPoint(80, busY));
                QCoreApplication::processEvents();
                busLane = wave::findLane(window.project().scenarios.front(), busLaneId);
                if (palette->isVisible()
                    || !busLane
                    || valueAt(*busLane, 270'000) != "0x55aa") {
                    fail(QStringLiteral("A stale End blur guard swallowed the Bus commit click"));
                    return;
                }
                click(QPoint(xAtTick(270'000), busY));
                if (!palette->isVisible()) {
                    fail(QStringLiteral("Bus draft controls were unavailable before Save"));
                    return;
                }
                busValue->setText(QStringLiteral("0xabcd"));
                busValue->setModified(true);
                durationEdit->setText(QStringLiteral("650 ns"));
                durationEdit->setModified(true);
                durationEdit->setFocus(Qt::OtherFocusReason);
                QCoreApplication::processEvents();

                bool saveDialogHandled = false;
                bool saveDialogUsedWildcardFilter = false;
                bool saveDialogStartedInDefaultDirectory = false;
                QTimer::singleShot(
                    0,
                    &window,
                    [&application,
                     &saveDialogHandled,
                     &saveDialogUsedWildcardFilter,
                     &saveDialogStartedInDefaultDirectory,
                     userJourneySavePath] {
                        auto* dialog = qobject_cast<QFileDialog*>(
                            QApplication::activeModalWidget());
                        if (!dialog) {
                            qCritical().noquote() << "Save As dialog did not open in user journey";
                            application.exit(4);
                            return;
                        }
                        auto defaultDirectory = QStandardPaths::writableLocation(
                            QStandardPaths::DocumentsLocation);
                        if (defaultDirectory.isEmpty()
                            || !QFileInfo(defaultDirectory).isDir()) {
                            defaultDirectory = QDir::homePath();
                        }
                        saveDialogStartedInDefaultDirectory =
                            QString::compare(
                                QDir::cleanPath(dialog->directory().absolutePath()),
                                QDir::cleanPath(defaultDirectory),
                                Qt::CaseInsensitive)
                            == 0;
                        dialog->setDirectory(QFileInfo(userJourneySavePath).absolutePath());
                        auto selectedName = QFileInfo(userJourneySavePath).fileName();
                        if (selectedName.endsWith(
                                QStringLiteral(".wave.json"),
                                Qt::CaseInsensitive)) {
                            selectedName.chop(QStringLiteral(".wave.json").size());
                        }
                        dialog->selectFile(selectedName);
                        saveDialogUsedWildcardFilter =
                            dialog->nameFilters().value(0).contains(
                                QStringLiteral("*.wave.json"));
                        saveDialogHandled = true;
                        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                    });
                sendKey(durationEdit, Qt::Key_S, Qt::ControlModifier);
                QCoreApplication::processEvents();
                auto* recentMenuAfterSave = window.findChild<QMenu*>(
                    QStringLiteral("RecentProjectsMenu"));
                auto* recentActionAfterSave = window.findChild<QAction*>(
                    QStringLiteral("RecentProjectAction1"));
                const auto saved = wave::loadProjectFile(userJourneySavePath);
                const auto* savedRenamedBit = saved.ok()
                    ? wave::findLane(saved.project->scenarios.front(), bitLaneId)
                    : nullptr;
                const auto* savedBus = saved.ok()
                    ? wave::findLane(saved.project->scenarios.front(), busLaneId)
                    : nullptr;
                if (!saveDialogHandled
                    || !saveDialogUsedWildcardFilter
                    || !saveDialogStartedInDefaultDirectory
                    || !recentMenuAfterSave
                    || !recentActionAfterSave
                    || QString::compare(
                        QDir::cleanPath(recentActionAfterSave->data().toString()),
                        QDir::cleanPath(QFileInfo(userJourneySavePath).absoluteFilePath()),
                        Qt::CaseInsensitive)
                        != 0
                    || !recentActionAfterSave->text().contains(
                        QFileInfo(userJourneySavePath).fileName())
                    || !QFileInfo::exists(userJourneySavePath)
                    || QFileInfo::exists(userJourneyBareSavePath)
                    || !saved.ok()
                    || !savedRenamedBit
                    || savedRenamedBit->name != "req_valid"
                    || !savedBus
                    || valueAt(*savedBus, 270'000) != "0xabcd"
                    || saved.project->scenarios.front().lanes.size() != 3
                    || saved.project->scenarios.front().duration != 650'000
                    || saveState->text() != QStringLiteral("Saved")
                    || window.project().name == "Untitled") {
                    fail(QStringLiteral("Save did not produce a valid file and unambiguous Saved state"));
                    return;
                }

                bool openDialogUsedWildcardFilter = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&application, &openDialogUsedWildcardFilter] {
                        auto* dialog = qobject_cast<QFileDialog*>(
                            QApplication::activeModalWidget());
                        if (!dialog) {
                            qCritical().noquote()
                                << "Open dialog did not appear in user journey";
                            application.exit(4);
                            return;
                        }
                        openDialogUsedWildcardFilter =
                            dialog->nameFilters().value(0).contains(
                                QStringLiteral("*.wave.json"));
                        dialog->reject();
                    });
                const auto openInvoked = QMetaObject::invokeMethod(
                    &window,
                    "openProject",
                    Qt::DirectConnection);
                QCoreApplication::processEvents();
                if (!openInvoked
                    || !openDialogUsedWildcardFilter
                    || QApplication::activeModalWidget()
                    || window.project().scenarios.front().duration != 650'000
                    || window.project().scenarios.front().lanes.size() != 3) {
                    fail(QStringLiteral(
                        "Open did not expose *.wave.json files or cancel safely"));
                    return;
                }

                const auto newInvoked = QMetaObject::invokeMethod(
                    &window,
                    "newProject",
                    Qt::DirectConnection);
                QCoreApplication::processEvents();
                if (!newInvoked
                    || !window.project().scenarios.front().lanes.empty()
                    || window.project().scenarios.front().duration != 200'000
                    || window.project().name != "Untitled"
                    || !saveState->text().startsWith(QStringLiteral("Not saved"))) {
                    fail(QStringLiteral(
                        "New did not create a clean blank waveform before reopen"));
                    return;
                }

                bool savedProjectSelectedForOpen = false;
                bool openDialogRememberedDirectory = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&application,
                     &savedProjectSelectedForOpen,
                     &openDialogRememberedDirectory,
                     userJourneySavePath] {
                        auto* dialog = qobject_cast<QFileDialog*>(
                            QApplication::activeModalWidget());
                        if (!dialog) {
                            qCritical().noquote()
                                << "Open dialog did not appear for saved project";
                            application.exit(4);
                            return;
                        }
                        openDialogRememberedDirectory =
                            QString::compare(
                                QDir::cleanPath(dialog->directory().absolutePath()),
                                QDir::cleanPath(
                                    QFileInfo(userJourneySavePath).absolutePath()),
                                Qt::CaseInsensitive)
                            == 0;
                        dialog->setDirectory(
                            QFileInfo(userJourneySavePath).absolutePath());
                        dialog->selectFile(
                            QFileInfo(userJourneySavePath).fileName());
                        savedProjectSelectedForOpen = true;
                        QMetaObject::invokeMethod(
                            dialog,
                            "accept",
                            Qt::DirectConnection);
                    });
                const auto reopenInvoked = QMetaObject::invokeMethod(
                    &window,
                    "openProject",
                    Qt::DirectConnection);
                QCoreApplication::processEvents();
                const auto reopenStatus = window.statusBar()->currentMessage();
                if (!reopenInvoked
                    || !savedProjectSelectedForOpen
                    || !openDialogRememberedDirectory
                    || QApplication::activeModalWidget()
                    || window.project().scenarios.front().duration != 650'000
                    || window.project().scenarios.front().lanes.size() != 3
                    || saveState->text() != QStringLiteral("Saved")
                    || !reopenStatus.startsWith(QStringLiteral("Opened "))
                    || !reopenStatus.contains(
                        QFileInfo(userJourneySavePath).fileName())) {
                    fail(QStringLiteral(
                        "Saved project did not reopen with explicit success feedback"));
                    return;
                }

                durationEdit->setText(QStringLiteral("660 ns"));
                durationEdit->setModified(true);
                sendKey(durationEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                if (window.project().scenarios.front().duration != 660'000
                    || saveState->text() != QStringLiteral("Unsaved changes")) {
                    fail(QStringLiteral(
                        "User journey could not create a dirty project before Open"));
                    return;
                }

                auto* autosaveWatcher = window.findChild<QFutureWatcherBase*>(
                    QStringLiteral("AutosaveWatcher"));
                QEventLoop savepointWait;
                bool autosaveFinished = false;
                if (autosaveWatcher) {
                    QObject::connect(
                        autosaveWatcher,
                        &QFutureWatcherBase::finished,
                        &savepointWait,
                        [&savepointWait, &autosaveFinished] {
                            autosaveFinished = true;
                            savepointWait.quit();
                        });
                }
                auto* savepointUndoAction = window.findChild<QAction*>(
                    QStringLiteral("UndoAction"));
                const auto autosaveStarted = QMetaObject::invokeMethod(
                    &window,
                    "startAutosave",
                    Qt::DirectConnection);
                if (savepointUndoAction) savepointUndoAction->trigger();
                if (autosaveWatcher && autosaveWatcher->isRunning()) {
                    QTimer::singleShot(4'000, &savepointWait, &QEventLoop::quit);
                    savepointWait.exec();
                } else if (autosaveWatcher) {
                    QCoreApplication::processEvents();
                    autosaveFinished = true;
                }
                if (!autosaveWatcher
                    || !savepointUndoAction
                    || !autosaveStarted
                    || !autosaveFinished
                    || autosaveWatcher->isRunning()
                    || window.project().scenarios.front().duration != 650'000
                    || saveState->text() != QStringLiteral("Saved")
                    || window.windowTitle().contains(QStringLiteral(" *"))
                    || QFileInfo::exists(
                        userJourneySavePath + QStringLiteral(".autosave"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("back to saved version"))) {
                    qCritical().noquote()
                        << "Savepoint diagnostic: watcher=" << (autosaveWatcher != nullptr)
                        << "started=" << autosaveStarted
                        << "finished=" << autosaveFinished
                        << "running=" << (autosaveWatcher && autosaveWatcher->isRunning())
                        << "duration=" << window.project().scenarios.front().duration
                        << "state=" << saveState->text()
                        << "title=" << window.windowTitle()
                        << "snapshot=" << QFileInfo::exists(
                            userJourneySavePath + QStringLiteral(".autosave"))
                        << "status=" << window.statusBar()->currentMessage();
                    fail(QStringLiteral(
                        "Undo did not return to Saved or clear the stale recovery snapshot"));
                    return;
                }

                auto* savepointRedoAction = window.findChild<QAction*>(
                    QStringLiteral("RedoAction"));
                if (savepointRedoAction) savepointRedoAction->trigger();
                QCoreApplication::processEvents();
                if (!savepointRedoAction
                    || window.project().scenarios.front().duration != 660'000
                    || saveState->text() != QStringLiteral("Unsaved changes")
                    || !window.windowTitle().contains(QStringLiteral(" *"))
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Redid "))) {
                    fail(QStringLiteral(
                        "Redo did not restore the edit and Unsaved changes state"));
                    return;
                }

                auto* currentRecentAction = window.findChild<QAction*>(
                    QStringLiteral("RecentProjectAction1"));
                if (!currentRecentAction) {
                    fail(QStringLiteral(
                        "The current project disappeared from Open Recent"));
                    return;
                }
                currentRecentAction->trigger();
                QCoreApplication::processEvents();
                if (QApplication::activeModalWidget()
                    || window.project().scenarios.front().duration != 660'000
                    || saveState->text() != QStringLiteral("Unsaved changes")
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Already open:"))) {
                    fail(QStringLiteral(
                        "Open Recent reloaded or prompted for the dirty current project"));
                    return;
                }

                bool dirtyOpenFileDialogSeen = false;
                bool prematureUnsavedPrompt = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&dirtyOpenFileDialogSeen, &prematureUnsavedPrompt] {
                        auto* active = QApplication::activeModalWidget();
                        if (auto* dialog = qobject_cast<QFileDialog*>(active)) {
                            dirtyOpenFileDialogSeen = true;
                            dialog->reject();
                        } else if (auto* warning = qobject_cast<QMessageBox*>(
                                       active)) {
                            prematureUnsavedPrompt = true;
                            warning->reject();
                        } else if (active) {
                            active->close();
                        }
                    });
                const auto dirtyOpenCancelInvoked = QMetaObject::invokeMethod(
                    &window,
                    "openProject",
                    Qt::DirectConnection);
                QCoreApplication::processEvents();
                if (!dirtyOpenCancelInvoked
                    || !dirtyOpenFileDialogSeen
                    || prematureUnsavedPrompt
                    || QApplication::activeModalWidget()
                    || window.project().scenarios.front().duration != 660'000
                    || saveState->text() != QStringLiteral("Unsaved changes")) {
                    fail(QStringLiteral(
                        "Cancelling Open did not bypass discard confirmation safely"));
                    return;
                }

                bool dirtyOpenFileSelected = false;
                bool discardPromptAfterSelection = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&application,
                     &dirtyOpenFileSelected,
                     &discardPromptAfterSelection,
                     userJourneySavePath] {
                        auto* active = QApplication::activeModalWidget();
                        auto* dialog = qobject_cast<QFileDialog*>(active);
                        if (!dialog) {
                            if (auto* warning = qobject_cast<QMessageBox*>(
                                    active)) {
                                warning->reject();
                            } else if (active) {
                                active->close();
                            }
                            return;
                        }
                        dialog->setDirectory(
                            QFileInfo(userJourneySavePath).absolutePath());
                        dialog->selectFile(
                            QFileInfo(userJourneySavePath).fileName());
                        dirtyOpenFileSelected = true;
                        QTimer::singleShot(
                            0,
                            &application,
                            [&discardPromptAfterSelection] {
                                auto* warning = qobject_cast<QMessageBox*>(
                                    QApplication::activeModalWidget());
                                auto* discard = warning
                                    ? warning->button(QMessageBox::Discard)
                                    : nullptr;
                                if (!warning || !discard
                                    || warning->windowTitle()
                                        != QStringLiteral("Unsaved changes")) {
                                    if (warning) warning->reject();
                                    return;
                                }
                                discardPromptAfterSelection = true;
                                discard->click();
                            });
                        QMetaObject::invokeMethod(
                            dialog,
                            "accept",
                            Qt::DirectConnection);
                    });
                const auto dirtyReopenInvoked = QMetaObject::invokeMethod(
                    &window,
                    "openProject",
                    Qt::DirectConnection);
                QCoreApplication::processEvents();
                const auto dirtyReopenStatus =
                    window.statusBar()->currentMessage();
                if (!dirtyReopenInvoked
                    || !dirtyOpenFileSelected
                    || !discardPromptAfterSelection
                    || QApplication::activeModalWidget()
                    || window.project().scenarios.front().duration != 650'000
                    || window.project().scenarios.front().lanes.size() != 3
                    || saveState->text() != QStringLiteral("Saved")
                    || !dirtyReopenStatus.startsWith(QStringLiteral("Opened "))) {
                    fail(QStringLiteral(
                        "Dirty Open did not confirm discard after file selection"));
                    return;
                }

                bool invalidExportRangeRetained = false;
                bool invalidPdfSpanRetained = false;
                bool exportDirectoryReached = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&application,
                     &invalidExportRangeRetained,
                     &invalidPdfSpanRetained,
                     &exportDirectoryReached] {
                        auto* dialog = qobject_cast<QDialog*>(
                            QApplication::activeModalWidget());
                        if (!dialog
                            || dialog->objectName()
                                != QStringLiteral("ExportOptionsDialog")) {
                            if (dialog) dialog->reject();
                            return;
                        }
                        auto* scope = dialog->findChild<QComboBox*>(
                            QStringLiteral("ExportScopeCombo"));
                        auto* start = dialog->findChild<QLineEdit*>(
                            QStringLiteral("ExportStartEdit"));
                        auto* end = dialog->findChild<QLineEdit*>(
                            QStringLiteral("ExportEndEdit"));
                        auto* width = dialog->findChild<QSpinBox*>(
                            QStringLiteral("ExportLogicalWidthSpin"));
                        auto* dpi = dialog->findChild<QSpinBox*>(
                            QStringLiteral("ExportPngDpiSpin"));
                        auto* pdfSpan = dialog->findChild<QLineEdit*>(
                            QStringLiteral("ExportPdfSpanEdit"));
                        auto* relations = dialog->findChild<QCheckBox*>(
                            QStringLiteral("ExportRelationsCheck"));
                        auto* markers = dialog->findChild<QCheckBox*>(
                            QStringLiteral("ExportMarkersCheck"));
                        auto* annotations = dialog->findChild<QCheckBox*>(
                            QStringLiteral("ExportAnnotationsCheck"));
                        auto* buttons = dialog->findChild<QDialogButtonBox*>();
                        auto* ok = buttons
                            ? buttons->button(QDialogButtonBox::Ok)
                            : nullptr;
                        if (!scope || !start || !end || !width || !dpi
                            || !pdfSpan || !relations || !markers
                            || !annotations || !ok) {
                            dialog->reject();
                            return;
                        }
                        scope->setCurrentIndex(
                            scope->findData(QStringLiteral("range")));
                        start->setText(QStringLiteral("80 ns"));
                        end->setText(QStringLiteral("40 ns"));
                        width->setValue(2048);
                        dpi->setValue(144);
                        pdfSpan->setText(QStringLiteral("25 ns"));
                        relations->setChecked(false);
                        markers->setChecked(true);
                        annotations->setChecked(false);

                        QTimer::singleShot(
                            0,
                            &application,
                            [&application,
                             dialog,
                             scope,
                             start,
                             end,
                             width,
                             dpi,
                             pdfSpan,
                             relations,
                             markers,
                             annotations,
                             ok,
                             &invalidExportRangeRetained,
                             &invalidPdfSpanRetained,
                             &exportDirectoryReached] {
                                auto* active = QApplication::activeModalWidget();
                                if (auto* warning = qobject_cast<QMessageBox*>(
                                        active)) {
                                    warning->accept();
                                    return;
                                }
                                auto* error = dialog->findChild<QLabel*>(
                                    QStringLiteral("ExportOptionsError"));
                                if (active != dialog
                                    || !error
                                    || !error->isVisible()
                                    || !error->text().contains(
                                        QStringLiteral("greater than start"))
                                    || start->text() != QStringLiteral("80 ns")
                                    || end->text() != QStringLiteral("40 ns")
                                    || !end->hasFocus()
                                    || width->value() != 2048
                                    || dpi->value() != 144
                                    || relations->isChecked()
                                    || !markers->isChecked()
                                    || annotations->isChecked()) {
                                    dialog->reject();
                                    return;
                                }
                                invalidExportRangeRetained = true;
                                end->setText(QStringLiteral("120 ns"));
                                pdfSpan->setText(QStringLiteral("-1 tick"));

                                QTimer::singleShot(
                                    0,
                                    &application,
                                    [&application,
                                     dialog,
                                     scope,
                                     start,
                                     end,
                                     width,
                                     dpi,
                                     pdfSpan,
                                     relations,
                                     markers,
                                     annotations,
                                     ok,
                                     &invalidPdfSpanRetained,
                                     &exportDirectoryReached] {
                                        auto* active = QApplication::activeModalWidget();
                                        if (auto* warning = qobject_cast<QMessageBox*>(
                                                active)) {
                                            warning->accept();
                                            return;
                                        }
                                        auto* error = dialog->findChild<QLabel*>(
                                            QStringLiteral("ExportOptionsError"));
                                        if (active != dialog
                                            || !error
                                            || !error->isVisible()
                                            || !error->text().contains(
                                                QStringLiteral("non-negative"))
                                            || scope->currentData().toString()
                                                != QStringLiteral("range")
                                            || start->text() != QStringLiteral("80 ns")
                                            || end->text() != QStringLiteral("120 ns")
                                            || pdfSpan->text() != QStringLiteral("-1 tick")
                                            || !pdfSpan->hasFocus()
                                            || width->value() != 2048
                                            || dpi->value() != 144
                                            || relations->isChecked()
                                            || !markers->isChecked()
                                            || annotations->isChecked()) {
                                            dialog->reject();
                                            return;
                                        }
                                        invalidPdfSpanRetained = true;
                                        pdfSpan->setText(QStringLiteral("25 ns"));
                                        QTimer::singleShot(
                                            0,
                                            &application,
                                            [&exportDirectoryReached] {
                                                auto* active =
                                                    QApplication::activeModalWidget();
                                                if (auto* fileDialog =
                                                        qobject_cast<QFileDialog*>(
                                                            active)) {
                                                    exportDirectoryReached = true;
                                                    fileDialog->reject();
                                                } else if (auto* warning =
                                                               qobject_cast<QMessageBox*>(
                                                                   active)) {
                                                    warning->accept();
                                                } else if (active) {
                                                    active->close();
                                                }
                                            });
                                        ok->click();
                                    });
                                ok->click();
                            });
                        ok->click();
                    });
                const auto exportInvoked = QMetaObject::invokeMethod(
                    &window,
                    "exportArtifacts",
                    Qt::DirectConnection);
                QCoreApplication::processEvents();
                if (!exportInvoked
                    || !invalidExportRangeRetained
                    || !invalidPdfSpanRetained
                    || !exportDirectoryReached
                    || QApplication::activeModalWidget()) {
                    fail(QStringLiteral(
                        "Export options did not retain invalid range/PDF drafts in place"));
                    return;
                }

                auto* newActionForRecent = window.findChild<QAction*>(
                    QStringLiteral("NewProjectAction"));
                if (!newActionForRecent) {
                    fail(QStringLiteral(
                        "New action was unavailable before recent-project reopen"));
                    return;
                }
                newActionForRecent->trigger();
                QCoreApplication::processEvents();
                auto* recentAction = window.findChild<QAction*>(
                    QStringLiteral("RecentProjectAction1"));
                if (!recentAction
                    || !window.project().scenarios.front().lanes.empty()
                    || window.project().scenarios.front().duration != 200'000
                    || saveState->text() != QStringLiteral("Not saved")) {
                    fail(QStringLiteral(
                        "A clean New did not retain the recent-project shortcut"));
                    return;
                }
                recentAction->trigger();
                QCoreApplication::processEvents();
                const auto recentOpenStatus =
                    window.statusBar()->currentMessage();
                if (QApplication::activeModalWidget()
                    || window.project().scenarios.front().duration != 650'000
                    || window.project().scenarios.front().lanes.size() != 3
                    || saveState->text() != QStringLiteral("Saved")
                    || !recentOpenStatus.startsWith(QStringLiteral("Opened "))
                    || !recentOpenStatus.contains(
                        QFileInfo(userJourneySavePath).fileName())) {
                    fail(QStringLiteral(
                        "Open Recent did not restore the last project in one step"));
                    return;
                }

                auto droppedProject = window.project();
                droppedProject.name = "Dropped waveform";
                droppedProject.scenarios.front().duration = 700'000;
                QString droppedWriteError;
                if (!wave::saveProjectFileAtomic(
                        droppedProject,
                        userJourneyDropPath,
                        &droppedWriteError)) {
                    fail(QStringLiteral("Cannot create dropped project: %1")
                             .arg(droppedWriteError));
                    return;
                }
                QMimeData projectMime;
                projectMime.setUrls({QUrl::fromLocalFile(userJourneyDropPath)});
                const auto dropPosition = canvas->viewport()->rect().center();
                QDragEnterEvent projectDragEnter(
                    dropPosition,
                    Qt::CopyAction,
                    &projectMime,
                    Qt::LeftButton,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(
                    canvas->viewport(),
                    &projectDragEnter);
                QDropEvent projectDrop(
                    QPointF(dropPosition),
                    Qt::CopyAction,
                    &projectMime,
                    Qt::LeftButton,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(canvas->viewport(), &projectDrop);
                QCoreApplication::processEvents();
                QCoreApplication::processEvents();
                auto* droppedRecentAction = window.findChild<QAction*>(
                    QStringLiteral("RecentProjectAction1"));
                const auto droppedOpenStatus =
                    window.statusBar()->currentMessage();
                if (!projectDragEnter.isAccepted()
                    || !projectDrop.isAccepted()
                    || QApplication::activeModalWidget()
                    || window.project().name != "Dropped waveform"
                    || window.project().scenarios.front().duration != 700'000
                    || window.project().scenarios.front().lanes.size() != 3
                    || saveState->text() != QStringLiteral("Saved")
                    || !droppedOpenStatus.startsWith(QStringLiteral("Opened "))
                    || !droppedOpenStatus.contains(
                        QFileInfo(userJourneyDropPath).fileName())
                    || !droppedRecentAction
                    || QString::compare(
                        QDir::cleanPath(droppedRecentAction->data().toString()),
                        QDir::cleanPath(
                            QFileInfo(userJourneyDropPath).absoluteFilePath()),
                        Qt::CaseInsensitive)
                        != 0) {
                    qCritical().noquote()
                        << "drop diagnostics"
                        << projectDragEnter.isAccepted()
                        << projectDrop.isAccepted()
                        << window.project().name.c_str()
                        << window.project().scenarios.front().duration
                        << window.project().scenarios.front().lanes.size()
                        << saveState->text()
                        << droppedOpenStatus
                        << (droppedRecentAction
                                ? droppedRecentAction->data().toString()
                                : QStringLiteral("<missing recent>"));
                    fail(QStringLiteral(
                        "Dropping one project file did not open and remember it"));
                    return;
                }

                const auto screenshot = QFileInfo(userJourneySavePath)
                                            .absolutePath()
                    + QStringLiteral("/user-journey-smoke.png");
                if (!window.grab().save(screenshot)) {
                    fail(QStringLiteral("User journey screenshot could not be saved"));
                    return;
                }
                window.hide();
                application.exit(0);
            });
    } else if (laneRemovalSmoke) {
        QTimer::singleShot(0, &window, [&application, &window] {
            window.revealLocation(QStringLiteral("lane-request"), 80'000);
            bool confirmationDescribedDependencies = false;
            QTimer::singleShot(
                0,
                &application,
                [&application, &confirmationDescribedDependencies] {
                    auto* confirmation = qobject_cast<QMessageBox*>(
                        QApplication::activeModalWidget());
                    if (!confirmation) {
                        qCritical().noquote() << "Lane removal confirmation did not open";
                        application.exit(4);
                        return;
                    }
                    auto* yes = confirmation->button(QMessageBox::Yes);
                    if (!yes) {
                        qCritical().noquote() << "Lane removal confirmation has no Yes button";
                        confirmation->reject();
                        application.exit(4);
                        return;
                    }
                    const auto text = confirmation->text();
                    confirmationDescribedDependencies =
                        text.contains(QStringLiteral("3 events"))
                        && text.contains(QStringLiteral("1 relation"))
                        && text.contains(QStringLiteral("Ctrl+Z"));
                    if (!confirmationDescribedDependencies) {
                        qCritical().noquote()
                            << "Lane removal confirmation did not describe impact and recovery:"
                            << text;
                        confirmation->reject();
                        application.exit(4);
                        return;
                    }
                    yes->click();
                });
            if (!QMetaObject::invokeMethod(
                    &window,
                    "removeSelectedLane",
                    Qt::DirectConnection)) {
                qCritical().noquote() << "Cannot trigger lane removal smoke";
                window.hide();
                application.exit(4);
                return;
            }
            const auto& removedProject = window.project();
            const auto& removedScenario = removedProject.scenarios.front();
            const auto relationRemoved = std::none_of(
                removedScenario.relations.begin(),
                removedScenario.relations.end(),
                [](const wave::Relation& relation) {
                    return relation.id == "relation-req-ack";
                });
            const auto removalStatus = window.statusBar()->currentMessage();
            if (wave::findLane(removedScenario, "lane-request")
                || !relationRemoved
                || !confirmationDescribedDependencies
                || !removalStatus.contains(QStringLiteral("Removed req"))
                || !removalStatus.contains(QStringLiteral("3 events removed"))
                || !removalStatus.contains(QStringLiteral("1 relation removed"))
                || !removalStatus.contains(QStringLiteral("Ctrl+Z"))) {
                QStringList remainingLanes;
                for (const auto& lane : removedScenario.lanes) {
                    remainingLanes.append(QString::fromStdString(lane.id));
                }
                QStringList remainingRelations;
                for (const auto& relation : removedScenario.relations) {
                    remainingRelations.append(QString::fromStdString(relation.id));
                }
                qCritical().noquote()
                    << "Lane removal did not clean dependent data or report the result; lanes:"
                    << remainingLanes.join(QLatin1Char(','))
                    << "relations:"
                    << remainingRelations.join(QLatin1Char(','))
                    << "status:" << removalStatus;
                window.hide();
                application.exit(4);
                return;
            }
            if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)) {
                qCritical().noquote() << "Cannot undo lane removal smoke";
                window.hide();
                application.exit(4);
                return;
            }
            const auto& restoredScenario = window.project().scenarios.front();
            const auto relationRestored = std::any_of(
                restoredScenario.relations.begin(),
                restoredScenario.relations.end(),
                [](const wave::Relation& relation) {
                    return relation.id == "relation-req-ack";
                });
            const auto undoStatus = window.statusBar()->currentMessage();
            if (!wave::findLane(restoredScenario, "lane-request")
                || !relationRestored
                || !undoStatus.contains(QStringLiteral("Undid Remove lane"))
                || !undoStatus.contains(QStringLiteral("Ctrl+Y"))) {
                qCritical().noquote()
                    << "Lane removal undo did not restore dependent data or report recovery"
                    << undoStatus;
                window.hide();
                application.exit(4);
                return;
            }
            window.hide();
            application.exit(0);
        });
    } else if (waveEditAutoScrollSmoke) {
        QTimer::singleShot(
            0,
            &window,
            [&application, &window, waveEditAutoScrollScreenshotPath] {
                auto* canvas = window.findChild<wave::WaveCanvas*>();
                auto* undoAction = window.findChild<QAction*>(
                    QStringLiteral("UndoAction"));
                auto* fitAction = window.findChild<QAction*>(
                    QStringLiteral("FitScenarioAction"));
                auto* selectFullRangeAction = window.findChild<QAction*>(
                    QStringLiteral("SelectFullRangeAction"));
                auto* durationEdit = window.findChild<QLineEdit*>(
                    QStringLiteral("TimelineDurationEdit"));
                auto* saveState = window.findChild<QLabel*>(
                    QStringLiteral("SaveStateLabel"));
                auto* goToTimeAction = window.findChild<QAction*>(
                    QStringLiteral("GoToTimeAction"));
                auto* goToTimeToolbarAction = window.findChild<QAction*>(
                    QStringLiteral("GoToTimeToolbarAction"));
                auto* goToTimeBar = window.findChild<QFrame*>(
                    QStringLiteral("GoToTimeBar"));
                auto* goToTimeEdit = window.findChild<QLineEdit*>(
                    QStringLiteral("GoToTimeEdit"));
                auto* goToTimeRange = window.findChild<QLabel*>(
                    QStringLiteral("GoToTimeRangeLabel"));
                auto* goToTimeGo = window.findChild<QToolButton*>(
                    QStringLiteral("GoToTimeGoButton"));
                auto* goToTimeClose = window.findChild<QToolButton*>(
                    QStringLiteral("GoToTimeCloseButton"));
                auto* signalFindToolbarAction = window.findChild<QAction*>(
                    QStringLiteral("SignalFindToolbarAction"));
                auto* goToBusPalette = window.findChild<QWidget*>(QStringLiteral("BusPresetPalette"));
                auto fail = [&application, &window](const QString& message) {
                    qCritical().noquote() << message;
                    window.hide();
                    application.exit(4);
                };
                if (!canvas || !undoAction || !fitAction || !selectFullRangeAction
                    || !durationEdit || !saveState || !goToTimeAction
                    || !goToTimeToolbarAction || !goToTimeBar
                    || !goToTimeEdit || !goToTimeRange || !goToTimeGo
                    || !goToTimeClose || !signalFindToolbarAction || !goToBusPalette
                    || window.project().scenarios.empty()) {
                    fail(QStringLiteral(
                        "Wave Edit autoscroll smoke prerequisites are missing"));
                    return;
                }

                auto& scenario = window.project().scenarios.front();
                if (scenario.lanes.size() != 4
                    || scenario.lanes.front().id != "lane-wave-edit-scroll"
                    || scenario.lanes.front().segments.size() != 1
                    || scenario.lanes.at(1).id != "lane-wave-edit-enum"
                    || scenario.lanes.at(1).segments.size() != 3
                    || scenario.lanes.at(2).id != "lane-wave-edit-enum-next"
                    || scenario.lanes.at(2).segments.size() != 1
                    || scenario.lanes.back().id != "lane-wave-edit-clock"
                    || window.project().clockDomains.size() != 1
                    || saveState->text() != QStringLiteral("Saved")
                    || undoAction->isEnabled()
                    || selectFullRangeAction->shortcut()
                        != QKeySequence::SelectAll
                    || fitAction->text() != QStringLiteral("Fit scenario")
                    || !fitAction->toolTip().contains(
                        QStringLiteral("complete scenario"))
                    || goToTimeAction->shortcut().matches(
                           QKeySequence(Qt::CTRL | Qt::Key_G))
                        != QKeySequence::ExactMatch
                    || goToTimeToolbarAction->isVisible()
                    || goToTimeBar->isVisibleTo(&window)
                    || goToTimeEdit->placeholderText()
                        != QStringLiteral("125 ns or cycle 25")
                    || goToTimeEdit->accessibleName()
                        != QStringLiteral("Exact timeline position")
                    || goToTimeRange->text() != QStringLiteral("0 ps–0 ps")
                    || signalFindToolbarAction->isVisible()
                    || QApplication::activeModalWidget()) {
                    fail(QStringLiteral(
                        "Wave Edit autoscroll smoke did not start from its Saved fixture"));
                    return;
                }
                const auto originalScenario = scenario;
                const auto originalRange = std::pair<wave::Tick, wave::Tick>{
                    50'000,
                    100'000,
                };

                const auto sendMouse = [canvas](
                                           const QEvent::Type type,
                                           const QPoint position,
                                           const Qt::MouseButton button,
                                           const Qt::MouseButtons buttons,
                                           const Qt::KeyboardModifiers modifiers) {
                    QMouseEvent event(
                        type,
                        QPointF(position),
                        QPointF(canvas->viewport()->mapToGlobal(position)),
                        button,
                        buttons,
                        modifiers);
                    QCoreApplication::sendEvent(canvas->viewport(), &event);
                };
                const auto sendKey = [](
                                         QObject* target,
                                         const int key,
                                         const Qt::KeyboardModifiers modifiers
                                             = Qt::NoModifier) {
                    QKeyEvent press(QEvent::KeyPress, key, modifiers);
                    QCoreApplication::sendEvent(target, &press);
                    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
                    QCoreApplication::sendEvent(target, &release);
                };
                const auto waitForScroll = [](const int milliseconds) {
                    QEventLoop loop;
                    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
                    loop.exec();
                    QCoreApplication::processEvents();
                    QCoreApplication::processEvents();
                };
                const auto waveWidth = [canvas] {
                    return std::max(
                        1,
                        canvas->viewport()->width()
                            - canvas->signalHeaderWidth());
                };
                const auto contentWidth = [canvas, &waveWidth] {
                    return static_cast<double>(waveWidth())
                        + canvas->horizontalScrollBar()->maximum();
                };
                const auto xAtTick = [canvas, &scenario, &contentWidth](
                                         const wave::Tick tick) {
                    return canvas->signalHeaderWidth()
                        + static_cast<int>(std::llround(
                            static_cast<double>(tick) * contentWidth()
                            / static_cast<double>(scenario.duration)))
                        - canvas->horizontalScrollBar()->value();
                };
                const auto tickAtX = [canvas, &scenario, &contentWidth](
                                         const int x) {
                    const auto contentX = canvas->horizontalScrollBar()->value()
                        + x - canvas->signalHeaderWidth();
                    return std::clamp<wave::Tick>(
                        static_cast<wave::Tick>(std::llround(
                            static_cast<double>(contentX)
                            * static_cast<double>(scenario.duration)
                            / contentWidth())),
                        0,
                        scenario.duration);
                };
                const auto segmentById = [&scenario]() -> const wave::Segment* {
                    const auto& segments = scenario.lanes.front().segments;
                    const auto segment = std::find_if(
                        segments.begin(),
                        segments.end(),
                        [](const wave::Segment& candidate) {
                            return candidate.id == "segment-wave-edit-scroll";
                        });
                    return segment == segments.end() ? nullptr : &*segment;
                };

                canvas->fitScenario();
                for (auto index = 0; index < 8; ++index) canvas->zoomIn();
                canvas->horizontalScrollBar()->setValue(0);
                canvas->setFocus(Qt::OtherFocusReason);
                QCoreApplication::processEvents();
                const auto laneY = 40 + scenario.lanes.front().height / 2;
                const auto rightEdge = canvas->viewport()->width() - 3;
                const auto leftEdge = canvas->signalHeaderWidth() + 3;
                if (canvas->horizontalScrollBar()->maximum()
                        <= waveWidth() * 3
                    || !canvas->viewport()->rect().contains(
                        QPoint(xAtTick(75'000), laneY))) {
                    fail(QStringLiteral(
                        "Long timeline did not create a stable horizontal viewport"));
                    return;
                }

                const auto beginRightEdgeSegmentDrag = [&] {
                    const QPoint start(xAtTick(75'000), laneY);
                    sendMouse(
                        QEvent::MouseButtonPress,
                        start,
                        Qt::LeftButton,
                        Qt::LeftButton,
                        Qt::NoModifier);
                    sendMouse(
                        QEvent::MouseMove,
                        QPoint(rightEdge, laneY),
                        Qt::NoButton,
                        Qt::LeftButton,
                        Qt::NoModifier);
                };
                const auto releaseLeftButton = [&](
                                                   const QPoint position,
                                                   const Qt::KeyboardModifiers modifiers) {
                    sendMouse(
                        QEvent::MouseButtonRelease,
                        position,
                        Qt::LeftButton,
                        Qt::NoButton,
                        modifiers);
                    QCoreApplication::processEvents();
                };

                beginRightEdgeSegmentDrag();
                waitForScroll(360);
                const auto cancelPreview = canvas->selectedTimeRange();
                if (canvas->horizontalScrollBar()->value() <= 0
                    || !cancelPreview
                    || cancelPreview->first <= originalRange.first
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Auto-scroll right"))) {
                    fail(QStringLiteral(
                        "Holding a Segment at the right edge did not scroll a model-free preview"));
                    return;
                }
                sendKey(canvas, Qt::Key_Escape);
                waitForScroll(120);
                releaseLeftButton(QPoint(rightEdge, laneY), Qt::NoModifier);
                if (canvas->horizontalScrollBar()->value() != 0
                    || canvas->selectedTimeRange()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || canvas->viewport()->cursor().shape()
                        != Qt::PointingHandCursor
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral(
                            "Waveform drag cancelled · view restored"))) {
                    fail(QStringLiteral(
                        "Escape did not cancel the Segment preview and restore its viewport"));
                    return;
                }

                beginRightEdgeSegmentDrag();
                waitForScroll(360);
                const auto committedPreview = canvas->selectedTimeRange();
                if (!committedPreview
                    || committedPreview->first <= originalRange.first
                    || scenario != originalScenario) {
                    fail(QStringLiteral(
                        "Second right-edge drag did not produce a stable Segment preview"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()
                    && !window.grab().save(
                        waveEditAutoScrollScreenshotPath)) {
                    fail(QStringLiteral(
                        "Cannot save Wave Edit autoscroll smoke screenshot"));
                    return;
                }
                releaseLeftButton(QPoint(rightEdge, laneY), Qt::NoModifier);
                const auto* movedSegment = segmentById();
                if (!movedSegment
                    || movedSegment->start != committedPreview->first
                    || movedSegment->end != committedPreview->second
                    || canvas->horizontalScrollBar()->value() <= 0
                    || !undoAction->isEnabled()
                    || saveState->text()
                        != QStringLiteral("Unsaved changes")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("data[7:0] segment moved"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    fail(QStringLiteral(
                        "Right-edge Segment release did not commit one visible move"));
                    return;
                }
                const auto stoppedAfterCommit =
                    canvas->horizontalScrollBar()->value();
                waitForScroll(140);
                if (canvas->horizontalScrollBar()->value()
                    != stoppedAfterCommit) {
                    fail(QStringLiteral(
                        "Wave Edit edge scrolling continued after Segment release"));
                    return;
                }
                undoAction->trigger();
                QCoreApplication::processEvents();
                if (scenario != originalScenario
                    || saveState->text() != QStringLiteral("Saved")
                    || window.windowTitle().contains(QStringLiteral(" *"))) {
                    fail(QStringLiteral(
                        "Undo did not restore the exact pre-drag Segment and Saved state"));
                    return;
                }

                canvas->horizontalScrollBar()->setValue(
                    canvas->horizontalScrollBar()->maximum() / 2);
                QCoreApplication::processEvents();
                const auto rangeScrollStart =
                    canvas->horizontalScrollBar()->value();
                const auto rangeStartX = canvas->signalHeaderWidth()
                    + waveWidth() * 2 / 3;
                const auto rangeStartTick = tickAtX(rangeStartX);
                sendMouse(
                    QEvent::MouseButtonPress,
                    QPoint(rangeStartX, laneY),
                    Qt::LeftButton,
                    Qt::LeftButton,
                    Qt::ShiftModifier);
                sendMouse(
                    QEvent::MouseMove,
                    QPoint(leftEdge, laneY),
                    Qt::NoButton,
                    Qt::LeftButton,
                    Qt::ShiftModifier);
                waitForScroll(300);
                const auto leftPreview = canvas->selectedTimeRange();
                if (canvas->horizontalScrollBar()->value()
                        >= rangeScrollStart
                    || !leftPreview
                    || leftPreview->first >= rangeStartTick
                    || leftPreview->second < rangeStartTick
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Auto-scroll left"))) {
                    fail(QStringLiteral(
                        "Shift range selection did not continuously expand across the left edge"));
                    return;
                }
                releaseLeftButton(
                    QPoint(leftEdge, laneY),
                    Qt::ShiftModifier);
                const auto committedRange = canvas->selectedTimeRange();
                if (!canvas->hasExplicitRangeSelection()
                    || !committedRange
                    || committedRange->first >= rangeStartTick
                    || committedRange->second < rangeStartTick
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || fitAction->text() != QStringLiteral("Fit selection")
                    || !fitAction->toolTip().contains(
                        QStringLiteral("selected time range"))
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Selected "))) {
                    fail(QStringLiteral(
                        "Left-edge Shift release did not preserve an actionable range"));
                    return;
                }
                const auto stoppedAfterRange =
                    canvas->horizontalScrollBar()->value();
                waitForScroll(140);
                if (canvas->horizontalScrollBar()->value()
                    != stoppedAfterRange) {
                    fail(QStringLiteral(
                        "Wave Edit edge scrolling continued after range release"));
                    return;
                }

                fitAction->trigger();
                QCoreApplication::processEvents();
                const auto fittedRange = canvas->selectedTimeRange();
                const auto fittedContentWidth = static_cast<double>(waveWidth())
                    + canvas->horizontalScrollBar()->maximum();
                const auto fittedXAtTick = [
                                              canvas,
                                              &scenario,
                                              fittedContentWidth](
                                              const wave::Tick tick) {
                    return canvas->signalHeaderWidth()
                        + static_cast<int>(std::llround(
                            static_cast<double>(tick) * fittedContentWidth
                            / static_cast<double>(scenario.duration)))
                        - canvas->horizontalScrollBar()->value();
                };
                const auto fittedStartX = fittedRange
                    ? fittedXAtTick(fittedRange->first)
                    : -1;
                const auto fittedEndX = fittedRange
                    ? fittedXAtTick(fittedRange->second)
                    : -1;
                if (!canvas->hasExplicitRangeSelection()
                    || fittedRange != committedRange
                    || fittedStartX < canvas->signalHeaderWidth() - 2
                    || fittedStartX > canvas->signalHeaderWidth() + 2
                    || fittedEndX < canvas->viewport()->width() - 2
                    || fittedEndX > canvas->viewport()->width() + 2
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || fitAction->text() != QStringLiteral("Fit selection")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Fitted selected range"))) {
                    fail(QStringLiteral(
                        "Contextual Fit did not fill the viewport with the selected range"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto fitScreenshotPath = waveEditAutoScrollScreenshotPath;
                    const auto suffix = fitScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        fitScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-fit-selection"));
                    } else {
                        fitScreenshotPath.append(
                            QStringLiteral("-fit-selection.png"));
                    }
                    if (!window.grab().save(fitScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save contextual Fit selection screenshot"));
                        return;
                    }
                }

                const auto rangeSignalIds = canvas->selectedLaneIds();
                const auto rangeVerticalScroll =
                    canvas->verticalScrollBar()->value();
                sendKey(canvas, Qt::Key_Down);
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != committedRange
                    || canvas->selectedLaneIds() != rangeSignalIds
                    || canvas->verticalScrollBar()->value()
                        != rangeVerticalScroll
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || fitAction->text() != QStringLiteral("Fit selection")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral(
                            "Esc clears the selected range before changing signals"))) {
                    fail(QStringLiteral(
                        "Signal Down discarded or changed an explicit range"));
                    return;
                }

                sendKey(canvas, Qt::Key_Escape);
                if (canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || fitAction->text() != QStringLiteral("Fit scenario")
                    || !fitAction->toolTip().contains(
                        QStringLiteral("complete scenario"))) {
                    fail(QStringLiteral(
                        "Range cleanup did not restore the Fit scenario action"));
                    return;
                }
                fitAction->trigger();
                QCoreApplication::processEvents();
                if (canvas->horizontalScrollBar()->value() != 0
                    || canvas->horizontalScrollBar()->maximum() != 0
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Fitted complete scenario"))) {
                    fail(QStringLiteral(
                        "Contextual Fit scenario did not restore the complete overview"));
                    return;
                }

                for (auto index = 0; index < 5; ++index) canvas->zoomIn();
                QCoreApplication::processEvents();
                const auto navigationMaximum =
                    canvas->horizontalScrollBar()->maximum();
                canvas->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_End);
                if (navigationMaximum <= 0
                    || canvas->cursorTick() != scenario.duration
                    || canvas->horizontalScrollBar()->value()
                        != navigationMaximum
                    || canvas->horizontalScrollBar()->maximum()
                        != navigationMaximum
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Timeline end"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Home jumps to"))) {
                    fail(QStringLiteral(
                        "End did not jump the edit cursor and viewport to the timeline boundary"));
                    return;
                }

                durationEdit->setFocus(Qt::OtherFocusReason);
                durationEdit->setCursorPosition(durationEdit->text().size());
                sendKey(durationEdit, Qt::Key_Home);
                QCoreApplication::processEvents();
                if (durationEdit->cursorPosition() != 0
                    || canvas->cursorTick() != scenario.duration
                    || canvas->horizontalScrollBar()->value()
                        != navigationMaximum
                    || durationEdit->isModified()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Timeline Home intercepted the End text field instead of moving its caret"));
                    return;
                }

                canvas->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Home, Qt::ControlModifier);
                QCoreApplication::processEvents();
                if (canvas->cursorTick() != 0
                    || canvas->horizontalScrollBar()->value() != 0
                    || canvas->horizontalScrollBar()->maximum()
                        != navigationMaximum
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Timeline start"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("End jumps to"))) {
                    fail(QStringLiteral(
                        "Ctrl+Home did not preserve zoom while returning to timeline start"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto navigationScreenshotPath =
                        waveEditAutoScrollScreenshotPath;
                    const auto suffix = navigationScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        navigationScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-timeline-home"));
                    } else {
                        navigationScreenshotPath.append(
                            QStringLiteral("-timeline-home.png"));
                    }
                    if (!window.grab().save(navigationScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save timeline Home navigation screenshot"));
                        return;
                    }
                }

                const auto clickSignalHeader = [&](const int y) {
                    const QPoint position(80, y);
                    sendMouse(
                        QEvent::MouseButtonPress,
                        position,
                        Qt::LeftButton,
                        Qt::LeftButton,
                        Qt::NoModifier);
                    sendMouse(
                        QEvent::MouseButtonRelease,
                        position,
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
                    QCoreApplication::processEvents();
                };
                clickSignalHeader(laneY);
                if (!window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Left/Right jumps edges"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Shift+Left/Right selects time"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Enter edits value"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("value X"))
                    || !goToBusPalette->isVisibleTo(&window)) {
                    fail(QStringLiteral(
                        "Signal selection did not disclose adjacent edge navigation"));
                    return;
                }
                sendKey(canvas, Qt::Key_G, Qt::ControlModifier);
                QCoreApplication::processEvents();
                if (!goToTimeToolbarAction->isVisible()
                    || !goToTimeBar->isVisibleTo(&window)
                    || !goToTimeEdit->hasFocus()
                    || goToTimeEdit->text() != QStringLiteral("0 ps")
                    || goToTimeEdit->selectedText() != QStringLiteral("0 ps")
                    || goToTimeRange->text() != QStringLiteral("0 ps–1 us")
                    || signalFindToolbarAction->isVisible()
                    || canvas->cursorTick() != 0
                    || goToBusPalette->isVisibleTo(&window)
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || canvas->horizontalScrollBar()->value() != 0
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || QApplication::activeModalWidget()) {
                    fail(QStringLiteral(
                        "Ctrl+G did not open a focused non-modal exact-time bar at the current cursor"));
                    return;
                }

                goToTimeEdit->setText(QStringLiteral("not-a-time"));
                sendKey(goToTimeEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                if (!goToTimeToolbarAction->isVisible()
                    || !goToTimeEdit->hasFocus()
                    || goToTimeEdit->text() != QStringLiteral("not-a-time")
                    || goToTimeEdit->styleSheet().isEmpty()
                    || canvas->cursorTick() != 0
                    || canvas->horizontalScrollBar()->value() != 0
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Cannot go to"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Use an integer"))
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Invalid time input moved the cursor or failed to preserve an inline correction"));
                    return;
                }

                goToTimeEdit->setText(QStringLiteral("1200 ns"));
                sendKey(goToTimeEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                if (goToTimeEdit->styleSheet().isEmpty()
                    || canvas->cursorTick() != 0
                    || canvas->horizontalScrollBar()->value() != 0
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("from 0 ps to 1 us"))
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Out-of-range time did not stay in place with the valid timeline bounds"));
                    return;
                }

                goToTimeEdit->setText(QStringLiteral("375 ns"));
                goToTimeGo->click();
                QCoreApplication::processEvents();
                const auto absoluteTimeScroll =
                    canvas->horizontalScrollBar()->value();
                if (!goToTimeToolbarAction->isVisible()
                    || !goToTimeEdit->hasFocus()
                    || goToTimeEdit->text() != QStringLiteral("375 ns")
                    || goToTimeEdit->selectedText() != QStringLiteral("375 ns")
                    || !goToTimeEdit->styleSheet().isEmpty()
                    || goToTimeRange->text() != QStringLiteral("0 ps–1 us")
                    || canvas->cursorTick() != 375'000
                    || absoluteTimeScroll <= 0
                    || absoluteTimeScroll >= navigationMaximum
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Edit cursor moved to 375 ns"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("data[7:0] remains selected"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Enter jumps again"))
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || QApplication::activeModalWidget()) {
                    fail(QStringLiteral(
                        "Exact absolute time did not move and reveal the edit cursor without changing its signal"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto goToTimeScreenshotPath =
                        waveEditAutoScrollScreenshotPath;
                    const auto suffix = goToTimeScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        goToTimeScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-go-to-time"));
                    } else {
                        goToTimeScreenshotPath.append(
                            QStringLiteral("-go-to-time.png"));
                    }
                    if (!window.grab().save(goToTimeScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save exact time navigation screenshot"));
                        return;
                    }
                }

                sendKey(goToTimeEdit, Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (goToTimeToolbarAction->isVisible()
                    || goToTimeBar->isVisibleTo(&window)
                    || !canvas->viewport()->hasFocus()
                    || canvas->cursorTick() != 375'000
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("edit cursor remains at 375 ns"))
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Escape did not close time navigation while retaining its result"));
                    return;
                }

                sendKey(canvas, Qt::Key_G, Qt::ControlModifier);
                QCoreApplication::processEvents();
                if (!goToTimeToolbarAction->isVisible()
                    || !goToTimeEdit->hasFocus()
                    || goToTimeEdit->text() != QStringLiteral("375 ns")
                    || goToTimeEdit->selectedText() != QStringLiteral("375 ns")) {
                    fail(QStringLiteral(
                        "Ctrl+G did not reopen at the current edit cursor"));
                    return;
                }
                goToTimeEdit->setText(QStringLiteral("cycle 25"));
                sendKey(goToTimeEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                if (canvas->cursorTick() != 250'000
                    || goToTimeEdit->text() != QStringLiteral("250 ns")
                    || goToTimeEdit->selectedText() != QStringLiteral("250 ns")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("cycle 25 on navigation clock"))
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Cycle-based time did not use the unambiguous project clock"));
                    return;
                }
                goToTimeClose->click();
                QCoreApplication::processEvents();
                if (goToTimeToolbarAction->isVisible()
                    || !canvas->viewport()->hasFocus()
                    || canvas->cursorTick() != 250'000
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("edit cursor remains at 250 ns"))) {
                    fail(QStringLiteral(
                        "Time navigation close button did not preserve the cycle result"));
                    return;
                }

                sendKey(canvas, Qt::Key_Home);
                sendKey(canvas, Qt::Key_Right, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                const auto goToGuardRange = canvas->selectedTimeRange();
                if (!canvas->hasExplicitRangeSelection()
                    || !goToGuardRange
                    || goToGuardRange->first != 0
                    || goToGuardRange->second != 10'000) {
                    fail(QStringLiteral(
                        "Time navigation range guard did not start from a 0–10 ns range"));
                    return;
                }
                sendKey(canvas, Qt::Key_G, Qt::ControlModifier);
                QCoreApplication::processEvents();
                if (goToTimeToolbarAction->isVisible()
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != goToGuardRange
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Esc clears the selected range"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+G"))
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Ctrl+G discarded an explicit range instead of explaining how to continue"));
                    return;
                }
                sendKey(canvas, Qt::Key_Escape);
                clickSignalHeader(laneY);
                sendKey(canvas, Qt::Key_Home);
                QCoreApplication::processEvents();
                if (canvas->hasExplicitRangeSelection()
                    || canvas->cursorTick() != 0
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || goToTimeToolbarAction->isVisible()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Time navigation range guard did not restore normal edge navigation"));
                    return;
                }

                sendKey(canvas, Qt::Key_Right, Qt::ControlModifier);
                if (canvas->cursorTick() != 50'000
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || canvas->horizontalScrollBar()->value() != 0
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Next edge on data[7:0]"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("value 0x35"))) {
                    fail(QStringLiteral(
                        "Ctrl+Right did not jump to the Bus segment start with its value"));
                    return;
                }
                sendKey(canvas, Qt::Key_Right, Qt::ControlModifier);
                if (canvas->cursorTick() != 100'000
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Next edge on data[7:0]"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("value X"))) {
                    fail(QStringLiteral(
                        "Ctrl+Right did not jump to the Bus segment end with its implicit value"));
                    return;
                }
                sendKey(canvas, Qt::Key_Left, Qt::ControlModifier);
                if (canvas->cursorTick() != 50'000
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Previous edge on data[7:0]"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("value 0x35"))) {
                    fail(QStringLiteral(
                        "Ctrl+Left did not return to the Bus segment start with its value"));
                    return;
                }
                sendKey(canvas, Qt::Key_Left, Qt::ControlModifier);
                if (canvas->cursorTick() != 50'000
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("No earlier edge on data[7:0]"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Home jumps to 0"))
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Bus edge navigation did not report its earlier boundary"));
                    return;
                }

                sendKey(canvas, Qt::Key_Return);
                QCoreApplication::processEvents();
                auto* busPalette = window.findChild<QWidget*>(QStringLiteral("BusPresetPalette"));
                auto* busValueEdit = window.findChild<QLineEdit*>(QStringLiteral("BusPresetValueEdit"));
                const std::array<QToolButton*, 4> busPresetButtons{
                    window.findChild<QToolButton*>(QStringLiteral("BusPresetZeroButton")),
                    window.findChild<QToolButton*>(QStringLiteral("BusPresetXButton")),
                    window.findChild<QToolButton*>(QStringLiteral("BusPresetZButton")),
                    window.findChild<QToolButton*>(QStringLiteral("BusPresetDontCareButton")),
                };
                const auto busPresetsVisible = std::all_of(
                    busPresetButtons.begin(),
                    busPresetButtons.end(),
                    [](const QToolButton* button) {
                        return button && button->isVisible();
                    });
                const auto busEditEntryStatus = window.statusBar()->currentMessage();
                if (!busPalette
                    || !busPalette->isVisible()
                    || !busValueEdit
                    || !busPresetsVisible
                    || !busValueEdit->hasFocus()
                    || busValueEdit->text() != QStringLiteral("0x35")
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !busEditEntryStatus.contains(
                        QStringLiteral("Edit data[7:0] at 50 ns"))
                    || !busEditEntryStatus.contains(
                        QStringLiteral("current value 0x35"))
                    || !busEditEntryStatus.contains(QStringLiteral("Enter"))
                    || !busEditEntryStatus.contains(QStringLiteral("Esc cancels"))) {
                    qCritical().noquote()
                        << "Bus keyboard edit diagnostics"
                        << "palette" << (busPalette && busPalette->isVisible())
                        << "editor" << static_cast<bool>(busValueEdit)
                        << "focus" << (busValueEdit && busValueEdit->hasFocus())
                        << "text" << (busValueEdit
                                ? busValueEdit->text()
                                : QStringLiteral("<missing>"))
                        << "cursor" << canvas->cursorTick()
                        << "scenarioChanged" << (scenario != originalScenario)
                        << "undo" << undoAction->isEnabled()
                        << "save" << saveState->text()
                        << "status" << busEditEntryStatus;
                    fail(QStringLiteral(
                        "Enter did not open the selected Bus value as a reversible inline draft"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto busEditScreenshotPath = waveEditAutoScrollScreenshotPath;
                    const auto suffix = busEditScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        busEditScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-keyboard-bus-edit"));
                    } else {
                        busEditScreenshotPath.append(
                            QStringLiteral("-keyboard-bus-edit.png"));
                    }
                    if (!window.grab().save(busEditScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save keyboard Bus value edit screenshot"));
                        return;
                    }
                }

                sendKey(busValueEdit, Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (busPalette->isVisible()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Escape did not discard the keyboard Bus value draft without changes"));
                    return;
                }

                canvas->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Return);
                QCoreApplication::processEvents();
                if (!busPalette->isVisible() || !busValueEdit->hasFocus()) {
                    fail(QStringLiteral(
                        "Enter could not reopen the selected Bus value editor"));
                    return;
                }
                busValueEdit->setText(QStringLiteral("0x5a"));
                busValueEdit->setModified(true);
                sendKey(busValueEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                const auto* keyboardEditedBus = wave::findLane(
                    scenario,
                    "lane-wave-edit-scroll");
                const auto keyboardEditedValue = keyboardEditedBus
                    && std::any_of(
                        keyboardEditedBus->segments.begin(),
                        keyboardEditedBus->segments.end(),
                        [](const wave::Segment& segment) {
                            return segment.start <= 50'000
                                && 50'000 < segment.end
                                && segment.value == "0x5a";
                        });
                const auto keyboardSelection = canvas->selectedTimeRange();
                const auto busEditCommitStatus = window.statusBar()->currentMessage();
                if (busPalette->isVisible()
                    || !keyboardEditedValue
                    || !keyboardSelection
                    || keyboardSelection->first > 50'000
                    || keyboardSelection->second <= 50'000
                    || scenario == originalScenario
                    || !undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Unsaved changes")
                    || !window.windowTitle().contains(QStringLiteral(" *"))
                    || !busEditCommitStatus.contains(QStringLiteral("data[7:0]"))
                    || !busEditCommitStatus.contains(QStringLiteral("0x5a"))
                    || !busEditCommitStatus.contains(QStringLiteral("Ctrl+Z"))) {
                    fail(QStringLiteral(
                        "Keyboard Bus value commit did not change one beat with visible undo feedback"));
                    return;
                }
                undoAction->trigger();
                QCoreApplication::processEvents();
                if (scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || window.windowTitle().contains(QStringLiteral(" *"))) {
                    fail(QStringLiteral(
                        "Undo did not restore the exact pre-keyboard-edit Bus and Saved state"));
                    return;
                }
                const auto enumLaneY = 40
                    + scenario.lanes.front().height
                    + scenario.lanes.at(1).height / 2;
                clickSignalHeader(enumLaneY);
                const auto enumSelectionStatus = window.statusBar()->currentMessage();
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-enum")
                    || !enumSelectionStatus.contains(
                        QStringLiteral("Selected signal state"))
                    || !enumSelectionStatus.contains(
                        QStringLiteral("value WAIT_ACK"))
                    || !enumSelectionStatus.contains(
                        QStringLiteral("Enter edits value"))) {
                    fail(QStringLiteral(
                        "Enum selection did not disclose its value and direct edit path"));
                    return;
                }
                canvas->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Return);
                QCoreApplication::processEvents();
                auto* enumContext = window.findChild<QLabel*>(QStringLiteral("BusPresetContextLabel"));
                auto* enumCompleter = busValueEdit->completer();
                QStringList enumCompletions;
                if (enumCompleter && enumCompleter->model()) {
                    for (auto row = 0; row < enumCompleter->model()->rowCount(); ++row) {
                        enumCompletions.append(
                            enumCompleter->model()->index(row, 0).data().toString());
                    }
                }
                const auto enumPresetsHidden = std::all_of(
                    busPresetButtons.begin(),
                    busPresetButtons.end(),
                    [](const QToolButton* button) {
                        return button && !button->isVisible();
                    });
                const auto enumEditEntryStatus = window.statusBar()->currentMessage();
                if (!busPalette->isVisible()
                    || !busValueEdit->hasFocus()
                    || busValueEdit->text() != QStringLiteral("WAIT_ACK")
                    || busValueEdit->accessibleName() != QStringLiteral("Enum value")
                    || !busValueEdit->placeholderText().contains(
                        QStringLiteral("Symbol"))
                    || !enumContext
                    || !enumContext->text().contains(QStringLiteral("state"))
                    || !enumContext->text().contains(QStringLiteral("50 ns"))
                    || !enumContext->toolTip().contains(
                        QStringLiteral("DONE, IDLE, WAIT_ACK"))
                    || enumCompletions
                        != QStringList{
                            QStringLiteral("DONE"),
                            QStringLiteral("IDLE"),
                            QStringLiteral("WAIT_ACK"),
                        }
                    || !enumPresetsHidden
                    || QApplication::activeModalWidget()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !enumEditEntryStatus.contains(
                        QStringLiteral("Edit state at 50 ns"))
                    || !enumEditEntryStatus.contains(
                        QStringLiteral("current value WAIT_ACK"))) {
                    qCritical().noquote()
                        << "Enum keyboard edit diagnostics"
                        << "palette" << busPalette->isVisible()
                        << "focus" << busValueEdit->hasFocus()
                        << "value" << busValueEdit->text()
                        << "accessible" << busValueEdit->accessibleName()
                        << "placeholder" << busValueEdit->placeholderText()
                        << "context" << (enumContext
                                ? enumContext->text()
                                : QStringLiteral("<missing>"))
                        << "contextHelp" << (enumContext
                                ? enumContext->toolTip()
                                : QStringLiteral("<missing>"))
                        << "completions" << enumCompletions.join(QLatin1Char(','))
                        << "presetsHidden" << enumPresetsHidden
                        << "status" << enumEditEntryStatus;
                    fail(QStringLiteral(
                        "Enter did not open a discoverable non-modal Enum value editor"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto enumEditScreenshotPath = waveEditAutoScrollScreenshotPath;
                    const auto suffix = enumEditScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        enumEditScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-keyboard-enum-edit"));
                    } else {
                        enumEditScreenshotPath.append(
                            QStringLiteral("-keyboard-enum-edit.png"));
                    }
                    if (!window.grab().save(enumEditScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save keyboard Enum value edit screenshot"));
                        return;
                    }
                }

                const auto enumCycleBaseline = scenario;
                sendKey(busValueEdit, Qt::Key_Down);
                QCoreApplication::processEvents();
                const auto enumCycleForwardStatus =
                    window.statusBar()->currentMessage();
                if (busValueEdit->text() != QStringLiteral("DONE")
                    || !busValueEdit->isModified()
                    || scenario != enumCycleBaseline
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !enumCycleForwardStatus.contains(
                        QStringLiteral("Enum symbol 1 of 3: DONE"))) {
                    fail(QStringLiteral(
                        "Enum Down key did not cycle to the next declared symbol without editing the model"));
                    return;
                }
                sendKey(busValueEdit, Qt::Key_Up);
                QCoreApplication::processEvents();
                const auto enumCycleBackwardStatus =
                    window.statusBar()->currentMessage();
                if (busValueEdit->text() != QStringLiteral("WAIT_ACK")
                    || scenario != enumCycleBaseline
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !enumCycleBackwardStatus.contains(
                        QStringLiteral("Enum symbol 3 of 3: WAIT_ACK"))
                    || !enumCycleBackwardStatus.contains(
                        QStringLiteral("Up/Down cycles"))) {
                    fail(QStringLiteral(
                        "Enum Up key did not cycle back through declared symbols"));
                    return;
                }

                busValueEdit->setText(QStringLiteral("MISSING"));
                busValueEdit->setModified(true);
                sendKey(busValueEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                const auto invalidEnumStatus = window.statusBar()->currentMessage();
                if (!busPalette->isVisible()
                    || !busValueEdit->hasFocus()
                    || busValueEdit->text() != QStringLiteral("MISSING")
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !invalidEnumStatus.contains(
                        QStringLiteral("symbols: DONE, IDLE, WAIT_ACK"))) {
                    fail(QStringLiteral(
                        "Invalid Enum value was not retained with declared-symbol guidance"));
                    return;
                }
                sendKey(busValueEdit, Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (busPalette->isVisible()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Escape did not discard the invalid Enum draft without changes"));
                    return;
                }

                canvas->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Return);
                QCoreApplication::processEvents();
                busValueEdit->setText(QStringLiteral("DONE"));
                busValueEdit->setModified(true);
                sendKey(busValueEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                const auto* keyboardEditedEnum = wave::findLane(
                    scenario,
                    "lane-wave-edit-enum");
                const auto enumEditedValue = keyboardEditedEnum
                    && std::any_of(
                        keyboardEditedEnum->segments.begin(),
                        keyboardEditedEnum->segments.end(),
                        [](const wave::Segment& segment) {
                            return segment.start <= 50'000
                                && 50'000 < segment.end
                                && segment.value == "DONE";
                        });
                const auto enumCommitStatus = window.statusBar()->currentMessage();
                if (busPalette->isVisible()
                    || !enumEditedValue
                    || scenario == originalScenario
                    || !undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Unsaved changes")
                    || !enumCommitStatus.contains(QStringLiteral("state"))
                    || !enumCommitStatus.contains(QStringLiteral("DONE"))
                    || !enumCommitStatus.contains(QStringLiteral("Ctrl+Z"))) {
                    fail(QStringLiteral(
                        "Keyboard Enum value commit did not create one visible undoable edit"));
                    return;
                }
                undoAction->trigger();
                QCoreApplication::processEvents();
                if (scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || window.windowTitle().contains(QStringLiteral(" *"))) {
                    fail(QStringLiteral(
                        "Undo did not restore the exact pre-Enum-edit Scenario and Saved state"));
                    return;
                }

                const auto enumNextLaneY = 40
                    + scenario.lanes.front().height
                    + scenario.lanes.at(1).height
                    + scenario.lanes.at(2).height / 2;
                const QPoint enumRangeStart(xAtTick(50'000), enumLaneY);
                const QPoint enumRangeEnd(xAtTick(100'000), enumNextLaneY);
                sendMouse(
                    QEvent::MouseButtonPress,
                    enumRangeStart,
                    Qt::LeftButton,
                    Qt::LeftButton,
                    Qt::ShiftModifier);
                sendMouse(
                    QEvent::MouseMove,
                    enumRangeEnd,
                    Qt::NoButton,
                    Qt::LeftButton,
                    Qt::ShiftModifier);
                releaseLeftButton(enumRangeEnd, Qt::ShiftModifier);
                auto* enumRangePalette = window.findChild<QFrame*>(
                    QStringLiteral("RangeEditPalette"));
                auto* enumRangeContext = window.findChild<QLabel*>(
                    QStringLiteral("RangeEditContextLabel"));
                auto* enumRangeValueEdit = window.findChild<QLineEdit*>(
                    QStringLiteral("RangeEditValueEdit"));
                const std::array<QToolButton*, 5> enumRangePresetButtons{
                    window.findChild<QToolButton*>(
                        QStringLiteral("RangeEditZeroButton")),
                    window.findChild<QToolButton*>(
                        QStringLiteral("RangeEditOneButton")),
                    window.findChild<QToolButton*>(
                        QStringLiteral("RangeEditXButton")),
                    window.findChild<QToolButton*>(
                        QStringLiteral("RangeEditZButton")),
                    window.findChild<QToolButton*>(
                        QStringLiteral("RangeEditDontCareButton")),
                };
                QStringList enumRangeCompletions;
                auto* enumRangeCompleter = enumRangeValueEdit
                    ? enumRangeValueEdit->completer()
                    : nullptr;
                if (enumRangeCompleter && enumRangeCompleter->model()) {
                    for (auto row = 0;
                         row < enumRangeCompleter->model()->rowCount();
                         ++row) {
                        enumRangeCompletions.append(
                            enumRangeCompleter->model()
                                ->index(row, 0)
                                .data()
                                .toString());
                    }
                }
                const auto enumRangePresetsHidden = std::all_of(
                    enumRangePresetButtons.begin(),
                    enumRangePresetButtons.end(),
                    [&window](const QToolButton* button) {
                        return button && !button->isVisibleTo(&window);
                    });
                const auto selectedEnumRange = canvas->selectedTimeRange();
                const auto selectedEnumLanes = canvas->selectedLaneIds();
                const auto enumRangeStatus = window.statusBar()->currentMessage();
                if (!canvas->hasExplicitRangeSelection()
                    || selectedEnumRange
                        != std::optional<std::pair<wave::Tick, wave::Tick>>{
                            std::pair<wave::Tick, wave::Tick>{50'000, 100'000}}
                    || selectedEnumLanes
                        != QStringList{
                            QStringLiteral("lane-wave-edit-enum"),
                            QStringLiteral("lane-wave-edit-enum-next"),
                        }
                    || !enumRangePalette
                    || !enumRangePalette->isVisibleTo(&window)
                    || !enumRangeContext
                    || !enumRangeContext->text().contains(
                        QStringLiteral("2 Enum"))
                    || !enumRangeContext->toolTip().contains(
                        QStringLiteral("Shared symbols: DONE, IDLE"))
                    || !enumRangeValueEdit
                    || !enumRangeValueEdit->isVisibleTo(&window)
                    || enumRangeValueEdit->accessibleName()
                        != QStringLiteral("Selected Enum range value")
                    || !enumRangeValueEdit->placeholderText().contains(
                        QStringLiteral("Symbol"))
                    || enumRangeCompletions
                        != QStringList{
                            QStringLiteral("DONE"),
                            QStringLiteral("IDLE"),
                        }
                    || !enumRangePresetsHidden
                    || QApplication::activeModalWidget()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !enumRangeStatus.contains(
                        QStringLiteral("2 Enum signals"))
                    || !enumRangeStatus.contains(
                        QStringLiteral("shared symbol"))) {
                    qCritical().noquote()
                        << "Enum range diagnostics"
                        << "selection"
                        << (selectedEnumRange
                                ? QStringLiteral("%1-%2")
                                      .arg(selectedEnumRange->first)
                                      .arg(selectedEnumRange->second)
                                : QStringLiteral("<missing>"))
                        << "lanes" << selectedEnumLanes.join(QLatin1Char(','))
                        << "palette"
                        << (enumRangePalette
                                && enumRangePalette->isVisibleTo(&window))
                        << "context"
                        << (enumRangeContext
                                ? enumRangeContext->text()
                                : QStringLiteral("<missing>"))
                        << "help"
                        << (enumRangeContext
                                ? enumRangeContext->toolTip()
                                : QStringLiteral("<missing>"))
                        << "completions"
                        << enumRangeCompletions.join(QLatin1Char(','))
                        << "presetsHidden" << enumRangePresetsHidden
                        << "status" << enumRangeStatus;
                    fail(QStringLiteral(
                        "Enum multi-lane range did not expose a shared-symbol editor"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto enumRangeScreenshotPath =
                        waveEditAutoScrollScreenshotPath;
                    const auto suffix = enumRangeScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        enumRangeScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-enum-range-edit"));
                    } else {
                        enumRangeScreenshotPath.append(
                            QStringLiteral("-enum-range-edit.png"));
                    }
                    if (!window.grab().save(enumRangeScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save Enum range edit screenshot"));
                        return;
                    }
                }

                enumRangeValueEdit->setFocus(Qt::OtherFocusReason);
                enumRangeValueEdit->setText(QStringLiteral("MISSING"));
                enumRangeValueEdit->setModified(true);
                sendKey(enumRangeValueEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                const auto invalidEnumRangeStatus =
                    window.statusBar()->currentMessage();
                if (!enumRangeValueEdit->hasFocus()
                    || !enumRangeValueEdit->isModified()
                    || enumRangeValueEdit->text() != QStringLiteral("MISSING")
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != selectedEnumRange
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !invalidEnumRangeStatus.contains(
                        QStringLiteral("shared symbols: DONE, IDLE"))
                    || canvas->commitPendingInlineEdits()) {
                    fail(QStringLiteral(
                        "Invalid Enum range value was not retained behind the draft gate"));
                    return;
                }

                enumRangeValueEdit->setText(QStringLiteral("DONE"));
                enumRangeValueEdit->setModified(true);
                sendKey(enumRangeValueEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                const auto enumValueAt = [](
                                             const wave::Lane* lane,
                                             const wave::Tick tick) {
                    if (!lane) return std::string{};
                    const auto segment = std::find_if(
                        lane->segments.begin(),
                        lane->segments.end(),
                        [tick](const wave::Segment& candidate) {
                            return candidate.start <= tick
                                && tick < candidate.end;
                        });
                    return segment == lane->segments.end()
                        ? std::string{}
                        : segment->value;
                };
                const auto* rangeEditedState = wave::findLane(
                    scenario,
                    "lane-wave-edit-enum");
                const auto* rangeEditedNextState = wave::findLane(
                    scenario,
                    "lane-wave-edit-enum-next");
                const auto enumRangeCommitStatus =
                    window.statusBar()->currentMessage();
                if (enumValueAt(rangeEditedState, 25'000) != "IDLE"
                    || enumValueAt(rangeEditedState, 75'000) != "DONE"
                    || enumValueAt(rangeEditedState, 125'000) != "DONE"
                    || enumValueAt(rangeEditedNextState, 25'000) != "IDLE"
                    || enumValueAt(rangeEditedNextState, 75'000) != "DONE"
                    || enumValueAt(rangeEditedNextState, 125'000) != "IDLE"
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != selectedEnumRange
                    || !enumRangePalette->isVisibleTo(&window)
                    || scenario == originalScenario
                    || !undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Unsaved changes")
                    || !enumRangeCommitStatus.contains(
                        QStringLiteral("2 signals"))
                    || !enumRangeCommitStatus.contains(QStringLiteral("DONE"))
                    || !enumRangeCommitStatus.contains(QStringLiteral("Ctrl+Z"))) {
                    fail(QStringLiteral(
                        "Enum range submission did not atomically update both selected signals"));
                    return;
                }
                undoAction->trigger();
                QCoreApplication::processEvents();
                if (scenario != originalScenario
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != selectedEnumRange
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || window.windowTitle().contains(QStringLiteral(" *"))) {
                    fail(QStringLiteral(
                        "Enum range Undo did not restore the exact Scenario and selection"));
                    return;
                }
                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Left, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                const auto keyboardExtendedEnumRange =
                    std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{40'000, 100'000}};
                const auto keyboardExtendStatus =
                    window.statusBar()->currentMessage();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != keyboardExtendedEnumRange
                    || canvas->selectedLaneIds() != selectedEnumLanes
                    || canvas->cursorTick() != 40'000
                    || !enumRangePalette->isVisibleTo(&window)
                    || !enumRangeContext->text().contains(QStringLiteral("2 Enum"))
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !keyboardExtendStatus.contains(
                        QStringLiteral("Keyboard range"))
                    || !keyboardExtendStatus.contains(
                        QStringLiteral("active edge"))) {
                    qCritical().noquote()
                        << "Keyboard Enum range diagnostics"
                        << "range"
                        << (canvas->selectedTimeRange()
                                ? QStringLiteral("%1-%2")
                                      .arg(canvas->selectedTimeRange()->first)
                                      .arg(canvas->selectedTimeRange()->second)
                                : QStringLiteral("<none>"))
                        << "lanes" << canvas->selectedLaneIds().join(QLatin1Char(','))
                        << "cursor" << canvas->cursorTick()
                        << "palette" << enumRangePalette->isVisibleTo(&window)
                        << "context" << enumRangeContext->text()
                        << "modified" << enumRangeValueEdit->isModified()
                        << "focus" << enumRangeValueEdit->hasFocus()
                        << "scenarioEqual" << (scenario == originalScenario)
                        << "undo" << undoAction->isEnabled()
                        << "save" << saveState->text()
                        << "status" << keyboardExtendStatus;
                    fail(QStringLiteral(
                        "Shift+Left did not extend the active Enum range edge without editing the model"));
                    return;
                }
                sendKey(canvas, Qt::Key_Right, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != selectedEnumRange
                    || canvas->selectedLaneIds() != selectedEnumLanes
                    || canvas->cursorTick() != 50'000
                    || !enumRangePalette->isVisibleTo(&window)
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Shift+Right did not shrink the active Enum range edge back to its original time"));
                    return;
                }
                canvas->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (canvas->hasExplicitRangeSelection()
                    || enumRangePalette->isVisibleTo(&window)
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Escape did not clear the completed Enum range context"));
                    return;
                }

                sendKey(canvas, Qt::Key_Home);
                clickSignalHeader(enumLaneY);
                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Right, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                const auto keyboardSingleEnumRange =
                    std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{0, 10'000}};
                const auto keyboardSingleEnumStatus =
                    window.statusBar()->currentMessage();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != keyboardSingleEnumRange
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-enum")
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-enum")}
                    || canvas->cursorTick() != 10'000
                    || !enumRangePalette->isVisibleTo(&window)
                    || !enumRangeContext->text().contains(QStringLiteral("1 Enum"))
                    || !enumRangeContext->toolTip().contains(
                        QStringLiteral("Shift+Up/Down adjusts signals"))
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !keyboardSingleEnumStatus.contains(
                        QStringLiteral("Shift+Up/Down adjusts signals"))) {
                    fail(QStringLiteral(
                        "Keyboard time selection did not expose signal-range adjustment"));
                    return;
                }

                enumRangeValueEdit->setText(QStringLiteral("D"));
                enumRangeValueEdit->setModified(true);
                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Down, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                const auto rangeDraftSignalStatus =
                    window.statusBar()->currentMessage();
                if (canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-enum")}
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-enum")
                    || enumRangeValueEdit->text() != QStringLiteral("D")
                    || !enumRangeValueEdit->isModified()
                    || !enumRangeValueEdit->hasFocus()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !rangeDraftSignalStatus.contains(
                        QStringLiteral("Finish the selected range value"))) {
                    fail(QStringLiteral(
                        "Shift+Down changed range signals or discarded an unfinished value draft"));
                    return;
                }

                enumRangeValueEdit->clear();
                enumRangeValueEdit->setModified(false);
                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Down, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                QStringList keyboardSignalEnumCompletions;
                if (enumRangeCompleter && enumRangeCompleter->model()) {
                    for (auto row = 0;
                         row < enumRangeCompleter->model()->rowCount();
                         ++row) {
                        keyboardSignalEnumCompletions.append(
                            enumRangeCompleter->model()
                                ->index(row, 0)
                                .data()
                                .toString());
                    }
                }
                const auto keyboardSignalExtendStatus =
                    window.statusBar()->currentMessage();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != keyboardSingleEnumRange
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-enum-next")
                    || canvas->selectedLaneIds()
                        != QStringList{
                            QStringLiteral("lane-wave-edit-enum"),
                            QStringLiteral("lane-wave-edit-enum-next"),
                        }
                    || canvas->cursorTick() != 10'000
                    || !enumRangePalette->isVisibleTo(&window)
                    || !enumRangeContext->text().contains(QStringLiteral("2 Enum"))
                    || !enumRangeContext->toolTip().contains(
                        QStringLiteral("Shared symbols: DONE, IDLE"))
                    || keyboardSignalEnumCompletions
                        != QStringList{
                            QStringLiteral("DONE"),
                            QStringLiteral("IDLE"),
                        }
                    || enumRangeValueEdit->isModified()
                    || QApplication::activeModalWidget()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !keyboardSignalExtendStatus.contains(
                        QStringLiteral("Keyboard signal range"))
                    || !keyboardSignalExtendStatus.contains(
                        QStringLiteral("2 Enum signal(s)"))
                    || !keyboardSignalExtendStatus.contains(
                        QStringLiteral("active state_next"))) {
                    qCritical().noquote()
                        << "Keyboard signal range diagnostics"
                        << "range"
                        << (canvas->selectedTimeRange()
                                ? QStringLiteral("%1-%2")
                                      .arg(canvas->selectedTimeRange()->first)
                                      .arg(canvas->selectedTimeRange()->second)
                                : QStringLiteral("<none>"))
                        << "active" << canvas->selectedLaneId()
                        << "lanes" << canvas->selectedLaneIds().join(QLatin1Char(','))
                        << "context" << enumRangeContext->text()
                        << "help" << enumRangeContext->toolTip()
                        << "completions"
                        << keyboardSignalEnumCompletions.join(QLatin1Char(','))
                        << "status" << keyboardSignalExtendStatus;
                    fail(QStringLiteral(
                        "Shift+Down did not extend the range to the adjacent Enum signal"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto keyboardSignalRangeScreenshotPath =
                        waveEditAutoScrollScreenshotPath;
                    const auto suffix = keyboardSignalRangeScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        keyboardSignalRangeScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-keyboard-signal-range"));
                    } else {
                        keyboardSignalRangeScreenshotPath.append(
                            QStringLiteral("-keyboard-signal-range.png"));
                    }
                    if (!window.grab().save(keyboardSignalRangeScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save keyboard signal-range screenshot"));
                        return;
                    }
                }

                sendKey(canvas, Qt::Key_Up, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                const auto keyboardSignalShrinkStatus =
                    window.statusBar()->currentMessage();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != keyboardSingleEnumRange
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-enum")
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-enum")}
                    || canvas->cursorTick() != 10'000
                    || !enumRangePalette->isVisibleTo(&window)
                    || !enumRangeContext->text().contains(QStringLiteral("1 Enum"))
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !keyboardSignalShrinkStatus.contains(
                        QStringLiteral("1 Enum signal(s)"))
                    || !keyboardSignalShrinkStatus.contains(
                        QStringLiteral("active state"))) {
                    fail(QStringLiteral(
                        "Shift+Up did not shrink the range back to its anchor signal"));
                    return;
                }
                sendKey(canvas, Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (canvas->hasExplicitRangeSelection()
                    || enumRangePalette->isVisibleTo(&window)
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Escape did not clear the keyboard signal range"));
                    return;
                }

                sendKey(canvas, Qt::Key_Home);
                clickSignalHeader(laneY);
                if (!window.statusBar()->currentMessage().contains(
                        QStringLiteral("Shift+Left/Right selects time"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Shift+Home/End selects to boundary"))) {
                    fail(QStringLiteral(
                        "Signal selection did not disclose keyboard time-range selection"));
                    return;
                }
                sendKey(canvas, Qt::Key_Right, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                const auto keyboardBusRange =
                    std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{0, 10'000}};
                const auto keyboardCreateStatus =
                    window.statusBar()->currentMessage();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != keyboardBusRange
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-scroll")}
                    || canvas->cursorTick() != 10'000
                    || !enumRangePalette->isVisibleTo(&window)
                    || !enumRangeContext->text().contains(QStringLiteral("1 Bus"))
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !keyboardCreateStatus.contains(
                        QStringLiteral("Keyboard range"))
                    || !keyboardCreateStatus.contains(
                        QStringLiteral("Esc clears"))) {
                    fail(QStringLiteral(
                        "Shift+Right did not create a visible one-step Bus range without editing the model"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto keyboardRangeScreenshotPath =
                        waveEditAutoScrollScreenshotPath;
                    const auto suffix = keyboardRangeScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        keyboardRangeScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-keyboard-range"));
                    } else {
                        keyboardRangeScreenshotPath.append(
                            QStringLiteral("-keyboard-range.png"));
                    }
                    if (!window.grab().save(keyboardRangeScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save keyboard time-range screenshot"));
                        return;
                    }
                }
                enumRangeValueEdit->setText(QStringLiteral("0xa5"));
                enumRangeValueEdit->setModified(false);
                enumRangeValueEdit->setCursorPosition(
                    enumRangeValueEdit->text().size());
                enumRangeValueEdit->setFocus(Qt::OtherFocusReason);
                sendKey(enumRangeValueEdit, Qt::Key_Home, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                if (enumRangeValueEdit->selectedText() != QStringLiteral("0xa5")
                    || enumRangeValueEdit->cursorPosition() != 0
                    || canvas->selectedTimeRange() != keyboardBusRange
                    || canvas->cursorTick() != 10'000
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Shift+Home escaped the range value field instead of selecting its text"));
                    return;
                }
                enumRangeValueEdit->clear();
                enumRangeValueEdit->setModified(false);

                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                const auto boundaryScrollMaximum =
                    canvas->horizontalScrollBar()->maximum();
                sendKey(canvas, Qt::Key_End, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                const auto keyboardBoundaryRange =
                    std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{0, scenario.duration}};
                const auto keyboardBoundaryStatus =
                    window.statusBar()->currentMessage();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != keyboardBoundaryRange
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-scroll")}
                    || canvas->cursorTick() != scenario.duration
                    || boundaryScrollMaximum <= 0
                    || canvas->horizontalScrollBar()->value()
                        != boundaryScrollMaximum
                    || !enumRangePalette->isVisibleTo(&window)
                    || !enumRangeContext->text().contains(QStringLiteral("1 Bus"))
                    || !enumRangeContext->toolTip().contains(
                        QStringLiteral("Shift+Home/End selects to a timeline boundary"))
                    || enumRangeValueEdit->isModified()
                    || QApplication::activeModalWidget()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !keyboardBoundaryStatus.contains(
                        QStringLiteral("Keyboard range"))
                    || !keyboardBoundaryStatus.contains(
                        QStringLiteral("Shift+Home/End selects to boundary"))) {
                    qCritical().noquote()
                        << "Keyboard boundary range diagnostics"
                        << "range"
                        << (canvas->selectedTimeRange()
                                ? QStringLiteral("%1-%2")
                                      .arg(canvas->selectedTimeRange()->first)
                                      .arg(canvas->selectedTimeRange()->second)
                                : QStringLiteral("<none>"))
                        << "cursor" << canvas->cursorTick()
                        << "scroll" << canvas->horizontalScrollBar()->value()
                        << "maximum" << boundaryScrollMaximum
                        << "context" << enumRangeContext->text()
                        << "help" << enumRangeContext->toolTip()
                        << "status" << keyboardBoundaryStatus;
                    fail(QStringLiteral(
                        "Shift+End did not extend the active range edge to timeline End"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto keyboardBoundaryScreenshotPath =
                        waveEditAutoScrollScreenshotPath;
                    const auto suffix = keyboardBoundaryScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        keyboardBoundaryScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-keyboard-boundary-range"));
                    } else {
                        keyboardBoundaryScreenshotPath.append(
                            QStringLiteral("-keyboard-boundary-range.png"));
                    }
                    if (!window.grab().save(keyboardBoundaryScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save keyboard boundary-range screenshot"));
                        return;
                    }
                }

                sendKey(canvas, Qt::Key_End, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                const auto keyboardBoundaryNoEffectStatus =
                    window.statusBar()->currentMessage();
                if (canvas->selectedTimeRange() != keyboardBoundaryRange
                    || canvas->cursorTick() != scenario.duration
                    || canvas->horizontalScrollBar()->value()
                        != boundaryScrollMaximum
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !keyboardBoundaryNoEffectStatus.contains(
                        QStringLiteral("Timeline end reached"))
                    || !keyboardBoundaryNoEffectStatus.contains(
                        QStringLiteral("Shift+Home moves the active edge back"))) {
                    fail(QStringLiteral(
                        "Repeated Shift+End changed the range or omitted boundary feedback"));
                    return;
                }

                sendKey(canvas, Qt::Key_Home, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                const auto keyboardBoundaryCollapseStatus =
                    window.statusBar()->currentMessage();
                if (canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange()
                    || enumRangePalette->isVisibleTo(&window)
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-scroll")}
                    || canvas->cursorTick() != 0
                    || canvas->horizontalScrollBar()->value() != 0
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !keyboardBoundaryCollapseStatus.contains(
                        QStringLiteral("Range collapsed"))
                    || !keyboardBoundaryCollapseStatus.contains(
                        QStringLiteral("Shift+Home/End starts a new range"))) {
                    fail(QStringLiteral(
                        "Shift+Home did not collapse the boundary range at its anchor"));
                    return;
                }

                sendKey(canvas, Qt::Key_Right, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != keyboardBusRange
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-scroll")}
                    || canvas->cursorTick() != 10'000
                    || !enumRangePalette->isVisibleTo(&window)
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Keyboard range could not restart after boundary collapse"));
                    return;
                }
                sendKey(canvas, Qt::Key_Left, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                const auto keyboardCollapseStatus =
                    window.statusBar()->currentMessage();
                if (canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange()
                    || enumRangePalette->isVisibleTo(&window)
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-scroll")}
                    || canvas->cursorTick() != 0
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !keyboardCollapseStatus.contains(
                        QStringLiteral("Range collapsed"))
                    || !keyboardCollapseStatus.contains(
                        QStringLiteral("starts a new range"))) {
                    fail(QStringLiteral(
                        "Shift+Left did not collapse the keyboard range while preserving the signal target"));
                    return;
                }
                sendKey(canvas, Qt::Key_Delete);
                QCoreApplication::processEvents();
                if (scenario != originalScenario
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Collapsed keyboard range left whole-signal Delete armed"));
                    return;
                }

                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_A, Qt::ControlModifier);
                QCoreApplication::processEvents();
                const auto fullTimelineRange =
                    std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{
                            0,
                            scenario.duration,
                        }};
                const auto singleSignalSelectAllStatus =
                    window.statusBar()->currentMessage();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != fullTimelineRange
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-scroll")}
                    || canvas->cursorTick() != 0
                    || canvas->horizontalScrollBar()->value() != 0
                    || !enumRangePalette->isVisibleTo(&window)
                    || !enumRangeContext->text().contains(QStringLiteral("1 Bus"))
                    || !enumRangeContext->toolTip().contains(
                        QStringLiteral("Ctrl+A selects the full timeline"))
                    || QApplication::activeModalWidget()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !singleSignalSelectAllStatus.contains(
                        QStringLiteral("Ctrl+A selected full timeline"))
                    || !singleSignalSelectAllStatus.contains(
                        QStringLiteral("1 signal(s)"))) {
                    fail(QStringLiteral(
                        "Ctrl+A did not select the full timeline for the current Bus without editing the model"));
                    return;
                }

                enumRangeValueEdit->setText(QStringLiteral("0xa5"));
                enumRangeValueEdit->setModified(false);
                enumRangeValueEdit->setCursorPosition(
                    enumRangeValueEdit->text().size());
                enumRangeValueEdit->setFocus(Qt::OtherFocusReason);
                sendKey(enumRangeValueEdit, Qt::Key_A, Qt::ControlModifier);
                QCoreApplication::processEvents();
                if (enumRangeValueEdit->selectedText()
                        != QStringLiteral("0xa5")
                    || canvas->selectedTimeRange() != fullTimelineRange
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-scroll")}
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Ctrl+A escaped the range value field instead of selecting its text"));
                    return;
                }
                enumRangeValueEdit->clear();
                enumRangeValueEdit->setModified(false);
                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Escape);
                sendKey(canvas, Qt::Key_Home);
                clickSignalHeader(enumLaneY);
                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Right, Qt::ShiftModifier);
                sendKey(canvas, Qt::Key_Down, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                if (canvas->selectedLaneIds() != selectedEnumLanes
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-enum-next")
                    || canvas->selectedTimeRange() != keyboardSingleEnumRange) {
                    fail(QStringLiteral(
                        "Multi-Enum range prerequisites were not restored before Ctrl+A"));
                    return;
                }
                sendKey(canvas, Qt::Key_A, Qt::ControlModifier);
                QCoreApplication::processEvents();
                const auto multiSignalSelectAllStatus =
                    window.statusBar()->currentMessage();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != fullTimelineRange
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-enum-next")
                    || canvas->selectedLaneIds() != selectedEnumLanes
                    || canvas->cursorTick() != 10'000
                    || !enumRangePalette->isVisibleTo(&window)
                    || !enumRangeContext->text().contains(QStringLiteral("2 Enum"))
                    || !enumRangeContext->toolTip().contains(
                        QStringLiteral("Shared symbols: DONE, IDLE"))
                    || QApplication::activeModalWidget()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !multiSignalSelectAllStatus.contains(
                        QStringLiteral("Ctrl+A selected full timeline"))
                    || !multiSignalSelectAllStatus.contains(
                        QStringLiteral("2 signal(s)"))) {
                    fail(QStringLiteral(
                        "Ctrl+A did not preserve the existing multi-Enum signal target range"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto selectAllScreenshotPath =
                        waveEditAutoScrollScreenshotPath;
                    const auto suffix = selectAllScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        selectAllScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-keyboard-select-all-range"));
                    } else {
                        selectAllScreenshotPath.append(
                            QStringLiteral("-keyboard-select-all-range.png"));
                    }
                    if (!window.grab().save(selectAllScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save keyboard select-all range screenshot"));
                        return;
                    }
                }
                sendKey(canvas, Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange()
                    || enumRangePalette->isVisibleTo(&window)
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Escape did not clear the Ctrl+A multi-signal selection"));
                    return;
                }

                sendKey(canvas, Qt::Key_Home);
                clickSignalHeader(laneY);
                if (!window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Shift+Left/Right selects to edges"))) {
                    fail(QStringLiteral(
                        "Signal selection did not disclose keyboard edge-range selection"));
                    return;
                }
                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                const auto edgeRangeModifiers =
                    Qt::ControlModifier | Qt::ShiftModifier;
                sendKey(canvas, Qt::Key_Right, edgeRangeModifiers);
                QCoreApplication::processEvents();
                const auto firstBusEdgeRange =
                    std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{0, 50'000}};
                const auto firstBusEdgeStatus =
                    window.statusBar()->currentMessage();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != firstBusEdgeRange
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-scroll")
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-scroll")}
                    || canvas->cursorTick() != 50'000
                    || canvas->horizontalScrollBar()->value() != 0
                    || !enumRangePalette->isVisibleTo(&window)
                    || !enumRangeContext->text().contains(QStringLiteral("1 Bus"))
                    || !enumRangeContext->toolTip().contains(
                        QStringLiteral("Ctrl+Shift+Left/Right selects to signal edges"))
                    || QApplication::activeModalWidget()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !firstBusEdgeStatus.contains(
                        QStringLiteral("Keyboard edge range on data[7:0]"))
                    || !firstBusEdgeStatus.contains(QStringLiteral("50 ns"))) {
                    fail(QStringLiteral(
                        "Ctrl+Shift+Right did not select from the cursor to the next Bus edge"));
                    return;
                }

                enumRangeValueEdit->setText(QStringLiteral("0xa5"));
                enumRangeValueEdit->setModified(false);
                enumRangeValueEdit->setCursorPosition(
                    enumRangeValueEdit->text().size());
                enumRangeValueEdit->setFocus(Qt::OtherFocusReason);
                sendKey(
                    enumRangeValueEdit,
                    Qt::Key_Left,
                    edgeRangeModifiers);
                QCoreApplication::processEvents();
                if (enumRangeValueEdit->selectedText().isEmpty()
                    || canvas->selectedTimeRange() != firstBusEdgeRange
                    || canvas->cursorTick() != 50'000
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Ctrl+Shift+Left escaped the range value field instead of selecting text"));
                    return;
                }
                enumRangeValueEdit->clear();
                enumRangeValueEdit->setModified(false);
                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);

                sendKey(canvas, Qt::Key_Right, edgeRangeModifiers);
                QCoreApplication::processEvents();
                const auto secondBusEdgeRange =
                    std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{0, 100'000}};
                if (canvas->selectedTimeRange() != secondBusEdgeRange
                    || canvas->cursorTick() != 100'000
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-scroll")}
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Repeated Ctrl+Shift+Right did not extend the Bus range to its next edge"));
                    return;
                }
                sendKey(canvas, Qt::Key_Left, edgeRangeModifiers);
                QCoreApplication::processEvents();
                if (canvas->selectedTimeRange() != firstBusEdgeRange
                    || canvas->cursorTick() != 50'000
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Ctrl+Shift+Left did not shrink the Bus range to its previous edge"));
                    return;
                }
                sendKey(canvas, Qt::Key_Left, edgeRangeModifiers);
                QCoreApplication::processEvents();
                const auto noEarlierBusEdgeStatus =
                    window.statusBar()->currentMessage();
                if (canvas->selectedTimeRange() != firstBusEdgeRange
                    || canvas->cursorTick() != 50'000
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !noEarlierBusEdgeStatus.contains(
                        QStringLiteral("No earlier edge on data[7:0]"))
                    || !noEarlierBusEdgeStatus.contains(
                        QStringLiteral("Shift+Home selects to timeline start"))) {
                    fail(QStringLiteral(
                        "Missing earlier Bus edge changed the range or omitted boundary guidance"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto edgeRangeScreenshotPath =
                        waveEditAutoScrollScreenshotPath;
                    const auto suffix = edgeRangeScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        edgeRangeScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-keyboard-edge-range"));
                    } else {
                        edgeRangeScreenshotPath.append(
                            QStringLiteral("-keyboard-edge-range.png"));
                    }
                    if (!window.grab().save(edgeRangeScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save keyboard edge-range screenshot"));
                        return;
                    }
                }
                sendKey(canvas, Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange()
                    || enumRangePalette->isVisibleTo(&window)
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Escape did not clear the keyboard edge range"));
                    return;
                }
                auto* keyboardRangeCopyButton = window.findChild<QToolButton*>(
                    QStringLiteral("RangeEditCopyButton"));
                auto* keyboardRangePasteButton = window.findChild<QToolButton*>(
                    QStringLiteral("RangeEditPasteButton"));
                if (!keyboardRangeCopyButton || !keyboardRangePasteButton) {
                    fail(QStringLiteral(
                        "Keyboard range Paste controls are missing"));
                    return;
                }

                sendKey(canvas, Qt::Key_Home);
                clickSignalHeader(laneY);
                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Right, Qt::ControlModifier);
                sendKey(canvas, Qt::Key_Right, edgeRangeModifiers);
                QCoreApplication::processEvents();
                const auto keyboardCopiedBusRange =
                    std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{50'000, 100'000}};
                if (canvas->selectedTimeRange() != keyboardCopiedBusRange
                    || canvas->cursorTick() != 100'000
                    || !keyboardRangeCopyButton->isVisibleTo(&window)
                    || !keyboardRangeCopyButton->isEnabled()) {
                    fail(QStringLiteral(
                        "Keyboard Paste source range was not selected at the Bus edges"));
                    return;
                }
                sendKey(canvas, Qt::Key_C, Qt::ControlModifier);
                QCoreApplication::processEvents();
                if (!window.statusBar()->currentMessage().contains(
                        QStringLiteral("Copied 1 lane(s), 50 ns"))
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Ctrl+C did not copy the selected Bus source range without editing"));
                    return;
                }

                sendKey(canvas, Qt::Key_Escape);
                sendKey(canvas, Qt::Key_Home);
                clickSignalHeader(laneY);
                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Right, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                const auto keyboardPasteTargetRange =
                    std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{0, 10'000}};
                const auto keyboardPasteTooltip =
                    keyboardRangePasteButton->toolTip();
                if (canvas->selectedTimeRange() != keyboardPasteTargetRange
                    || canvas->cursorTick() != 10'000
                    || !keyboardRangePasteButton->isVisibleTo(&window)
                    || !keyboardRangePasteButton->isEnabled()
                    || !keyboardPasteTooltip.contains(
                        QStringLiteral("selected targets at 0 ps"))) {
                    fail(QStringLiteral(
                        "Keyboard Paste target did not disclose the selected start independently of the active edge"));
                    return;
                }

                sendKey(canvas, Qt::Key_V, Qt::ControlModifier);
                QCoreApplication::processEvents();
                const auto keyboardPastedRange =
                    std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{0, 50'000}};
                const auto* keyboardPastedBus = wave::findLane(
                    scenario,
                    "lane-wave-edit-scroll");
                const auto keyboardPasteStatus =
                    window.statusBar()->currentMessage();
                if (enumValueAt(keyboardPastedBus, 5'000) != "0x35"
                    || canvas->selectedTimeRange() != keyboardPastedRange
                    || canvas->cursorTick() != 10'000
                    || scenario == originalScenario
                    || !undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Unsaved changes")
                    || !keyboardPasteStatus.contains(QStringLiteral("Pasted"))
                    || !keyboardPasteStatus.contains(QStringLiteral("at 0 ps"))
                    || !keyboardPasteStatus.contains(QStringLiteral("Ctrl+Z"))) {
                    fail(QStringLiteral(
                        "Ctrl+V used the keyboard range active edge instead of its disclosed start"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto keyboardPasteScreenshotPath =
                        waveEditAutoScrollScreenshotPath;
                    const auto suffix = keyboardPasteScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        keyboardPasteScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-keyboard-paste-start"));
                    } else {
                        keyboardPasteScreenshotPath.append(
                            QStringLiteral("-keyboard-paste-start.png"));
                    }
                    if (!window.grab().save(keyboardPasteScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save keyboard range Paste start screenshot"));
                        return;
                    }
                }
                undoAction->trigger();
                QCoreApplication::processEvents();
                if (scenario != originalScenario
                    || enumValueAt(
                           wave::findLane(scenario, "lane-wave-edit-scroll"),
                           5'000)
                        != std::string{}
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Undo did not restore the pre-Paste Bus and Saved state"));
                    return;
                }
                sendKey(canvas, Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange()
                    || enumRangePalette->isVisibleTo(&window)) {
                    fail(QStringLiteral(
                        "Escape did not clear the pasted keyboard range"));
                    return;
                }
                sendKey(canvas, Qt::Key_Home);
                const auto clockLaneY = 40
                    + scenario.lanes.front().height
                    + scenario.lanes.at(1).height
                    + scenario.lanes.at(2).height
                    + scenario.lanes.back().height / 2;
                clickSignalHeader(clockLaneY);
                canvas->setFocus(Qt::OtherFocusReason);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Right, edgeRangeModifiers);
                QCoreApplication::processEvents();
                const auto firstClockEdgeRange =
                    std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{0, 5'000}};
                const auto clockEdgeRangeStatus =
                    window.statusBar()->currentMessage();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != firstClockEdgeRange
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-clock")
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-wave-edit-clock")}
                    || canvas->cursorTick() != 5'000
                    || canvas->horizontalScrollBar()->value() != 0
                    || !enumRangePalette->isVisibleTo(&window)
                    || QApplication::activeModalWidget()
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !clockEdgeRangeStatus.contains(
                        QStringLiteral("Keyboard edge range on clk"))
                    || !clockEdgeRangeStatus.contains(QStringLiteral("5 ns"))) {
                    fail(QStringLiteral(
                        "Ctrl+Shift+Right did not select the first Clock half-cycle edge"));
                    return;
                }
                sendKey(canvas, Qt::Key_Escape);
                sendKey(canvas, Qt::Key_Home);
                clickSignalHeader(clockLaneY);
                sendKey(canvas, Qt::Key_Right, Qt::ControlModifier);
                if (canvas->cursorTick() != 5'000
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-clock")
                    || canvas->horizontalScrollBar()->value() != 0
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Next edge on clk"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("value 0"))) {
                    fail(QStringLiteral(
                        "Ctrl+Right did not jump to the Clock falling edge with value 0"));
                    return;
                }
                sendKey(canvas, Qt::Key_Right, Qt::ControlModifier);
                if (canvas->cursorTick() != 10'000
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-clock")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Next edge on clk"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("value 1"))) {
                    fail(QStringLiteral(
                        "Ctrl+Right did not jump to the next Clock rising edge with value 1"));
                    return;
                }
                sendKey(canvas, Qt::Key_Left, Qt::ControlModifier);
                if (canvas->cursorTick() != 5'000
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-wave-edit-clock")
                    || canvas->horizontalScrollBar()->value() != 0
                    || scenario != originalScenario
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Previous edge on clk"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("value 0"))) {
                    fail(QStringLiteral(
                        "Ctrl+Left did not return to the Clock falling edge with value 0"));
                    return;
                }
                if (!waveEditAutoScrollScreenshotPath.isEmpty()) {
                    auto edgeScreenshotPath = waveEditAutoScrollScreenshotPath;
                    const auto suffix = edgeScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        edgeScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-next-edge"));
                    } else {
                        edgeScreenshotPath.append(
                            QStringLiteral("-next-edge.png"));
                    }
                    if (!window.grab().save(edgeScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save adjacent signal edge navigation screenshot"));
                        return;
                    }
                }
                window.hide();
                application.exit(0);
            });
    } else if (laneAutoScrollSmoke) {
        QTimer::singleShot(
            0,
            &window,
            [&application, &window, laneAutoScrollScreenshotPath] {
                auto* canvas = window.findChild<wave::WaveCanvas*>();
                auto* undoAction = window.findChild<QAction*>(QStringLiteral("UndoAction"));
                auto* saveState = window.findChild<QLabel*>(QStringLiteral("SaveStateLabel"));
                auto* durationEdit = window.findChild<QLineEdit*>(
                    QStringLiteral("TimelineDurationEdit"));
                auto* findSignalAction = window.findChild<QAction*>(
                    QStringLiteral("FindSignalAction"));
                auto* findToolbarAction = window.findChild<QAction*>(
                    QStringLiteral("SignalFindToolbarAction"));
                auto* findBar = window.findChild<QFrame*>(
                    QStringLiteral("SignalFindBar"));
                auto* findEdit = window.findChild<QLineEdit*>(
                    QStringLiteral("SignalFindEdit"));
                auto* findResult = window.findChild<QLabel*>(
                    QStringLiteral("SignalFindResultLabel"));
                auto* findPrevious = window.findChild<QToolButton*>(
                    QStringLiteral("SignalFindPreviousButton"));
                auto* findNext = window.findChild<QToolButton*>(
                    QStringLiteral("SignalFindNextButton"));
                auto* findClose = window.findChild<QToolButton*>(
                    QStringLiteral("SignalFindCloseButton"));
                auto fail = [&application, &window](const QString& message) {
                    qCritical().noquote() << message;
                    window.hide();
                    application.exit(4);
                };
                if (!canvas || !undoAction || !saveState || !durationEdit
                    || !findSignalAction || !findToolbarAction || !findBar
                    || !findEdit || !findResult || !findPrevious || !findNext
                    || !findClose || window.project().scenarios.empty()) {
                    fail(QStringLiteral("Lane autoscroll smoke prerequisites are missing"));
                    return;
                }
                auto& scenario = window.project().scenarios.front();
                const auto originalLanes = scenario.lanes;
                if (originalLanes.size() != 20
                    || originalLanes.front().id != "lane-scroll-00"
                    || originalLanes.back().id != "lane-scroll-19"
                    || canvas->verticalScrollBar()->maximum() <= 0
                    || findSignalAction->shortcut().matches(QKeySequence::Find)
                        != QKeySequence::ExactMatch
                    || findToolbarAction->isVisible()
                    || findBar->isVisibleTo(&window)
                    || findEdit->placeholderText()
                        != QStringLiteral("Visible signal name or ID")
                    || findEdit->accessibleName()
                        != QStringLiteral("Find visible signal")
                    || findResult->text() != QStringLiteral("0/0")
                    || QApplication::activeModalWidget()
                    || saveState->text() != QStringLiteral("Saved")
                    || undoAction->isEnabled()) {
                    fail(QStringLiteral("Lane autoscroll smoke did not start from a long Saved list"));
                    return;
                }

                const auto sendMouse = [canvas](
                                           const QEvent::Type type,
                                           const QPoint position,
                                           const Qt::MouseButton button,
                                           const Qt::MouseButtons buttons) {
                    QMouseEvent event(
                        type,
                        QPointF(position),
                        QPointF(canvas->viewport()->mapToGlobal(position)),
                        button,
                        buttons,
                        Qt::NoModifier);
                    QCoreApplication::sendEvent(canvas->viewport(), &event);
                };
                const auto sendKey = [](
                                         QObject* target,
                                         const int key,
                                         const Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
                    QKeyEvent press(QEvent::KeyPress, key, modifiers);
                    QCoreApplication::sendEvent(target, &press);
                    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
                    QCoreApplication::sendEvent(target, &release);
                };
                const auto waitForScroll = [](const int milliseconds) {
                    QEventLoop loop;
                    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
                    loop.exec();
                    QCoreApplication::processEvents();
                };
                const auto laneCenter = [canvas, &scenario](const std::string& laneId) {
                    auto y = 40 - canvas->verticalScrollBar()->value();
                    for (const auto& lane : scenario.lanes) {
                        if (!lane.visible) continue;
                        const auto height = std::clamp(lane.height, 30, 240);
                        if (lane.id == laneId) return y + height / 2;
                        y += height;
                    }
                    return -1;
                };
                const auto beginEdgeDrag = [&sendMouse](
                                                   const int startY,
                                                   const int edgeY) {
                    sendMouse(
                        QEvent::MouseButtonPress,
                        QPoint(80, startY),
                        Qt::LeftButton,
                        Qt::LeftButton);
                    sendMouse(
                        QEvent::MouseMove,
                        QPoint(80, edgeY),
                        Qt::NoButton,
                        Qt::LeftButton);
                };
                const auto releaseEdgeDrag = [&sendMouse](const int edgeY) {
                    sendMouse(
                        QEvent::MouseButtonRelease,
                        QPoint(80, edgeY),
                        Qt::LeftButton,
                        Qt::NoButton);
                    QCoreApplication::processEvents();
                };

                canvas->verticalScrollBar()->setValue(0);
                canvas->setFocus(Qt::OtherFocusReason);
                QCoreApplication::processEvents();
                const auto bottomEdge = canvas->viewport()->height() - 3;
                auto firstY = laneCenter("lane-scroll-00");
                if (firstY < 40 || firstY >= canvas->viewport()->height()) {
                    fail(QStringLiteral("First signal is not visible for downward edge drag"));
                    return;
                }

                beginEdgeDrag(firstY, bottomEdge);
                waitForScroll(180);
                if (canvas->verticalScrollBar()->value() <= 0
                    || !canvas->laneDropDestinationIndex()
                    || canvas->viewport()->cursor().shape() != Qt::ClosedHandCursor) {
                    fail(QStringLiteral("Holding a lane at the lower edge did not start continuous scrolling"));
                    return;
                }
                sendKey(canvas, Qt::Key_Escape);
                waitForScroll(150);
                releaseEdgeDrag(bottomEdge);
                if (canvas->verticalScrollBar()->value() != 0
                    || laneCenter("lane-scroll-00") < 40
                    || canvas->laneDropDestinationIndex()
                    || scenario.lanes != originalLanes
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || canvas->viewport()->cursor().shape() != Qt::PointingHandCursor
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Move cancelled · signal_00 remains at position 1"))) {
                    fail(QStringLiteral("Escape did not stop edge scrolling without committing a move"));
                    return;
                }

                canvas->verticalScrollBar()->setValue(0);
                QCoreApplication::processEvents();
                firstY = laneCenter("lane-scroll-00");
                beginEdgeDrag(firstY, bottomEdge);
                waitForScroll(700);
                if (canvas->verticalScrollBar()->value()
                        != canvas->verticalScrollBar()->maximum()
                    || canvas->laneDropDestinationIndex()
                        != std::optional<std::size_t>{19}) {
                    fail(QStringLiteral("Downward edge drag did not reach the real end of the signal list"));
                    return;
                }
                if (!laneAutoScrollScreenshotPath.isEmpty()
                    && !window.grab().save(laneAutoScrollScreenshotPath)) {
                    fail(QStringLiteral("Cannot save lane autoscroll smoke screenshot"));
                    return;
                }
                releaseEdgeDrag(bottomEdge);
                if (scenario.lanes.back().id != "lane-scroll-00"
                    || !undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Unsaved changes")
                    || canvas->viewport()->cursor().shape() != Qt::PointingHandCursor
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Moved signal_00: position 1 -> 20"))
                    || !window.statusBar()->currentMessage().contains(QStringLiteral("Ctrl+Z"))) {
                    fail(QStringLiteral("Cross-viewport downward drop did not commit one clear move"));
                    return;
                }
                undoAction->trigger();
                QCoreApplication::processEvents();
                if (scenario.lanes != originalLanes
                    || saveState->text() != QStringLiteral("Saved")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Undid Move lane"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("back to saved version"))) {
                    fail(QStringLiteral("Undo did not restore the downward cross-viewport move"));
                    return;
                }

                canvas->verticalScrollBar()->setValue(
                    canvas->verticalScrollBar()->maximum());
                QCoreApplication::processEvents();
                const auto lastY = laneCenter("lane-scroll-19");
                const auto topEdge = 42;
                if (lastY < 40 || lastY >= canvas->viewport()->height()) {
                    fail(QStringLiteral("Last signal is not visible for upward edge drag"));
                    return;
                }
                beginEdgeDrag(lastY, topEdge);
                waitForScroll(700);
                if (canvas->verticalScrollBar()->value() != 0
                    || canvas->laneDropDestinationIndex()
                        != std::optional<std::size_t>{0}) {
                    fail(QStringLiteral("Upward edge drag did not reach the real start of the signal list"));
                    return;
                }
                releaseEdgeDrag(topEdge);
                if (scenario.lanes.front().id != "lane-scroll-19"
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Moved signal_19: position 20 -> 1"))) {
                    fail(QStringLiteral("Cross-viewport upward drop committed the wrong signal order"));
                    return;
                }
                const auto stoppedAt = canvas->verticalScrollBar()->value();
                waitForScroll(120);
                if (canvas->verticalScrollBar()->value() != stoppedAt) {
                    fail(QStringLiteral("Lane edge scrolling continued after mouse release"));
                    return;
                }
                undoAction->trigger();
                QCoreApplication::processEvents();
                if (scenario.lanes != originalLanes
                    || saveState->text() != QStringLiteral("Saved")
                    || window.windowTitle().contains(QStringLiteral(" *"))) {
                    fail(QStringLiteral("Final Undo did not return to the exact Saved lane order"));
                    return;
                }

                const auto originalHorizontalScroll =
                    canvas->horizontalScrollBar()->value();
                sendKey(canvas, Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (!canvas->selectedLaneId().isEmpty()
                    || canvas->verticalScrollBar()->value() != 0
                    || scenario.lanes != originalLanes
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Keyboard signal navigation did not start from a clean selection"));
                    return;
                }

                durationEdit->setFocus(Qt::OtherFocusReason);
                sendKey(durationEdit, Qt::Key_Down);
                QCoreApplication::processEvents();
                if (!canvas->selectedLaneId().isEmpty()
                    || canvas->verticalScrollBar()->value() != 0
                    || canvas->horizontalScrollBar()->value()
                        != originalHorizontalScroll
                    || scenario.lanes != originalLanes
                    || undoAction->isEnabled()) {
                    fail(QStringLiteral(
                        "Signal Down intercepted the timeline End text field"));
                    return;
                }

                canvas->setFocus(Qt::OtherFocusReason);
                sendKey(canvas, Qt::Key_Down);
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-00")
                    || canvas->verticalScrollBar()->value() != 0
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("1 of 19"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("value 0"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Up/Down selects signals"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Left/Right jumps edges"))) {
                    fail(QStringLiteral(
                        "Down without a selection did not select the first visible signal"));
                    return;
                }
                sendKey(canvas, Qt::Key_Up);
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-00")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("No signal above signal_00"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Down selects the next signal"))) {
                    fail(QStringLiteral(
                        "Up at the first signal did not keep the target and explain the boundary"));
                    return;
                }

                for (auto index = 0; index < 5; ++index) {
                    sendKey(canvas, Qt::Key_Down);
                }
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-06")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("6 of 19"))) {
                    fail(QStringLiteral(
                        "Down did not skip the Group while preserving visible-signal order"));
                    return;
                }
                for (auto index = 0; index < 13; ++index) {
                    sendKey(canvas, Qt::Key_Down);
                }
                QCoreApplication::processEvents();
                const auto lastSignalScroll =
                    canvas->verticalScrollBar()->value();
                const auto lastSignalY = laneCenter("lane-scroll-19");
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-19")
                    || lastSignalScroll <= 0
                    || lastSignalScroll >= canvas->verticalScrollBar()->maximum()
                    || lastSignalY < 40
                    || lastSignalY >= canvas->viewport()->height()
                    || canvas->horizontalScrollBar()->value()
                        != originalHorizontalScroll
                    || scenario.lanes != originalLanes
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("19 of 19"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("value 0"))) {
                    fail(QStringLiteral(
                        "Repeated Down did not minimally reveal the last visible signal"));
                    return;
                }
                if (!laneAutoScrollScreenshotPath.isEmpty()) {
                    auto navigationScreenshotPath = laneAutoScrollScreenshotPath;
                    const auto suffix = navigationScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        navigationScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-keyboard-navigation"));
                    } else {
                        navigationScreenshotPath.append(
                            QStringLiteral("-keyboard-navigation.png"));
                    }
                    if (!window.grab().save(navigationScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save keyboard signal navigation screenshot"));
                        return;
                    }
                }

                sendKey(canvas, Qt::Key_Down);
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-19")
                    || canvas->verticalScrollBar()->value() != lastSignalScroll
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("No signal below signal_19"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Up selects the previous signal"))) {
                    fail(QStringLiteral(
                        "Down at the last signal did not keep the target and explain the boundary"));
                    return;
                }
                sendKey(canvas, Qt::Key_Up);
                sendKey(canvas, Qt::Key_Delete);
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-18")
                    || scenario.lanes != originalLanes
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || window.windowTitle().contains(QStringLiteral(" *"))) {
                    fail(QStringLiteral(
                        "Keyboard signal navigation armed an unintended signal deletion"));
                    return;
                }

                sendKey(canvas, Qt::Key_Right, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                if (!canvas->hasExplicitRangeSelection()
                    || findToolbarAction->isVisible()
                    || scenario.lanes != originalLanes
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Signal search range guard did not start from a read-only explicit range"));
                    return;
                }
                sendKey(canvas, Qt::Key_F, Qt::ControlModifier);
                QCoreApplication::processEvents();
                if (findToolbarAction->isVisible()
                    || !canvas->hasExplicitRangeSelection()
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Esc clears the selected range"))
                    || scenario.lanes != originalLanes
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Ctrl+F discarded an explicit range instead of explaining how to continue"));
                    return;
                }
                sendKey(canvas, Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (canvas->hasExplicitRangeSelection()
                    || scenario.lanes != originalLanes
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(
                        QStringLiteral(
                            "Escape did not safely clear the range before signal search: range=%1 lane=%2 lanesSame=%3 undo=%4 save=%5")
                            .arg(canvas->hasExplicitRangeSelection())
                            .arg(canvas->selectedLaneId())
                            .arg(scenario.lanes == originalLanes)
                            .arg(undoAction->isEnabled())
                            .arg(saveState->text()));
                    return;
                }
                const auto selectedBeforeFind = canvas->selectedLaneId();

                sendKey(canvas, Qt::Key_F, Qt::ControlModifier);
                QCoreApplication::processEvents();
                if (!findToolbarAction->isVisible()
                    || !findBar->isVisibleTo(&window)
                    || !findEdit->hasFocus()
                    || !findEdit->text().isEmpty()
                    || findResult->text() != QStringLiteral("0/0")
                    || findPrevious->isEnabled()
                    || findNext->isEnabled()
                    || canvas->selectedLaneId() != selectedBeforeFind
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Find visible signal"))
                    || QApplication::activeModalWidget()) {
                    fail(QStringLiteral(
                        "Ctrl+F did not open an empty non-modal signal search"));
                    return;
                }

                findEdit->setText(QStringLiteral("signal_0"));
                QCoreApplication::processEvents();
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-00")
                    || findResult->text() != QStringLiteral("1/9")
                    || !findPrevious->isEnabled()
                    || !findNext->isEnabled()
                    || canvas->verticalScrollBar()->value() != 0
                    || canvas->horizontalScrollBar()->value()
                        != originalHorizontalScroll
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Found signal signal_00"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("1 of 9"))) {
                    fail(QStringLiteral(
                        "Typing a signal query did not select the first visible match"));
                    return;
                }

                findNext->click();
                QCoreApplication::processEvents();
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-01")
                    || findResult->text() != QStringLiteral("2/9")) {
                    fail(QStringLiteral(
                        "Signal search Next button did not advance in display order"));
                    return;
                }
                sendKey(findEdit, Qt::Key_Return);
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-02")
                    || findResult->text() != QStringLiteral("3/9")) {
                    fail(QStringLiteral(
                        "Enter did not advance to the next signal match"));
                    return;
                }
                sendKey(findEdit, Qt::Key_Return, Qt::ShiftModifier);
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-01")
                    || findResult->text() != QStringLiteral("2/9")) {
                    fail(QStringLiteral(
                        "Shift+Enter did not return to the previous signal match"));
                    return;
                }
                findPrevious->click();
                sendKey(findEdit, Qt::Key_Return, Qt::ShiftModifier);
                QCoreApplication::processEvents();
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-09")
                    || findResult->text() != QStringLiteral("9/9")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("wrapped"))) {
                    fail(QStringLiteral(
                        "Previous signal search did not wrap while skipping the Group"));
                    return;
                }

                findEdit->setText(QStringLiteral("group_05"));
                QCoreApplication::processEvents();
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-09")
                    || findResult->text() != QStringLiteral("0/0")
                    || findPrevious->isEnabled()
                    || findNext->isEnabled()
                    || findEdit->styleSheet().isEmpty()
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("No visible signal matches"))) {
                    fail(QStringLiteral(
                        "Signal search treated a Group as a selectable match or hid no-result feedback"));
                    return;
                }

                findEdit->setText(QStringLiteral("LANE-SCROLL-19"));
                QCoreApplication::processEvents();
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-19")
                    || findResult->text() != QStringLiteral("1/1")
                    || !findEdit->styleSheet().isEmpty()
                    || !findPrevious->isEnabled()
                    || !findNext->isEnabled()
                    || canvas->verticalScrollBar()->value() <= 0
                    || canvas->horizontalScrollBar()->value()
                        != originalHorizontalScroll) {
                    fail(QStringLiteral(
                        "Case-insensitive signal ID search did not reveal the exact match"));
                    return;
                }

                findEdit->setText(QStringLiteral("signal_1"));
                QCoreApplication::processEvents();
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-10")
                    || findResult->text() != QStringLiteral("1/10")) {
                    fail(QStringLiteral(
                        "Changing the query did not restart at its first signal match"));
                    return;
                }
                sendKey(findEdit, Qt::Key_Return, Qt::ShiftModifier);
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-19")
                    || findResult->text() != QStringLiteral("10/10")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("wrapped"))) {
                    fail(QStringLiteral(
                        "Shift+Enter did not wrap to the final signal match"));
                    return;
                }
                sendKey(findEdit, Qt::Key_Return);
                QCoreApplication::processEvents();
                if (canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-10")
                    || findResult->text() != QStringLiteral("1/10")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("wrapped"))
                    || scenario.lanes != originalLanes
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")
                    || window.windowTitle().contains(QStringLiteral(" *"))
                    || QApplication::activeModalWidget()) {
                    fail(QStringLiteral(
                        "Signal search changed the model or failed to wrap forward"));
                    return;
                }
                if (!laneAutoScrollScreenshotPath.isEmpty()) {
                    auto findScreenshotPath = laneAutoScrollScreenshotPath;
                    const auto suffix = findScreenshotPath.lastIndexOf(
                        QLatin1Char('.'));
                    if (suffix >= 0) {
                        findScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-signal-find"));
                    } else {
                        findScreenshotPath.append(
                            QStringLiteral("-signal-find.png"));
                    }
                    if (!window.grab().save(findScreenshotPath)) {
                        fail(QStringLiteral(
                            "Cannot save signal search screenshot"));
                        return;
                    }
                }

                sendKey(findEdit, Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (findToolbarAction->isVisible()
                    || findBar->isVisibleTo(&window)
                    || !canvas->viewport()->hasFocus()
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-10")
                    || findEdit->text() != QStringLiteral("signal_1")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("signal_10 remains selected"))
                    || canvas->horizontalScrollBar()->value()
                        != originalHorizontalScroll
                    || scenario.lanes != originalLanes
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Escape did not close signal search while retaining its result safely"));
                    return;
                }

                sendKey(canvas, Qt::Key_F, Qt::ControlModifier);
                QCoreApplication::processEvents();
                if (!findToolbarAction->isVisible()
                    || !findEdit->hasFocus()
                    || findEdit->selectedText() != QStringLiteral("signal_1")) {
                    fail(QStringLiteral(
                        "Ctrl+F did not reopen signal search with the prior query selected"));
                    return;
                }
                findClose->click();
                QCoreApplication::processEvents();
                if (findToolbarAction->isVisible()
                    || canvas->selectedLaneId()
                        != QStringLiteral("lane-scroll-10")
                    || scenario.lanes != originalLanes
                    || undoAction->isEnabled()
                    || saveState->text() != QStringLiteral("Saved")) {
                    fail(QStringLiteral(
                        "Signal search close button did not preserve the selected match and Saved state"));
                    return;
                }
                window.hide();
                application.exit(0);
            });
    } else if (laneReorderSmoke) {
        QTimer::singleShot(0, &window, [&application, &window] {
            window.revealLocation(QStringLiteral("lane-request"), 80'000);
            const auto originalLanes = window.project().scenarios.front().lanes;
            QTimer::singleShot(0, &application, [&application, &window, originalLanes] {
                const auto invoke = [&window](const char* method) {
                    return QMetaObject::invokeMethod(
                        &window,
                        method,
                        Qt::DirectConnection);
                };
                if (!invoke("moveSelectedLaneUp")) {
                    qCritical().noquote() << "Cannot trigger lane reorder up smoke";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto& movedUp = window.project().scenarios.front().lanes;
                const auto expectedUpStatus = QStringLiteral(
                    "Moved %1: position 4 -> 3. Ctrl+Z to undo.")
                                                  .arg(QString::fromStdString(
                                                      originalLanes.at(3).name));
                if (movedUp.size() != originalLanes.size()
                    || movedUp.at(2).id != "lane-request"
                    || window.statusBar()->currentMessage() != expectedUpStatus) {
                    qCritical().noquote()
                        << "Lane reorder up produced the wrong order or feedback"
                        << window.statusBar()->currentMessage();
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!invoke("undo")
                    || window.project().scenarios.front().lanes != originalLanes
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Undid Move lane"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Y"))) {
                    qCritical().noquote()
                        << "Lane reorder up undo or recovery feedback failed"
                        << window.statusBar()->currentMessage();
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!invoke("moveSelectedLaneDown")) {
                    qCritical().noquote() << "Cannot trigger lane reorder down smoke";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto& movedDown = window.project().scenarios.front().lanes;
                const auto expectedDownStatus = QStringLiteral(
                    "Moved %1: position 4 -> 5. Ctrl+Z to undo.")
                                                    .arg(QString::fromStdString(
                                                        originalLanes.at(3).name));
                if (movedDown.size() != originalLanes.size()
                    || movedDown.at(4).id != "lane-request"
                    || window.statusBar()->currentMessage() != expectedDownStatus) {
                    qCritical().noquote()
                        << "Lane reorder down produced the wrong order or feedback"
                        << window.statusBar()->currentMessage();
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!invoke("undo")
                    || window.project().scenarios.front().lanes != originalLanes
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Undid Move lane"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Y"))) {
                    qCritical().noquote()
                        << "Lane reorder down undo or recovery feedback failed"
                        << window.statusBar()->currentMessage();
                    window.hide();
                    application.exit(4);
                    return;
                }
                window.hide();
                application.exit(0);
            });
        });
    } else if (hiddenLaneSmoke) {
        QTimer::singleShot(
            0, &window, [&application, &window, hiddenLaneScreenshotPath] {
            auto fail = [&application, &window](const QString& message) {
                qCritical().noquote() << message;
                if (auto* modal = QApplication::activeModalWidget()) modal->close();
                window.hide();
                application.exit(4);
            };
            auto* canvas = window.findChild<wave::WaveCanvas*>();
            auto* showButton = window.findChild<QToolButton*>(
                QStringLiteral("CanvasShowHiddenLanesButton"));
            auto* showAction = window.findChild<QAction*>(
                QStringLiteral("ShowHiddenLanesAction"));
            auto* hideAction = window.findChild<QAction*>(
                QStringLiteral("HideLaneAction"));
            auto* undoAction = window.findChild<QAction*>(QStringLiteral("UndoAction"));
            auto* redoAction = window.findChild<QAction*>(QStringLiteral("RedoAction"));
            auto* saveState = window.findChild<QLabel*>(QStringLiteral("SaveStateLabel"));
            const auto* initialLane = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            const auto* initialGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!canvas || !showButton || !showAction || !hideAction || !undoAction || !redoAction
                || !saveState || !initialLane || !initialLane->visible
                || !initialGroup || initialGroup->visible
                || !showButton->isVisible() || !showAction->isVisible()
                || showButton->text() != QStringLiteral("Show 1 hidden item")
                || showAction->text() != QStringLiteral("Show 1 hidden item")
                || saveState->text() != QStringLiteral("Saved")) {
                qCritical().noquote()
                    << "Hidden-item start diagnostic: canvas=" << (canvas != nullptr)
                    << "button=" << (showButton != nullptr)
                    << "action=" << (showAction != nullptr)
                    << "saveState=" << (saveState ? saveState->text() : QStringLiteral("<missing>"))
                    << "laneVisible=" << (initialLane && initialLane->visible)
                    << "groupHidden=" << (initialGroup && !initialGroup->visible)
                    << "buttonText=" << (showButton ? showButton->text() : QStringLiteral("<missing>"))
                    << "actionText=" << (showAction ? showAction->text() : QStringLiteral("<missing>"));
                fail(QStringLiteral(
                    "Existing hidden project data did not expose a recovery entry on load"));
                return;
            }

            const auto sendKey = [](QObject* target,
                                    const int key,
                                    const Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
                QKeyEvent press(QEvent::KeyPress, key, modifiers);
                QCoreApplication::sendEvent(target, &press);
                QKeyEvent release(QEvent::KeyRelease, key, modifiers);
                QCoreApplication::sendEvent(target, &release);
            };
            const auto sendMouse = [canvas](
                                       const QEvent::Type type,
                                       const QPoint position,
                                       const Qt::MouseButton button,
                                       const Qt::MouseButtons buttons) {
                QMouseEvent event(
                    type,
                    QPointF(position),
                    QPointF(canvas->viewport()->mapToGlobal(position)),
                    button,
                    buttons,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(canvas->viewport(), &event);
            };
            const auto clickHeader = [&sendMouse](const QPoint position) {
                sendMouse(
                    QEvent::MouseButtonPress,
                    position,
                    Qt::LeftButton,
                    Qt::LeftButton);
                sendMouse(
                    QEvent::MouseButtonRelease,
                    position,
                    Qt::LeftButton,
                    Qt::NoButton);
            };
            const auto laneCenter = [canvas, &window](const std::string& laneId) {
                auto y = 40 - canvas->verticalScrollBar()->value();
                for (const auto& lane : window.project().scenarios.front().lanes) {
                    if (!lane.visible) continue;
                    const auto height = std::clamp(lane.height, 30, 240);
                    if (lane.id == laneId) return y + height / 2;
                    y += height;
                }
                return -1;
            };

            canvas->verticalScrollBar()->setValue(0);
            QCoreApplication::processEvents();
            auto requestY = laneCenter("lane-request");
            if (requestY < 40 || requestY >= canvas->viewport()->height()) {
                fail(QStringLiteral("The request signal is outside the visible canvas"));
                return;
            }
            canvas->setFocus(Qt::OtherFocusReason);
            clickHeader(QPoint(80, requestY));
            QCoreApplication::processEvents();
            if (canvas->selectedLaneId() != QStringLiteral("lane-request")
                || !hideAction->isEnabled()
                || !hideAction->text().contains(QStringLiteral("Hide selected signal"))) {
                fail(QStringLiteral("Selecting a signal did not expose the Edit hide action"));
                return;
            }

            sendKey(canvas, Qt::Key_Right, Qt::ShiftModifier);
            QCoreApplication::processEvents();
            hideAction->trigger();
            QCoreApplication::processEvents();
            const auto* rangeGuardLane = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            if (!canvas->hasExplicitRangeSelection()
                || !rangeGuardLane || !rangeGuardLane->visible
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Esc clears"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("before hiding a whole item"))) {
                fail(QStringLiteral("Hide action discarded an explicit range instead of explaining recovery"));
                return;
            }
            sendKey(canvas, Qt::Key_Escape);
            QCoreApplication::processEvents();
            requestY = laneCenter("lane-request");
            clickHeader(QPoint(80, requestY));
            QCoreApplication::processEvents();

            hideAction->trigger();
            QCoreApplication::processEvents();
            const auto* hiddenByEdit = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            if (!hiddenByEdit || hiddenByEdit->visible
                || !canvas->selectedLaneId().isEmpty()
                || hideAction->isEnabled()
                || !showButton->isVisible()
                || !showButton->geometry().intersects(canvas->viewport()->rect())
                || showButton->text() != QStringLiteral("Show 2 hidden items")
                || !showAction->isVisible()
                || showAction->text() != QStringLiteral("Show 2 hidden items")
                || saveState->text() != QStringLiteral("Unsaved changes")
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Hidden signal req"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Show 2 hidden items"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Z"))
                || !undoAction->text().contains(QStringLiteral("Hide lane"))
                || QApplication::activeModalWidget()) {
                fail(QStringLiteral("Edit hide did not remove one signal with clear recovery feedback"));
                return;
            }
            if (!hiddenLaneScreenshotPath.isEmpty()
                && !window.grab().save(hiddenLaneScreenshotPath)) {
                fail(QStringLiteral("Cannot save quick-hide screenshot"));
                return;
            }

            undoAction->trigger();
            QCoreApplication::processEvents();
            const auto* restoredAfterEditUndo = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            const auto* baselineHiddenGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!restoredAfterEditUndo || !restoredAfterEditUndo->visible
                || !baselineHiddenGroup || baselineHiddenGroup->visible
                || !showButton->isVisible()
                || showButton->text() != QStringLiteral("Show 1 hidden item")
                || saveState->text() != QStringLiteral("Saved")
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Undid Hide lane"))) {
                fail(QStringLiteral("One Undo did not restore the signal and saved baseline"));
                return;
            }

            requestY = laneCenter("lane-request");
            clickHeader(QPoint(80, requestY));
            QCoreApplication::processEvents();
            bool contextHideHandled = false;
            QTimer::singleShot(
                0,
                &application,
                [&contextHideHandled] {
                    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                    auto* hide = menu
                        ? menu->findChild<QAction*>(
                              QStringLiteral("HideLaneContextAction"))
                        : nullptr;
                    if (!menu
                        || menu->objectName() != QStringLiteral("LaneHeaderContextMenu")
                        || !hide
                        || hide->text() != QStringLiteral("Hide signal")) {
                        if (menu) menu->close();
                        return;
                    }
                    contextHideHandled = true;
                    hide->trigger();
                    menu->close();
                });
            const QPoint contextPoint(80, requestY);
            QContextMenuEvent contextEvent(
                QContextMenuEvent::Mouse,
                contextPoint,
                canvas->viewport()->mapToGlobal(contextPoint));
            QCoreApplication::sendEvent(canvas->viewport(), &contextEvent);
            QCoreApplication::processEvents();
            const auto* hiddenLane = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            if (!contextHideHandled || !hiddenLane || hiddenLane->visible
                || !canvas->selectedLaneId().isEmpty()
                || !showButton->isVisible()
                || showButton->text() != QStringLiteral("Show 2 hidden items")
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Hidden signal req"))
                || QApplication::activeModalWidget()) {
                fail(QStringLiteral("Signal header context action did not hide one signal without a dialog"));
                return;
            }
            auto* hiddenMenu = showButton->menu();
            QAction* requestRestoreAction = nullptr;
            QAction* groupRestoreAction = nullptr;
            QAction* showAllMenuAction = nullptr;
            if (hiddenMenu) {
                for (auto* action : hiddenMenu->actions()) {
                    if (!action) continue;
                    if (action->objectName()
                        == QStringLiteral("ShowAllHiddenLanesMenuAction")) {
                        showAllMenuAction = action;
                    } else if (action->objectName()
                               == QStringLiteral("ShowHiddenLaneAction")) {
                        if (action->data().toString()
                            == QStringLiteral("lane-request")) {
                            requestRestoreAction = action;
                        } else if (action->data().toString()
                                   == QStringLiteral("group-handshake")) {
                            groupRestoreAction = action;
                        }
                    }
                }
            }
            if (!hiddenMenu
                || hiddenMenu->objectName() != QStringLiteral("HiddenLanesMenu")
                || showButton->popupMode() != QToolButton::MenuButtonPopup
                || !showButton->toolTip().contains(QStringLiteral("arrow"))
                || !requestRestoreAction
                || requestRestoreAction->text() != QStringLiteral("Show signal req")
                || !groupRestoreAction
                || groupRestoreAction->text()
                    != QStringLiteral("Show group Handshake signals")
                || !showAllMenuAction
                || showAllMenuAction->text()
                    != QStringLiteral("Show all 2 hidden items")) {
                fail(QStringLiteral("Multiple hidden items did not expose one-item and show-all choices on the same button"));
                return;
            }

            const auto resetY = laneCenter("lane-reset");
            if (resetY < 40 || resetY >= canvas->viewport()->height()) {
                fail(QStringLiteral("The reset signal is outside the visible canvas"));
                return;
            }
            clickHeader(QPoint(80, resetY));
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_Right, Qt::ShiftModifier);
            QCoreApplication::processEvents();
            if (!canvas->hasExplicitRangeSelection()) {
                fail(QStringLiteral("Cannot establish a range before selective restore"));
                return;
            }
            const auto horizontalBeforeRestore =
                canvas->horizontalScrollBar()->value();
            const auto horizontalMaximumBeforeRestore =
                canvas->horizontalScrollBar()->maximum();
            bool restoreMenuOpened = false;
            bool restoreMenuScreenshotSaved = hiddenLaneScreenshotPath.isEmpty();
            QTimer::singleShot(
                0,
                &application,
                [&] {
                    auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                    restoreMenuOpened = popup == hiddenMenu
                        && hiddenMenu->isVisible();
                    if (restoreMenuOpened && !hiddenLaneScreenshotPath.isEmpty()) {
                        auto menuScreenshotPath = hiddenLaneScreenshotPath;
                        const auto suffix = menuScreenshotPath.lastIndexOf(
                            QLatin1Char('.'));
                        if (suffix >= 0) {
                            menuScreenshotPath.insert(
                                suffix,
                                QStringLiteral("-restore-menu"));
                        } else {
                            menuScreenshotPath.append(
                                QStringLiteral("-restore-menu.png"));
                        }
                        restoreMenuScreenshotSaved =
                            hiddenMenu->grab().save(menuScreenshotPath);
                    }
                    if (restoreMenuOpened) requestRestoreAction->trigger();
                    if (popup) popup->close();
                });
            showButton->showMenu();
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            if (!restoreMenuOpened || !restoreMenuScreenshotSaved) {
                fail(QStringLiteral("The hidden-item arrow did not open and render its named restore menu"));
                return;
            }
            const auto* rangeGuardHiddenLane = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            if (!rangeGuardHiddenLane || rangeGuardHiddenLane->visible
                || !canvas->hasExplicitRangeSelection()
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Esc clears"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("before restoring and selecting"))) {
                fail(QStringLiteral("Selective restore discarded an explicit range instead of explaining recovery"));
                return;
            }
            sendKey(canvas, Qt::Key_Escape);
            QCoreApplication::processEvents();

            hiddenMenu = showButton->menu();
            requestRestoreAction = nullptr;
            if (hiddenMenu) {
                for (auto* action : hiddenMenu->actions()) {
                    if (action
                        && action->objectName()
                            == QStringLiteral("ShowHiddenLaneAction")
                        && action->data().toString()
                            == QStringLiteral("lane-request")) {
                        requestRestoreAction = action;
                        break;
                    }
                }
            }
            if (!hiddenMenu || !requestRestoreAction) {
                fail(QStringLiteral("Selective restore choice disappeared after range recovery"));
                return;
            }
            bool selectiveMenuOpened = false;
            QTimer::singleShot(
                0,
                &application,
                [&] {
                    auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                    selectiveMenuOpened = popup == hiddenMenu
                        && hiddenMenu->isVisible();
                    if (selectiveMenuOpened) requestRestoreAction->trigger();
                    if (popup) popup->close();
                });
            showButton->showMenu();
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            if (!selectiveMenuOpened) {
                fail(QStringLiteral("The hidden-item arrow did not reopen after range recovery"));
                return;
            }
            const auto* selectivelyRestoredLane = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            const auto* stillHiddenGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!selectivelyRestoredLane || !selectivelyRestoredLane->visible
                || !stillHiddenGroup || stillHiddenGroup->visible
                || canvas->selectedLaneId() != QStringLiteral("lane-request")
                || !showButton->isVisible()
                || showButton->text() != QStringLiteral("Show 1 hidden item")
                || showButton->menu()
                || showButton->popupMode() != QToolButton::DelayedPopup
                || !showAction->isVisible()
                || showAction->text() != QStringLiteral("Show 1 hidden item")
                || canvas->horizontalScrollBar()->value()
                    != horizontalBeforeRestore
                || canvas->horizontalScrollBar()->maximum()
                    != horizontalMaximumBeforeRestore
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Restored signal req in its original position"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("selected"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("1 hidden item remains"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Z"))
                || !undoAction->text().contains(QStringLiteral("Show lane"))
                || QApplication::activeModalWidget()) {
                fail(QStringLiteral("Named restore did not reveal only req in place with stable view and feedback"));
                return;
            }
            if (!hiddenLaneScreenshotPath.isEmpty()) {
                auto restoredScreenshotPath = hiddenLaneScreenshotPath;
                const auto suffix = restoredScreenshotPath.lastIndexOf(QLatin1Char('.'));
                if (suffix >= 0) {
                    restoredScreenshotPath.insert(
                        suffix,
                        QStringLiteral("-single-restore"));
                } else {
                    restoredScreenshotPath.append(
                        QStringLiteral("-single-restore.png"));
                }
                if (!window.grab().save(restoredScreenshotPath)) {
                    fail(QStringLiteral("Cannot save selective restore screenshot"));
                    return;
                }
            }

            undoAction->trigger();
            QCoreApplication::processEvents();
            const auto* hiddenAfterSelectiveUndo = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            if (!hiddenAfterSelectiveUndo || hiddenAfterSelectiveUndo->visible
                || !showButton->menu()
                || showButton->text() != QStringLiteral("Show 2 hidden items")
                || canvas->horizontalScrollBar()->value()
                    != horizontalBeforeRestore
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Undid Show lane"))) {
                fail(QStringLiteral("Selective restore Undo did not return to two hidden items"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();
            const auto* visibleAfterSelectiveRedo = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            if (!visibleAfterSelectiveRedo || !visibleAfterSelectiveRedo->visible
                || showButton->text() != QStringLiteral("Show 1 hidden item")
                || canvas->horizontalScrollBar()->value()
                    != horizontalBeforeRestore
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Redid Show lane"))) {
                fail(QStringLiteral("Selective restore Redo did not restore only req"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (wave::findLane(
                    window.project().scenarios.front(),
                    "lane-request")->visible
                || showButton->text() != QStringLiteral("Show 2 hidden items")) {
                fail(QStringLiteral("Second selective restore Undo did not prepare show-all baseline"));
                return;
            }
            showButton->click();
            QCoreApplication::processEvents();
            const auto* restoredLane = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            const auto* restoredGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!restoredLane || !restoredLane->visible
                || !restoredGroup || !restoredGroup->visible
                || showButton->isVisible() || showAction->isVisible()
                || saveState->text() != QStringLiteral("Unsaved changes")
                || !window.statusBar()->currentMessage().startsWith(
                    QStringLiteral("Restored 2 hidden items"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Z"))) {
                fail(QStringLiteral(
                    "Show hidden items did not restore every item with clear undo feedback"));
                return;
            }

            undoAction->trigger();
            QCoreApplication::processEvents();
            const auto* hiddenAfterUndo = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            const auto* groupAfterUndo = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!hiddenAfterUndo || hiddenAfterUndo->visible
                || !groupAfterUndo || groupAfterUndo->visible
                || !showButton->isVisible() || !showAction->isVisible()
                || showButton->text() != QStringLiteral("Show 2 hidden items")
                || !window.statusBar()->currentMessage().startsWith(
                    QStringLiteral("Undid Show hidden items"))) {
                fail(QStringLiteral(
                    "Undo did not atomically restore all hidden items and the recovery entry"));
                return;
            }

            redoAction->trigger();
            QCoreApplication::processEvents();
            const auto* visibleAfterRedo = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            const auto* groupAfterRedo = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!visibleAfterRedo || !visibleAfterRedo->visible
                || !groupAfterRedo || !groupAfterRedo->visible
                || showButton->isVisible() || showAction->isVisible()
                || !window.statusBar()->currentMessage().startsWith(
                    QStringLiteral("Redid Show hidden items"))) {
                fail(QStringLiteral(
                    "Redo did not restore every hidden item and remove stale recovery entries"));
                return;
            }

            undoAction->trigger();
            undoAction->trigger();
            QCoreApplication::processEvents();
            const auto* baselineLane = wave::findLane(
                window.project().scenarios.front(),
                "lane-request");
            const auto* baselineGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!baselineLane || !baselineLane->visible
                || !baselineGroup || baselineGroup->visible
                || !showButton->isVisible() || !showAction->isVisible()
                || showButton->text() != QStringLiteral("Show 1 hidden item")
                || saveState->text() != QStringLiteral("Saved")
                || window.windowTitle().contains(QStringLiteral(" *"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("back to saved version"))) {
                fail(QStringLiteral(
                    "Undoing hide and restore did not return to the exact Saved baseline"));
                return;
            }

            window.hide();
            application.exit(0);
        });
    } else if (groupHeaderSmoke) {
        QTimer::singleShot(0, &window, [&application, &window] {
            auto* canvas = window.findChild<wave::WaveCanvas*>();
            auto* showButton = window.findChild<QToolButton*>(
                QStringLiteral("CanvasShowHiddenLanesButton"));
            auto* hideAction = window.findChild<QAction*>(
                QStringLiteral("HideLaneAction"));
            auto* undoAction = window.findChild<QAction*>(QStringLiteral("UndoAction"));
            auto* redoAction = window.findChild<QAction*>(QStringLiteral("RedoAction"));
            auto* saveState = window.findChild<QLabel*>(QStringLiteral("SaveStateLabel"));
            auto fail = [&application, &window](const QString& message) {
                qCritical().noquote() << message;
                if (auto* popup = QApplication::activePopupWidget()) popup->close();
                if (auto* modal = QApplication::activeModalWidget()) modal->close();
                window.hide();
                application.exit(4);
            };
            const auto* initialGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!canvas || !showButton || !hideAction || !undoAction || !redoAction
                || !saveState
                || !initialGroup || initialGroup->visible
                || !showButton->isVisible()
                || showButton->text() != QStringLiteral("Show 1 hidden item")) {
                fail(QStringLiteral(
                    "Group header smoke could not find the hidden example group recovery path"));
                return;
            }

            showButton->click();
            QCoreApplication::processEvents();
            const auto* shownGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!shownGroup || !shownGroup->visible || showButton->isVisible()
                || saveState->text() != QStringLiteral("Unsaved changes")) {
                fail(QStringLiteral("The example group could not be restored for header interaction"));
                return;
            }

            const auto sendKey = [](QObject* target,
                                    const int key,
                                    const Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
                QKeyEvent press(QEvent::KeyPress, key, modifiers);
                QCoreApplication::sendEvent(target, &press);
                QKeyEvent release(QEvent::KeyRelease, key, modifiers);
                QCoreApplication::sendEvent(target, &release);
            };
            const auto sendMouse = [canvas](
                                       const QEvent::Type type,
                                       const QPoint position,
                                       const Qt::MouseButton button,
                                       const Qt::MouseButtons buttons) {
                QMouseEvent event(
                    type,
                    QPointF(position),
                    QPointF(canvas->viewport()->mapToGlobal(position)),
                    button,
                    buttons,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(canvas->viewport(), &event);
            };
            const auto clickHeader = [&sendMouse](const QPoint position) {
                sendMouse(
                    QEvent::MouseButtonPress,
                    position,
                    Qt::LeftButton,
                    Qt::LeftButton);
                sendMouse(
                    QEvent::MouseButtonRelease,
                    position,
                    Qt::LeftButton,
                    Qt::NoButton);
            };
            const auto laneScreenTop = [canvas, &window](const std::string& laneId) {
                auto y = 40 - canvas->verticalScrollBar()->value();
                for (const auto& lane : window.project().scenarios.front().lanes) {
                    if (!lane.visible) continue;
                    const auto height = std::clamp(lane.height, 30, 240);
                    if (lane.id == laneId) return y;
                    y += height;
                }
                return std::numeric_limits<int>::min();
            };
            const auto laneCenter = [&laneScreenTop, &window](const std::string& laneId) {
                const auto top = laneScreenTop(laneId);
                const auto* lane = wave::findLane(
                    window.project().scenarios.front(), laneId);
                return !lane || top == std::numeric_limits<int>::min()
                    ? -1
                    : top + std::clamp(lane->height, 30, 240) / 2;
            };
            const auto laneOrder = [&window] {
                std::vector<std::string> ids;
                for (const auto& lane : window.project().scenarios.front().lanes) {
                    ids.push_back(lane.id);
                }
                return ids;
            };
            const auto groupedMemberCount = [&window] {
                return static_cast<std::size_t>(std::count_if(
                    window.project().scenarios.front().lanes.begin(),
                    window.project().scenarios.front().lanes.end(),
                    [](const wave::Lane& lane) {
                        return lane.groupId == "group-handshake";
                    }));
            };

            canvas->verticalScrollBar()->setValue(0);
            QCoreApplication::processEvents();
            auto groupY = laneCenter("group-handshake");
            if (groupY < 40 || groupY >= canvas->viewport()->height()) {
                fail(QStringLiteral("The restored group header is outside the visible canvas"));
                return;
            }
            window.activateWindow();
            canvas->setFocus(Qt::OtherFocusReason);
            clickHeader(QPoint(80, groupY));
            QCoreApplication::processEvents();
            const auto selectedStatus = window.statusBar()->currentMessage();
            if (canvas->selectedLaneId() != QStringLiteral("group-handshake")
                || !hideAction->isEnabled()
                || !hideAction->text().contains(
                    QStringLiteral("Hide selected group"))
                || !selectedStatus.contains(QStringLiteral("Selected group Handshake signals"))
                || !selectedStatus.contains(QStringLiteral("Delete removes group"))
                || !selectedStatus.contains(QStringLiteral("F2 renames"))) {
                fail(QStringLiteral("Clicking a visible Group header did not select it with clear actions"));
                return;
            }

            sendKey(canvas, Qt::Key_F2);
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            auto* renameEdit = canvas->findChild<QLineEdit*>(
                QStringLiteral("LaneRenameEdit"));
            if (!renameEdit || !renameEdit->isVisible() || !renameEdit->hasFocus()
                || renameEdit->accessibleName() != QStringLiteral("Group name")
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Rename group Handshake signals"))) {
                fail(QStringLiteral("F2 did not open an identified inline Group rename editor"));
                return;
            }
            renameEdit->setText(QStringLiteral("Handshake I/O"));
            sendKey(renameEdit, Qt::Key_Return);
            QCoreApplication::processEvents();
            auto* renamedGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (renameEdit->isVisible() || !renamedGroup
                || renamedGroup->name != "Handshake I/O"
                || groupedMemberCount() != 5
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Renamed group Handshake signals to Handshake I/O"))
                || !window.statusBar()->currentMessage().contains(QStringLiteral("Ctrl+Z"))) {
                fail(QStringLiteral("Inline Group rename changed the wrong data or lacked recovery feedback"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            renamedGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!renamedGroup || renamedGroup->name != "Handshake signals"
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Undid Change group"))) {
                fail(QStringLiteral("Group rename Undo was not explicit or complete"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();
            renamedGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!renamedGroup || renamedGroup->name != "Handshake I/O"
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Redid Change group"))) {
                fail(QStringLiteral("Group rename Redo was not explicit or complete"));
                return;
            }

            groupY = laneCenter("group-handshake");
            sendMouse(
                QEvent::MouseButtonDblClick,
                QPoint(80, groupY),
                Qt::LeftButton,
                Qt::LeftButton);
            QCoreApplication::processEvents();
            if (!renameEdit->isVisible()
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Rename group Handshake I/O"))) {
                fail(QStringLiteral("Double-click did not start inline Group rename"));
                return;
            }
            sendKey(renameEdit, Qt::Key_Escape);
            QCoreApplication::processEvents();
            if (renameEdit->isVisible()
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Rename cancelled · Selected group Handshake I/O"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Delete removes group"))) {
                fail(QStringLiteral("Escape did not cancel Group rename and restore its target"));
                return;
            }

            groupY = laneCenter("group-handshake");
            bool contextMenuHandled = false;
            bool propertiesDialogHandled = false;
            QTimer::singleShot(
                0,
                &application,
                [&application, &contextMenuHandled, &propertiesDialogHandled] {
                    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                    auto* action = menu
                        ? menu->findChild<QAction*>(QStringLiteral("GroupPropertiesAction"))
                        : nullptr;
                    auto* hide = menu
                        ? menu->findChild<QAction*>(
                              QStringLiteral("HideLaneContextAction"))
                        : nullptr;
                    auto* duplicate = menu
                        ? menu->findChild<QAction*>(
                              QStringLiteral("DuplicateLaneContextAction"))
                        : nullptr;
                    if (!menu
                        || menu->objectName() != QStringLiteral("LaneHeaderContextMenu")
                        || !action
                        || action->text() != QStringLiteral("Group properties…")
                        || !hide
                        || hide->text() != QStringLiteral("Hide group")
                        || duplicate) {
                        if (menu) menu->close();
                        return;
                    }
                    contextMenuHandled = true;
                    QTimer::singleShot(
                        0,
                        &application,
                        [&propertiesDialogHandled] {
                            auto* dialog = qobject_cast<QDialog*>(
                                QApplication::activeModalWidget());
                            auto* name = dialog
                                ? dialog->findChild<QLineEdit*>(
                                      QStringLiteral("LanePropertiesNameEdit"))
                                : nullptr;
                            if (dialog
                                && dialog->objectName() == QStringLiteral("LanePropertiesDialog")
                                && name
                                && name->text() == QStringLiteral("Handshake I/O")) {
                                propertiesDialogHandled = true;
                            }
                            if (dialog) dialog->reject();
                        });
                    action->trigger();
                    menu->close();
                });
            const QPoint contextPoint(80, groupY);
            QContextMenuEvent contextEvent(
                QContextMenuEvent::Mouse,
                contextPoint,
                canvas->viewport()->mapToGlobal(contextPoint));
            QCoreApplication::sendEvent(canvas->viewport(), &contextEvent);
            QCoreApplication::processEvents();
            renamedGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!contextMenuHandled || !propertiesDialogHandled
                || !renamedGroup || renamedGroup->name != "Handshake I/O") {
                fail(QStringLiteral("Group right-click did not expose a cancellable properties entry"));
                return;
            }

            groupY = laneCenter("group-handshake");
            bool groupHideHandled = false;
            QTimer::singleShot(
                0,
                &application,
                [&groupHideHandled] {
                    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                    auto* hide = menu
                        ? menu->findChild<QAction*>(
                              QStringLiteral("HideLaneContextAction"))
                        : nullptr;
                    if (!menu
                        || menu->objectName() != QStringLiteral("LaneHeaderContextMenu")
                        || !hide
                        || hide->text() != QStringLiteral("Hide group")) {
                        if (menu) menu->close();
                        return;
                    }
                    groupHideHandled = true;
                    hide->trigger();
                    menu->close();
                });
            const QPoint hideContextPoint(80, groupY);
            QContextMenuEvent hideContextEvent(
                QContextMenuEvent::Mouse,
                hideContextPoint,
                canvas->viewport()->mapToGlobal(hideContextPoint));
            QCoreApplication::sendEvent(canvas->viewport(), &hideContextEvent);
            QCoreApplication::processEvents();
            const auto* hiddenGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!groupHideHandled || !hiddenGroup || hiddenGroup->visible
                || !canvas->selectedLaneId().isEmpty()
                || !showButton->isVisible()
                || showButton->text() != QStringLiteral("Show 1 hidden item")
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Hidden group Handshake I/O"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Z"))
                || !undoAction->text().contains(QStringLiteral("Hide group"))
                || QApplication::activeModalWidget()) {
                fail(QStringLiteral("Group right-click hide did not provide a one-step recoverable result"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            const auto* groupAfterHideUndo = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!groupAfterHideUndo || !groupAfterHideUndo->visible
                || showButton->isVisible()
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Undid Hide group"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Y"))) {
                fail(QStringLiteral("Group hide Undo did not restore the visible group"));
                return;
            }
            const auto orderBeforeMove = laneOrder();
            groupY = laneCenter("group-handshake");
            const auto acknowledgeTop = laneScreenTop("lane-ack");
            if (groupY < 40 || acknowledgeTop < 40) {
                fail(QStringLiteral("Group reorder targets are outside the visible canvas"));
                return;
            }
            sendMouse(
                QEvent::MouseButtonPress,
                QPoint(80, groupY),
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseMove,
                QPoint(80, acknowledgeTop + 3),
                Qt::NoButton,
                Qt::LeftButton);
            QCoreApplication::processEvents();
            if (!canvas->laneDropDestinationIndex()
                || *canvas->laneDropDestinationIndex() != 3) {
                sendMouse(
                    QEvent::MouseButtonRelease,
                    QPoint(80, acknowledgeTop + 3),
                    Qt::LeftButton,
                    Qt::NoButton);
                fail(QStringLiteral("Dragging a Group header did not expose the expected insertion target"));
                return;
            }
            sendMouse(
                QEvent::MouseButtonRelease,
                QPoint(80, acknowledgeTop + 3),
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            const auto orderAfterMove = laneOrder();
            if (orderAfterMove.size() != orderBeforeMove.size()
                || orderAfterMove.at(3) != "group-handshake"
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Moved Handshake I/O: position 3 -> 4"))) {
                fail(QStringLiteral("Group header drag committed the wrong order or feedback"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (laneOrder() != orderBeforeMove
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Undid Move group"))
                || !window.statusBar()->currentMessage().contains(QStringLiteral("Ctrl+Y"))) {
                fail(QStringLiteral("Group reorder Undo did not restore order with accurate feedback"));
                return;
            }

            groupY = laneCenter("group-handshake");
            clickHeader(QPoint(80, groupY));
            canvas->setFocus(Qt::OtherFocusReason);
            bool removalConfirmed = false;
            QTimer::singleShot(
                0,
                &application,
                [&removalConfirmed] {
                    auto* confirmation = qobject_cast<QMessageBox*>(
                        QApplication::activeModalWidget());
                    auto* yes = confirmation
                        ? confirmation->button(QMessageBox::Yes)
                        : nullptr;
                    if (!confirmation || !yes) {
                        if (confirmation) confirmation->reject();
                        return;
                    }
                    const auto text = confirmation->text();
                    removalConfirmed = text.contains(
                                           QStringLiteral("Remove group \"Handshake I/O\""))
                        && text.contains(QStringLiteral("5 member signals"))
                        && text.contains(QStringLiteral("become ungrouped"))
                        && text.contains(QStringLiteral("Ctrl+Z"));
                    if (removalConfirmed) {
                        yes->click();
                    } else {
                        confirmation->reject();
                    }
                });
            sendKey(canvas, Qt::Key_Delete);
            QCoreApplication::processEvents();
            const auto* removedGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!removalConfirmed || removedGroup || groupedMemberCount() != 0
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Removed group Handshake I/O"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("5 member signals ungrouped"))
                || !window.statusBar()->currentMessage().contains(QStringLiteral("Ctrl+Z"))) {
                fail(QStringLiteral("Delete did not remove the selected Group with dependency feedback"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            const auto* restoredGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!restoredGroup || !restoredGroup->visible
                || restoredGroup->name != "Handshake I/O"
                || groupedMemberCount() != 5
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Undid Remove group"))
                || !window.statusBar()->currentMessage().contains(QStringLiteral("Ctrl+Y"))) {
                fail(QStringLiteral("Group delete Undo did not restore membership and feedback"));
                return;
            }

            undoAction->trigger();
            undoAction->trigger();
            QCoreApplication::processEvents();
            const auto* baselineGroup = wave::findLane(
                window.project().scenarios.front(),
                "group-handshake");
            if (!baselineGroup || baselineGroup->visible
                || baselineGroup->name != "Handshake signals"
                || groupedMemberCount() != 5
                || !showButton->isVisible()
                || showButton->text() != QStringLiteral("Show 1 hidden item")
                || saveState->text() != QStringLiteral("Saved")
                || window.windowTitle().contains(QStringLiteral(" *"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("back to saved version"))) {
                fail(QStringLiteral("Group workflow did not return to the exact Saved baseline"));
                return;
            }

            window.hide();
            application.exit(0);
        });
    } else if (signalHeaderSmoke) {
        QTimer::singleShot(0, &window, [&application, &window] {
            auto* canvas = window.findChild<wave::WaveCanvas*>();
            auto* undoAction = window.findChild<QAction*>(QStringLiteral("UndoAction"));
            auto* saveState = window.findChild<QLabel*>(QStringLiteral("SaveStateLabel"));
            auto fail = [&application, &window](const QString& message) {
                qCritical().noquote() << message;
                QToolTip::hideText();
                window.hide();
                application.exit(4);
            };
            if (!canvas || !undoAction || !saveState
                || window.project().scenarios.empty()) {
                fail(QStringLiteral("Signal header smoke prerequisites are missing"));
                return;
            }

            auto& scenario = window.project().scenarios.front();
            const auto* requestLane = wave::findLane(scenario, "lane-request");
            if (!requestLane
                || requestLane->name.find("distinguishing_suffix") == std::string::npos
                || canvas->signalHeaderWidth() != 190
                || saveState->text() != QStringLiteral("Saved")
                || undoAction->isEnabled()
                || window.windowTitle().contains(QStringLiteral(" *"))) {
                fail(QStringLiteral("Signal header smoke did not start from the expected Saved baseline"));
                return;
            }

            const auto sendMouse = [canvas](
                                       const QEvent::Type type,
                                       const QPoint position,
                                       const Qt::MouseButton button,
                                       const Qt::MouseButtons buttons) {
                QMouseEvent event(
                    type,
                    QPointF(position),
                    QPointF(canvas->viewport()->mapToGlobal(position)),
                    button,
                    buttons,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(canvas->viewport(), &event);
            };
            const auto sendKey = [](QObject* target, const int key) {
                QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
                QCoreApplication::sendEvent(target, &press);
                QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
                QCoreApplication::sendEvent(target, &release);
            };
            const auto laneCenter = [canvas, &scenario](const std::string& laneId) {
                auto y = 40 - canvas->verticalScrollBar()->value();
                for (const auto& lane : scenario.lanes) {
                    if (!lane.visible) continue;
                    const auto height = std::clamp(lane.height, 30, 240);
                    if (lane.id == laneId) return y + height / 2;
                    y += height;
                }
                return -1;
            };

            canvas->verticalScrollBar()->setValue(0);
            QCoreApplication::processEvents();
            const auto requestY = laneCenter("lane-request");
            const auto fullName = QString::fromStdString(requestLane->name);
            if (requestY < 40 || requestY >= canvas->viewport()->height()) {
                fail(QStringLiteral("Long-name signal is outside the visible canvas"));
                return;
            }

            QHelpEvent nameTip(
                QEvent::ToolTip,
                QPoint(40, requestY),
                canvas->viewport()->mapToGlobal(QPoint(40, requestY)));
            QCoreApplication::sendEvent(canvas->viewport(), &nameTip);
            QCoreApplication::processEvents();
            if (QToolTip::text() != fullName) {
                fail(QStringLiteral("Signal header tooltip did not expose the complete long name"));
                return;
            }
            QToolTip::hideText();

            sendMouse(
                QEvent::MouseMove,
                QPoint(190, 20),
                Qt::NoButton,
                Qt::NoButton);
            if (canvas->viewport()->cursor().shape() != Qt::SplitHCursor) {
                fail(QStringLiteral("Signal header divider did not advertise horizontal resizing"));
                return;
            }
            QHelpEvent dividerTip(
                QEvent::ToolTip,
                QPoint(190, 20),
                canvas->viewport()->mapToGlobal(QPoint(190, 20)));
            QCoreApplication::sendEvent(canvas->viewport(), &dividerTip);
            QCoreApplication::processEvents();
            if (!QToolTip::text().contains(QStringLiteral("Drag to resize"))
                || !QToolTip::text().contains(QStringLiteral("double-click to fit"))) {
                fail(QStringLiteral("Signal header divider tooltip did not explain both direct actions"));
                return;
            }
            QToolTip::hideText();

            sendMouse(
                QEvent::MouseButtonPress,
                QPoint(190, 20),
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseMove,
                QPoint(300, 20),
                Qt::NoButton,
                Qt::LeftButton);
            if (canvas->signalHeaderWidth() != 300) {
                fail(QStringLiteral("Dragging the signal header divider did not resize live"));
                return;
            }
            sendMouse(
                QEvent::MouseButtonRelease,
                QPoint(300, 20),
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            if (canvas->signalHeaderWidth() != 300
                || QSettings{}.value(QStringLiteral("canvas/signalHeaderWidth")).toInt() != 300
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Signal names width 300 px"))
                || saveState->text() != QStringLiteral("Saved")
                || undoAction->isEnabled()
                || window.windowTitle().contains(QStringLiteral(" *"))) {
                fail(QStringLiteral("Committed signal header width was not persisted as a UI-only preference"));
                return;
            }

            sendMouse(
                QEvent::MouseButtonPress,
                QPoint(300, 20),
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseMove,
                QPoint(360, 20),
                Qt::NoButton,
                Qt::LeftButton);
            if (canvas->signalHeaderWidth() != 360) {
                fail(QStringLiteral("Second signal header drag did not preview the new width"));
                return;
            }
            sendKey(canvas, Qt::Key_Escape);
            QCoreApplication::processEvents();
            if (canvas->signalHeaderWidth() != 300
                || QSettings{}.value(QStringLiteral("canvas/signalHeaderWidth")).toInt() != 300
                || canvas->viewport()->cursor().shape() != Qt::PointingHandCursor
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Signal names resize cancelled"))) {
                fail(QStringLiteral("Escape did not restore the last committed signal header width"));
                return;
            }
            sendMouse(
                QEvent::MouseButtonRelease,
                QPoint(360, 20),
                Qt::LeftButton,
                Qt::NoButton);

            sendMouse(
                QEvent::MouseButtonDblClick,
                QPoint(300, 20),
                Qt::LeftButton,
                Qt::LeftButton);
            QCoreApplication::processEvents();
            if (canvas->signalHeaderWidth() != 480
                || QSettings{}.value(QStringLiteral("canvas/signalHeaderWidth")).toInt() != 480
                || canvas->viewport()->cursor().shape() != Qt::PointingHandCursor
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Signal names fitted to 480 px"))
                || saveState->text() != QStringLiteral("Saved")
                || undoAction->isEnabled()) {
                fail(QStringLiteral("Double-click did not auto-fit and persist the long-name column"));
                return;
            }

            QHelpEvent resizedNameTip(
                QEvent::ToolTip,
                QPoint(40, requestY),
                canvas->viewport()->mapToGlobal(QPoint(40, requestY)));
            QCoreApplication::sendEvent(canvas->viewport(), &resizedNameTip);
            QCoreApplication::processEvents();
            if (QToolTip::text() != fullName) {
                fail(QStringLiteral("Long-name tooltip was lost after signal header resizing"));
                return;
            }
            QToolTip::hideText();

            wave::MainWindow reopened(window.project(), QString{});
            reopened.show();
            QCoreApplication::processEvents();
            auto* reopenedCanvas = reopened.findChild<wave::WaveCanvas*>();
            if (!reopenedCanvas
                || reopenedCanvas->signalHeaderWidth() != 480
                || reopenedCanvas->horizontalScrollBar()->maximum() != 0) {
                reopened.hide();
                fail(QStringLiteral("A new window did not restore the committed signal header width"));
                return;
            }
            reopened.hide();

            if (saveState->text() != QStringLiteral("Saved")
                || undoAction->isEnabled()
                || window.windowTitle().contains(QStringLiteral(" *"))) {
                fail(QStringLiteral("Signal header interactions altered project history or dirty state"));
                return;
            }
            window.hide();
            application.exit(0);
        });
    } else if (canvasAddLaneSmoke) {
        QTimer::singleShot(0, &window, [&application, &window, canvasAddLaneScreenshotPath] {
            auto* canvas = window.findChild<wave::WaveCanvas*>();
            const std::array<QToolButton*, 3> addButtons{
                window.findChild<QToolButton*>(QStringLiteral("CanvasAddClockButton")),
                window.findChild<QToolButton*>(QStringLiteral("CanvasAddBitButton")),
                window.findChild<QToolButton*>(QStringLiteral("CanvasAddBusButton")),
            };
            auto fail = [&application, &window](const QString& message) {
                qCritical().noquote() << message;
                window.hide();
                application.exit(4);
            };
            if (!canvas
                || std::any_of(addButtons.begin(), addButtons.end(), [](const auto* button) {
                       return button == nullptr;
                   })) {
                fail(QStringLiteral("Canvas quick-add controls are missing"));
                return;
            }

            canvas->verticalScrollBar()->setValue(
                canvas->verticalScrollBar()->maximum());
            QCoreApplication::processEvents();
            const std::array<QString, 3> expectedLabels{
                QStringLiteral("+ CLK"),
                QStringLiteral("+ BIT"),
                QStringLiteral("+ BUS"),
            };
            for (std::size_t index = 0; index < addButtons.size(); ++index) {
                const auto* button = addButtons.at(index);
                if (!button->isVisible()
                    || !button->geometry().intersects(canvas->viewport()->rect())
                    || button->text() != expectedLabels.at(index)) {
                    fail(QStringLiteral("Canvas quick-add controls are not visible at the lane-list end"));
                    return;
                }
            }

            auto* waveformToolbar = window.findChild<QToolBar*>(
                QStringLiteral("WaveformToolbar"));
            auto* undoAction = window.findChild<QAction*>(QStringLiteral("UndoAction"));
            auto* redoAction = window.findChild<QAction*>(QStringLiteral("RedoAction"));
            auto* cutRangeAction = window.findChild<QAction*>(QStringLiteral("CutRangeAction"));
            auto* duplicateLaneAction = window.findChild<QAction*>(
                QStringLiteral("DuplicateLaneAction"));
            auto* measureAction = window.findChild<QAction*>(
                QStringLiteral("MeasureToolAction"));
            auto* asyncTimingAction = window.findChild<QAction*>(
                QStringLiteral("AsyncTimingAction"));
            const auto toolbarActions = waveformToolbar
                ? waveformToolbar->actions()
                : QList<QAction*>{};
            const auto checkableModeCount = std::count_if(
                toolbarActions.begin(),
                toolbarActions.end(),
                [](const QAction* action) { return action && action->isCheckable(); });
            const auto hasRemovedTool = std::any_of(
                toolbarActions.begin(),
                toolbarActions.end(),
                [](const QAction* action) {
                    auto text = action ? action->text() : QString{};
                    text.remove(QLatin1Char('&'));
                    return text.compare(QStringLiteral("Transition"), Qt::CaseInsensitive) == 0
                        || text.compare(QStringLiteral("Edit"), Qt::CaseInsensitive) == 0
                        || text.compare(QStringLiteral("Export"), Qt::CaseInsensitive) == 0
                        || (text.compare(
                                QStringLiteral("Fit selection"),
                                Qt::CaseInsensitive)
                            == 0
                            && action->objectName()
                                != QStringLiteral("FitScenarioAction"));
                });
            if (!waveformToolbar
                || !undoAction
                || !redoAction
                || !cutRangeAction
                || !duplicateLaneAction
                || !measureAction
                || !asyncTimingAction
                || window.findChild<QAction*>(QStringLiteral("WaveEditToolAction"))
                || checkableModeCount != 2
                || measureAction->isChecked()
                || asyncTimingAction->isChecked()
                || asyncTimingAction->text() != QStringLiteral("Sync")
                || canvas->asynchronousEditing()
                || measureAction->shortcut().matches(QKeySequence(Qt::CTRL | Qt::Key_M))
                    != QKeySequence::ExactMatch
                || canvas->tool() != wave::WaveCanvas::Tool::WaveEdit
                || toolbarActions.contains(undoAction)
                || toolbarActions.contains(redoAction)
                || toolbarActions.contains(duplicateLaneAction)
                || hasRemovedTool
                || undoAction->shortcut().matches(QKeySequence(QKeySequence::Undo))
                    != QKeySequence::ExactMatch
                || redoAction->shortcut().matches(QKeySequence(QKeySequence::Redo))
                    != QKeySequence::ExactMatch
                || cutRangeAction->shortcut().matches(QKeySequence(QKeySequence::Cut))
                    != QKeySequence::ExactMatch
                || duplicateLaneAction->shortcut().matches(
                       QKeySequence(Qt::CTRL | Qt::Key_D))
                    != QKeySequence::ExactMatch) {
                fail(QStringLiteral("Toolbar convergence or Edit menu shortcuts are incorrect"));
                return;
            }

            const auto originalLaneCount = window.project().scenarios.front().lanes.size();
            const auto originalClockCount = window.project().clockDomains.size();
            for (auto* button : addButtons) {
                button->click();
                QCoreApplication::processEvents();
                auto* setupPanel = canvas->findChild<QWidget*>(
                    QStringLiteral("QuickLaneSetupPanel"));
                auto* nameEdit = canvas->findChild<QLineEdit*>(
                    QStringLiteral("QuickLaneNameEdit"));
                if (QApplication::activeModalWidget()
                    || !setupPanel
                    || !setupPanel->isVisible()
                    || !nameEdit
                    || !nameEdit->hasFocus()) {
                    fail(QStringLiteral("Quick-add did not open the inline setup row"));
                    return;
                }
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(nameEdit, &enter);
                QCoreApplication::processEvents();
                if (setupPanel->isVisible()) {
                    fail(QStringLiteral("Inline quick-add did not commit with Enter"));
                    return;
                }
            }

            const auto& addedLanes = window.project().scenarios.front().lanes;
            if (addedLanes.size() != originalLaneCount + 3
                || addedLanes.at(originalLaneCount).kind != wave::LaneKind::Clock
                || addedLanes.at(originalLaneCount + 1).kind != wave::LaneKind::Bit
                || addedLanes.at(originalLaneCount + 2).kind != wave::LaneKind::Bus
                || addedLanes.at(originalLaneCount + 2).width != 8
                || window.project().clockDomains.size() != originalClockCount + 1) {
                fail(QStringLiteral("Quick-add created incorrect lane kinds or clock-domain state"));
                return;
            }
            const auto quickClock = addedLanes.at(originalLaneCount);
            const auto quickBit = addedLanes.at(originalLaneCount + 1);
            const auto quickBus = addedLanes.at(originalLaneCount + 2);
            if (!wave::findClock(window.project(), quickClock.clockDomainId)) {
                fail(QStringLiteral("Quick CLK did not create and link a default clock domain"));
                return;
            }
            const std::array<wave::Lane, 3> quickLanes{quickClock, quickBit, quickBus};
            for (const auto& lane : quickLanes) {
                const auto nameCount = std::count_if(
                    addedLanes.begin(),
                    addedLanes.end(),
                    [&lane](const wave::Lane& candidate) {
                        return QString::compare(
                                   QString::fromStdString(candidate.name),
                                   QString::fromStdString(lane.name),
                                   Qt::CaseInsensitive)
                            == 0;
                    });
                if (nameCount != 1 || !QColor(QString::fromStdString(lane.color)).isValid()) {
                    fail(QStringLiteral("Quick-add did not assign a unique name and valid color"));
                    return;
                }
            }
            for (std::size_t left = 0; left < quickLanes.size(); ++left) {
                for (std::size_t right = left + 1; right < quickLanes.size(); ++right) {
                    if (QString::compare(
                            QString::fromStdString(quickLanes.at(left).color),
                            QString::fromStdString(quickLanes.at(right).color),
                            Qt::CaseInsensitive)
                        == 0) {
                        fail(QStringLiteral("Quick-add reused a color while unused palette colors remained"));
                        return;
                    }
                }
            }

            const auto sendKey = [](QObject* target,
                                    const int key,
                                    const Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
                QKeyEvent press(QEvent::KeyPress, key, modifiers);
                QCoreApplication::sendEvent(target, &press);
                QKeyEvent release(QEvent::KeyRelease, key, modifiers);
                QCoreApplication::sendEvent(target, &release);
            };
            const auto sendMouse = [canvas](
                                       const QEvent::Type type,
                                       const QPoint position,
                                       const Qt::MouseButton button,
                                       const Qt::MouseButtons buttons,
                                       const Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
                QMouseEvent event(
                    type,
                    QPointF(position),
                    QPointF(canvas->viewport()->mapToGlobal(position)),
                    button,
                    buttons,
                    modifiers);
                QCoreApplication::sendEvent(canvas->viewport(), &event);
            };
            const auto clickHeader = [&sendMouse](const QPoint position) {
                sendMouse(
                    QEvent::MouseButtonPress,
                    position,
                    Qt::LeftButton,
                    Qt::LeftButton);
                sendMouse(
                    QEvent::MouseButtonRelease,
                    position,
                    Qt::LeftButton,
                    Qt::NoButton);
            };
            const auto laneScreenTop = [canvas, &window](const std::string& laneId) {
                auto y = 40 - canvas->verticalScrollBar()->value();
                for (const auto& lane : window.project().scenarios.front().lanes) {
                    if (!lane.visible) continue;
                    const auto height = std::clamp(lane.height, 30, 240);
                    if (lane.id == laneId) return y;
                    y += height;
                }
                return std::numeric_limits<int>::min();
            };
            const auto laneCenter = [&laneScreenTop, &window](const std::string& laneId) {
                const auto top = laneScreenTop(laneId);
                const auto* lane = wave::findLane(
                    window.project().scenarios.front(), laneId);
                return !lane || top == std::numeric_limits<int>::min()
                    ? -1
                    : top + std::clamp(lane->height, 30, 240) / 2;
            };

            window.activateWindow();
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_Z, Qt::ControlModifier);
            QCoreApplication::processEvents();
            if (window.project().scenarios.front().lanes.size() != originalLaneCount + 2
                || wave::findLane(window.project().scenarios.front(), quickBus.id)) {
                fail(QStringLiteral("Ctrl+Z did not undo quick-add through the menu action"));
                return;
            }
            sendKey(canvas, Qt::Key_Y, Qt::ControlModifier);
            QCoreApplication::processEvents();
            if (window.project().scenarios.front().lanes.size() != originalLaneCount + 3
                || !wave::findLane(window.project().scenarios.front(), quickBus.id)) {
                fail(QStringLiteral("Ctrl+Y did not redo quick-add through the menu action"));
                return;
            }

            const auto* implicitBus = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!implicitBus || !implicitBus->segments.empty()) {
                fail(QStringLiteral("Quick Bus did not begin as an implicit X lane"));
                return;
            }
            canvas->revealLocation(QString::fromStdString(quickBus.id), 100'000);
            QCoreApplication::processEvents();
            const auto presetBusY = laneCenter(quickBus.id);
            const QPoint paletteClick(520, presetBusY);
            sendMouse(
                QEvent::MouseButtonPress,
                paletteClick,
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                paletteClick,
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            auto* presetPalette = window.findChild<QWidget*>(QStringLiteral("BusPresetPalette"));
            const std::array<QToolButton*, 4> presetButtons{
                window.findChild<QToolButton*>(QStringLiteral("BusPresetZeroButton")),
                window.findChild<QToolButton*>(QStringLiteral("BusPresetXButton")),
                window.findChild<QToolButton*>(QStringLiteral("BusPresetZButton")),
                window.findChild<QToolButton*>(QStringLiteral("BusPresetDontCareButton")),
            };
            auto* directValue = window.findChild<QLineEdit*>(QStringLiteral("BusPresetValueEdit"));
            auto* radixCombo = window.findChild<QComboBox*>(QStringLiteral("BusEditRadixCombo"));
            auto* recentValues = window.findChild<QComboBox*>(QStringLiteral("BusEditRecentValuesCombo"));
            auto* contextLabel = window.findChild<QLabel*>(QStringLiteral("BusPresetContextLabel"));
            if (!presetPalette
                || !presetPalette->isVisible()
                || !directValue
                || !directValue->isVisible()
                || !radixCombo
                || !radixCombo->isVisible()
                || radixCombo->currentText() != QStringLiteral("HEX")
                || !recentValues
                || !recentValues->isVisible()
                || !contextLabel
                || !contextLabel->text().contains(QString::fromStdString(quickBus.name))
                || window.findChild<QToolButton*>(QStringLiteral("BusPresetCustomButton"))
                || std::any_of(
                    presetButtons.begin(),
                    presetButtons.end(),
                    [](const auto* button) { return !button || !button->isVisible(); })) {
                fail(QStringLiteral("Clicking a Bus did not show the simplified direct-value palette"));
                return;
            }

            presetButtons.front()->click();
            QCoreApplication::processEvents();
            auto* busAfterButtonClick = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            const auto zeroSegment = busAfterButtonClick
                ? std::find_if(
                      busAfterButtonClick->segments.begin(),
                      busAfterButtonClick->segments.end(),
                      [](const wave::Segment& segment) {
                          const auto preset = segment.extensions.find(
                              "waveWorkbench.busPreset");
                          return preset != segment.extensions.end()
                              && preset->second == "\"zero\"";
                      })
                : std::vector<wave::Segment>::iterator{};
            if (!busAfterButtonClick
                || zeroSegment == busAfterButtonClick->segments.end()
                || zeroSegment->value != "0b00000000") {
                fail(QStringLiteral("Bus 0 button did not insert one zero beat"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            busAfterButtonClick = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!busAfterButtonClick || !busAfterButtonClick->segments.empty()) {
                fail(QStringLiteral("Bus 0 button was not undoable"));
                return;
            }

            sendMouse(
                QEvent::MouseButtonPress,
                paletteClick,
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                paletteClick,
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            directValue->setText(QStringLiteral("2a"));
            QKeyEvent valueEnter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QCoreApplication::sendEvent(directValue, &valueEnter);
            QCoreApplication::processEvents();
            busAfterButtonClick = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!busAfterButtonClick
                || busAfterButtonClick->segments.empty()
                || busAfterButtonClick->segments.front().value != "0x2a") {
                fail(QStringLiteral("Bus direct value input did not apply with Enter"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();

            sendMouse(
                QEvent::MouseButtonPress,
                paletteClick,
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                paletteClick,
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            if (!recentValues->isVisible()
                || !recentValues->isEnabled()
                || recentValues->findText(QStringLiteral("0x2a")) <= 0) {
                fail(QStringLiteral("Bus recent values did not retain the normalized per-signal value"));
                return;
            }
            sendKey(directValue, Qt::Key_Escape);
            QCoreApplication::processEvents();

            const auto busHasExactValue = [&window, &quickBus](
                                              const std::pair<wave::Tick, wave::Tick>& range,
                                              const std::string_view value) {
                const auto* lane = wave::findLane(
                    window.project().scenarios.front(),
                    quickBus.id);
                return lane
                    && std::any_of(
                        lane->segments.begin(),
                        lane->segments.end(),
                        [&range, value](const wave::Segment& segment) {
                            return segment.start == range.first
                                && segment.end == range.second
                                && segment.value == value;
                        });
            };
            const auto assertBusEmpty = [&window, &quickBus]() {
                const auto* lane = wave::findLane(
                    window.project().scenarios.front(),
                    quickBus.id);
                return lane && lane->segments.empty();
            };

            const auto sequentialStart = wave::Tick{100'000};
            canvas->revealLocation(
                QString::fromStdString(quickBus.id),
                sequentialStart);
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_Return);
            QCoreApplication::processEvents();
            const auto firstSequentialBeat = canvas->selectedTimeRange();
            if (!presetPalette->isVisible()
                || !directValue->hasFocus()
                || !firstSequentialBeat
                || firstSequentialBeat->second <= firstSequentialBeat->first
                || !directValue->placeholderText().contains(
                    QStringLiteral("X (implicit)"))
                || !contextLabel->toolTip().contains(
                    QStringLiteral("Tab applies and advances"))) {
                fail(QStringLiteral("Enter did not open an explicit sequential Bus beat target"));
                return;
            }

            sendKey(directValue, Qt::Key_Tab);
            QCoreApplication::processEvents();
            const auto skippedImplicitBeat = canvas->selectedTimeRange();
            if (!skippedImplicitBeat
                || skippedImplicitBeat->first != firstSequentialBeat->second
                || !assertBusEmpty()
                || !directValue->hasFocus()
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("implicit X skipped"))) {
                fail(QStringLiteral("Tab did not skip an untouched implicit-X Bus beat"));
                return;
            }
            sendKey(directValue, Qt::Key_Backtab, Qt::ShiftModifier);
            QCoreApplication::processEvents();
            if (canvas->selectedTimeRange() != firstSequentialBeat
                || !assertBusEmpty()
                || !directValue->hasFocus()) {
                fail(QStringLiteral("Shift+Tab did not skip back across implicit-X Bus beats"));
                return;
            }

            directValue->setText(QStringLiteral("0x0f"));
            directValue->setModified(true);
            sendKey(directValue, Qt::Key_Up);
            QCoreApplication::processEvents();
            if (directValue->text() != QStringLiteral("0x10")
                || !directValue->isModified()
                || !directValue->hasFocus()
                || !assertBusEmpty()) {
                fail(QStringLiteral("Bus Up did not increment a known draft without committing"));
                return;
            }
            sendKey(directValue, Qt::Key_Down);
            QCoreApplication::processEvents();
            if (directValue->text() != QStringLiteral("0xf")
                || !directValue->isModified()
                || !assertBusEmpty()) {
                fail(QStringLiteral("Bus Down did not decrement a known draft without committing"));
                return;
            }
            directValue->setText(QStringLiteral("0xff"));
            directValue->setModified(true);
            sendKey(directValue, Qt::Key_Up);
            QCoreApplication::processEvents();
            if (directValue->text() != QStringLiteral("0xff")
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("maximum"))
                || !assertBusEmpty()) {
                fail(QStringLiteral("Bus numeric stepping did not stop at the width maximum"));
                return;
            }

            directValue->setText(QStringLiteral("0x0a"));
            directValue->setModified(true);
            sendKey(directValue, Qt::Key_Return, Qt::ControlModifier);
            QCoreApplication::processEvents();
            if (!busHasExactValue(*firstSequentialBeat, "0x0a")
                || canvas->selectedTimeRange() != firstSequentialBeat
                || !presetPalette->isVisible()
                || !directValue->hasFocus()
                || directValue->text() != QStringLiteral("0x0a")
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("target kept"))) {
                fail(QStringLiteral("Ctrl+Enter did not apply the Bus draft while keeping its target"));
                return;
            }
            sendKey(directValue, Qt::Key_Return, Qt::ControlModifier);
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (!assertBusEmpty()) {
                fail(QStringLiteral("Repeated Ctrl+Enter created empty Bus history"));
                return;
            }

            directValue->setText(QStringLiteral("0x11"));
            directValue->setModified(true);
            sendKey(directValue, Qt::Key_Tab);
            QCoreApplication::processEvents();
            const auto secondSequentialBeat = canvas->selectedTimeRange();
            const auto sequentialBeatWidth =
                firstSequentialBeat->second - firstSequentialBeat->first;
            if (!busHasExactValue(*firstSequentialBeat, "0x11")
                || !presetPalette->isVisible()
                || !directValue->hasFocus()
                || !secondSequentialBeat
                || secondSequentialBeat->first != firstSequentialBeat->second
                || secondSequentialBeat->second
                    != firstSequentialBeat->second + sequentialBeatWidth) {
                fail(QStringLiteral("Tab did not apply and advance by one Sync Bus beat"));
                return;
            }
            if (!canvasAddLaneScreenshotPath.isEmpty()) {
                auto sequentialScreenshotPath = canvasAddLaneScreenshotPath;
                const auto suffix = sequentialScreenshotPath.lastIndexOf(QLatin1Char('.'));
                if (suffix >= 0) {
                    sequentialScreenshotPath.insert(
                        suffix,
                        QStringLiteral("-bus-sequential-entry"));
                } else {
                    sequentialScreenshotPath.append(
                        QStringLiteral("-bus-sequential-entry.png"));
                }
                if (!window.grab().save(sequentialScreenshotPath)) {
                    fail(QStringLiteral("Cannot save Bus sequential-entry screenshot"));
                    return;
                }
            }

            directValue->setText(QStringLiteral("0x22"));
            directValue->setModified(true);
            sendKey(directValue, Qt::Key_Backtab, Qt::ShiftModifier);
            QCoreApplication::processEvents();
            if (!busHasExactValue(*secondSequentialBeat, "0x22")
                || canvas->selectedTimeRange() != firstSequentialBeat
                || directValue->text() != QStringLiteral("0x11")
                || !directValue->hasFocus()) {
                fail(QStringLiteral("Shift+Tab did not apply and return to the previous Bus beat"));
                return;
            }

            sendKey(directValue, Qt::Key_Down, Qt::ControlModifier);
            QCoreApplication::processEvents();
            if (directValue->text() != QStringLiteral("0x0a")
                || !directValue->isModified()
                || !directValue->hasFocus()
                || recentValues->currentText() != QStringLiteral("0x0a")
                || !busHasExactValue(*firstSequentialBeat, "0x11")) {
                fail(QStringLiteral("Ctrl+Down did not cycle to the next recent Bus value as a draft"));
                return;
            }
            sendKey(directValue, Qt::Key_Up, Qt::ControlModifier);
            QCoreApplication::processEvents();
            if (directValue->text() != QStringLiteral("0x11")
                || recentValues->currentText() != QStringLiteral("0x11")
                || !busHasExactValue(*firstSequentialBeat, "0x11")) {
                fail(QStringLiteral("Ctrl+Up did not cycle back through recent Bus values"));
                return;
            }

            directValue->setText(QStringLiteral("0x1ff"));
            directValue->setModified(true);
            sendKey(directValue, Qt::Key_Tab);
            QCoreApplication::processEvents();
            if (canvas->selectedTimeRange() != firstSequentialBeat
                || !directValue->hasFocus()
                || !directValue->isModified()
                || directValue->toolTip().isEmpty()
                || !busHasExactValue(*firstSequentialBeat, "0x11")) {
                fail(QStringLiteral("Invalid sequential Bus input navigated away or changed the model"));
                return;
            }

            directValue->setText(QStringLiteral("0x11"));
            directValue->setModified(false);
            sendKey(directValue, Qt::Key_Tab);
            QCoreApplication::processEvents();
            if (canvas->selectedTimeRange() != secondSequentialBeat) {
                fail(QStringLiteral("Confirming an unchanged Bus beat did not advance"));
                return;
            }
            sendKey(directValue, Qt::Key_Escape);
            undoAction->trigger();
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (!assertBusEmpty()) {
                fail(QStringLiteral("Sequential Bus entry created empty history or was not independently undoable"));
                return;
            }

            asyncTimingAction->trigger();
            QCoreApplication::processEvents();
            const auto asyncStart = wave::Tick{123'456};
            canvas->revealLocation(
                QString::fromStdString(quickBus.id),
                asyncStart);
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_Return);
            QCoreApplication::processEvents();
            const auto firstAsyncBeat = canvas->selectedTimeRange();
            const auto syncOffset =
                ((firstSequentialBeat->first % sequentialBeatWidth)
                 + sequentialBeatWidth)
                % sequentialBeatWidth;
            const auto asyncOffset = firstAsyncBeat
                ? ((firstAsyncBeat->first % sequentialBeatWidth)
                   + sequentialBeatWidth)
                    % sequentialBeatWidth
                : syncOffset;
            if (!canvas->asynchronousEditing()
                || !firstAsyncBeat
                || asyncOffset == syncOffset
                || firstAsyncBeat->second <= firstAsyncBeat->first) {
                fail(QStringLiteral("Async Bus entry did not preserve its off-grid start"));
                return;
            }
            directValue->setText(QStringLiteral("0x33"));
            directValue->setModified(true);
            sendKey(directValue, Qt::Key_Tab);
            QCoreApplication::processEvents();
            const auto secondAsyncBeat = canvas->selectedTimeRange();
            if (!busHasExactValue(*firstAsyncBeat, "0x33")
                || !secondAsyncBeat
                || secondAsyncBeat->first != firstAsyncBeat->second
                || secondAsyncBeat->second - secondAsyncBeat->first
                    != firstAsyncBeat->second - firstAsyncBeat->first) {
                fail(QStringLiteral("Async Tab advance lost the off-grid beat offset"));
                return;
            }
            sendKey(directValue, Qt::Key_Escape);
            undoAction->trigger();
            asyncTimingAction->trigger();
            QCoreApplication::processEvents();
            if (canvas->asynchronousEditing() || !assertBusEmpty()) {
                fail(QStringLiteral("Async sequential Bus entry did not restore cleanly"));
                return;
            }

            canvas->revealLocation(
                QString::fromStdString(quickBus.id),
                window.project().scenarios.front().duration - 1);
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_Return);
            QCoreApplication::processEvents();
            const auto finalBusBeat = canvas->selectedTimeRange();
            directValue->setText(QStringLiteral("0x44"));
            directValue->setModified(true);
            sendKey(directValue, Qt::Key_Tab);
            QCoreApplication::processEvents();
            if (!finalBusBeat
                || !busHasExactValue(*finalBusBeat, "0x44")
                || canvas->selectedTimeRange() != finalBusBeat
                || !presetPalette->isVisible()
                || !directValue->hasFocus()
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("End"))) {
                fail(QStringLiteral("Tab at timeline End did not apply in place with clear feedback"));
                return;
            }
            sendKey(directValue, Qt::Key_Escape);
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (!assertBusEmpty()) {
                fail(QStringLiteral("Timeline-end Bus entry was not undoable"));
                return;
            }

            canvas->revealLocation(
                QString::fromStdString(quickBus.id),
                0);
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_Return);
            QCoreApplication::processEvents();
            const auto initialBusBeat = canvas->selectedTimeRange();
            directValue->setText(QStringLiteral("0x55"));
            directValue->setModified(true);
            sendKey(directValue, Qt::Key_Backtab, Qt::ShiftModifier);
            QCoreApplication::processEvents();
            if (!initialBusBeat
                || !busHasExactValue(*initialBusBeat, "0x55")
                || canvas->selectedTimeRange() != initialBusBeat
                || !presetPalette->isVisible()
                || !directValue->hasFocus()
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("start"))) {
                fail(QStringLiteral("Shift+Tab at timeline start did not apply in place with clear feedback"));
                return;
            }
            sendKey(directValue, Qt::Key_Escape);
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (!assertBusEmpty()) {
                fail(QStringLiteral("Timeline-start Bus entry was not undoable"));
                return;
            }

            const auto undoTextBeforeBeatNavigation = undoAction->text();
            canvas->revealLocation(
                QString::fromStdString(quickBit.id),
                0);
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_Tab);
            QCoreApplication::processEvents();
            const auto tabNavigatedBitBeat = canvas->selectedTimeRange();
            if (canvas->selectedLaneId()
                    != QString::fromStdString(quickBit.id)
                || !tabNavigatedBitBeat
                || tabNavigatedBitBeat->first <= 0
                || tabNavigatedBitBeat->second
                    - tabNavigatedBitBeat->first
                    != sequentialBeatWidth
                || undoAction->text() != undoTextBeforeBeatNavigation) {
                fail(QStringLiteral("Canvas Tab did not navigate one signal beat without history"));
                return;
            }
            sendKey(canvas, Qt::Key_Backtab, Qt::ShiftModifier);
            QCoreApplication::processEvents();
            const auto backtabNavigatedBitBeat = canvas->selectedTimeRange();
            if (!backtabNavigatedBitBeat
                || backtabNavigatedBitBeat->first != 0
                || backtabNavigatedBitBeat->second
                    - backtabNavigatedBitBeat->first
                    != sequentialBeatWidth
                || undoAction->text() != undoTextBeforeBeatNavigation) {
                fail(QStringLiteral("Canvas Shift+Tab did not navigate back one signal beat"));
                return;
            }
            sendKey(canvas, Qt::Key_Backtab, Qt::ShiftModifier);
            QCoreApplication::processEvents();
            if (canvas->selectedTimeRange() != backtabNavigatedBitBeat
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("start"))
                || undoAction->text() != undoTextBeforeBeatNavigation) {
                fail(QStringLiteral("Canvas Shift+Tab did not stop at timeline start"));
                return;
            }

            canvas->zoomIn();
            canvas->zoomIn();
            canvas->zoomIn();
            canvas->goToTick(0);
            canvas->setFocus(Qt::OtherFocusReason);
            QCoreApplication::processEvents();
            sendKey(canvas, Qt::Key_PageDown);
            QCoreApplication::processEvents();
            const auto pageDownTick = canvas->cursorTick();
            if (pageDownTick <= 0
                || pageDownTick >= window.project().scenarios.front().duration
                || canvas->selectedLaneId()
                    != QString::fromStdString(quickBit.id)
                || undoAction->text() != undoTextBeforeBeatNavigation
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Forward one visible page"))) {
                fail(QStringLiteral("PageDown did not advance the edit cursor by one visible page"));
                return;
            }
            sendKey(canvas, Qt::Key_PageUp);
            QCoreApplication::processEvents();
            if (canvas->cursorTick() != 0
                || undoAction->text() != undoTextBeforeBeatNavigation) {
                fail(QStringLiteral("PageUp did not return by one visible page"));
                return;
            }
            sendKey(canvas, Qt::Key_PageUp);
            QCoreApplication::processEvents();
            if (canvas->cursorTick() != 0
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Timeline start"))) {
                fail(QStringLiteral("PageUp did not stop at timeline start"));
                return;
            }
            canvas->fitScenario();
            QCoreApplication::processEvents();

            const auto clockOverrideAtZero = [&window, &quickClock]() {
                const auto* lane = wave::findLane(
                    window.project().scenarios.front(),
                    quickClock.id);
                if (!lane) return std::string{};
                const auto segment = std::find_if(
                    lane->segments.begin(),
                    lane->segments.end(),
                    [](const wave::Segment& candidate) {
                        return candidate.start <= 0 && 0 < candidate.end;
                    });
                return segment == lane->segments.end()
                    ? std::string{}
                    : segment->value;
            };
            canvas->revealLocation(
                QString::fromStdString(quickClock.id),
                0);
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_G);
            QCoreApplication::processEvents();
            if (clockOverrideAtZero() != "gated") {
                fail(QStringLiteral("Clock G did not gate the current period"));
                return;
            }
            sendKey(canvas, Qt::Key_X);
            QCoreApplication::processEvents();
            if (clockOverrideAtZero() != "disabled") {
                fail(QStringLiteral("Clock X did not drive the current period unknown"));
                return;
            }
            sendKey(canvas, Qt::Key_R);
            QCoreApplication::processEvents();
            if (!clockOverrideAtZero().empty()
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("restored normal clock waveform"))) {
                fail(QStringLiteral("Clock R did not restore the current period"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (clockOverrideAtZero() != "disabled") {
                fail(QStringLiteral("Undo did not restore the Clock X keyboard edit"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (clockOverrideAtZero() != "gated") {
                fail(QStringLiteral("Undo did not restore the Clock G keyboard edit"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (!clockOverrideAtZero().empty() || !redoAction->isEnabled()) {
                fail(QStringLiteral("Clock keyboard edits did not undo to the original waveform"));
                return;
            }
            const auto undoTextBeforeNormalClockRun = undoAction->text();
            sendKey(canvas, Qt::Key_R);
            QCoreApplication::processEvents();
            if (!clockOverrideAtZero().empty()
                || undoAction->text() != undoTextBeforeNormalClockRun
                || !redoAction->isEnabled()
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("no values changed"))) {
                fail(QStringLiteral("Clock R on a normal period changed history"));
                return;
            }

            sendMouse(
                QEvent::MouseButtonPress,
                paletteClick,
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                paletteClick,
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            directValue->setText(QStringLiteral("0x3c"));
            directValue->setModified(true);
            clickHeader(QPoint(80, laneCenter(quickBit.id)));
            QCoreApplication::processEvents();
            busAfterButtonClick = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (presetPalette->isVisible()
                || !busAfterButtonClick
                || busAfterButtonClick->segments.empty()
                || busAfterButtonClick->segments.front().value != "0x3c") {
                fail(QStringLiteral("Clicking elsewhere discarded an unsubmitted Bus draft"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();

            sendMouse(
                QEvent::MouseButtonPress,
                paletteClick,
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                paletteClick,
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            directValue->setText(QStringLiteral("0x4d"));
            directValue->setModified(true);
            presetPalette->hide();
            if (!canvas->commitPendingInlineEdits()) {
                fail(QStringLiteral("A hidden valid Bus draft could not be committed"));
                return;
            }
            busAfterButtonClick = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!busAfterButtonClick
                || busAfterButtonClick->segments.empty()
                || busAfterButtonClick->segments.front().value != "0x4d") {
                fail(QStringLiteral("A hidden valid Bus draft was ignored"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();

            sendMouse(
                QEvent::MouseButtonPress,
                paletteClick,
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                paletteClick,
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            directValue->setText(QStringLiteral("0x1ff"));
            directValue->setModified(true);
            presetPalette->hide();
            if (canvas->commitPendingInlineEdits()
                || !presetPalette->isVisible()
                || !directValue->hasFocus()
                || !directValue->isModified()) {
                fail(QStringLiteral("A hidden invalid Bus draft was not recoverable in place"));
                return;
            }
            directValue->setText(QStringLiteral("0x5e"));
            sendKey(directValue, Qt::Key_Return);
            QCoreApplication::processEvents();
            busAfterButtonClick = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (presetPalette->isVisible()
                || !busAfterButtonClick
                || busAfterButtonClick->segments.empty()
                || busAfterButtonClick->segments.front().value != "0x5e") {
                fail(QStringLiteral("A recovered hidden Bus draft could not be corrected"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();

            auto* rangeDurationEdit = canvas->findChild<QLineEdit*>(
                QStringLiteral("TimelineDurationEdit"));
            const auto originalDuration = window.project().scenarios.front().duration;
            const auto extendedDuration = originalDuration + 300'000;
            const auto staleDraftTick = originalDuration + 200'000;
            if (!rangeDurationEdit) {
                fail(QStringLiteral("Timeline End editor is unavailable for Bus range regression"));
                return;
            }
            rangeDurationEdit->setText(QString::fromStdString(
                wave::formatTick(extendedDuration, window.project().timeBase)));
            rangeDurationEdit->setModified(true);
            rangeDurationEdit->setFocus(Qt::OtherFocusReason);
            sendKey(rangeDurationEdit, Qt::Key_Return);
            QCoreApplication::processEvents();
            if (window.project().scenarios.front().duration != extendedDuration) {
                fail(QStringLiteral("Could not extend End for Bus range regression"));
                return;
            }
            const auto staleDraftX = 190 + static_cast<int>(std::llround(
                static_cast<double>(staleDraftTick)
                / static_cast<double>(extendedDuration)
                * static_cast<double>(canvas->viewport()->width() - 190)))
                - canvas->horizontalScrollBar()->value();
            const QPoint staleDraftPoint(staleDraftX, laneCenter(quickBus.id));
            sendMouse(
                QEvent::MouseButtonPress,
                staleDraftPoint,
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                staleDraftPoint,
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            if (!presetPalette->isVisible()) {
                fail(QStringLiteral("Bus controls were unavailable for range regression"));
                return;
            }
            directValue->setText(QStringLiteral("0x6f"));
            directValue->setModified(true);
            presetPalette->hide();
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (window.project().scenarios.front().duration != originalDuration
                || canvas->commitPendingInlineEdits()
                || !presetPalette->isVisible()
                || !directValue->hasFocus()
                || !directValue->isModified()
                || !directValue->toolTip().contains(QStringLiteral("beyond End"))) {
                fail(QStringLiteral("An out-of-range Bus draft was not blocked after End undo"));
                return;
            }
            busAfterButtonClick = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!busAfterButtonClick
                || std::any_of(
                    busAfterButtonClick->segments.begin(),
                    busAfterButtonClick->segments.end(),
                    [](const wave::Segment& segment) { return segment.value == "0x6f"; })) {
                fail(QStringLiteral("An out-of-range Bus draft was clamped into the timeline"));
                return;
            }
            rangeDurationEdit->setText(QString::fromStdString(
                wave::formatTick(extendedDuration, window.project().timeBase)));
            rangeDurationEdit->setModified(true);
            rangeDurationEdit->setFocus(Qt::OtherFocusReason);
            sendKey(rangeDurationEdit, Qt::Key_Return);
            QCoreApplication::processEvents();
            busAfterButtonClick = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            const auto recoveredAtOriginalTick = busAfterButtonClick
                && std::any_of(
                    busAfterButtonClick->segments.begin(),
                    busAfterButtonClick->segments.end(),
                    [staleDraftTick](const wave::Segment& segment) {
                        return segment.start <= staleDraftTick
                            && staleDraftTick < segment.end
                            && segment.value == "0x6f";
                    });
            if (window.project().scenarios.front().duration != extendedDuration
                || presetPalette->isVisible()
                || !recoveredAtOriginalTick) {
                fail(QStringLiteral("Extending End did not recover the Bus draft at its original position"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            undoAction->trigger();
            QCoreApplication::processEvents();
            canvas->fitScenario();
            QCoreApplication::processEvents();
            busAfterButtonClick = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (window.project().scenarios.front().duration != originalDuration
                || !busAfterButtonClick
                || std::any_of(
                    busAfterButtonClick->segments.begin(),
                    busAfterButtonClick->segments.end(),
                    [](const wave::Segment& segment) { return segment.value == "0x6f"; })) {
                fail(QStringLiteral("Recovered Bus draft and End change were not independently undoable"));
                return;
            }

            QMimeData presetMime;
            presetMime.setData(
                QByteArrayLiteral("application/x-wave-workbench-bus-preset"),
                QByteArrayLiteral("dont-care"));
            const QPoint presetDropPoint(690, presetBusY);
            QDragEnterEvent dragEnter(
                presetDropPoint,
                Qt::CopyAction,
                &presetMime,
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(canvas->viewport(), &dragEnter);
            QDropEvent drop(
                QPointF(presetDropPoint),
                Qt::CopyAction,
                &presetMime,
                Qt::LeftButton,
                Qt::NoModifier);
            QCoreApplication::sendEvent(canvas->viewport(), &drop);
            QCoreApplication::processEvents();
            if (dragEnter.isAccepted() || drop.isAccepted()) {
                fail(QStringLiteral("Removed Bus preset drag/drop interaction is still active"));
                return;
            }

            sendMouse(
                QEvent::MouseButtonPress,
                presetDropPoint,
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                presetDropPoint,
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            auto* busToolbarAction = window.findChild<QAction*>(
                QStringLiteral("BusEditToolbarAction"));
            const auto paletteGlobal = QRect(
                presetPalette->mapToGlobal(QPoint(0, 0)),
                presetPalette->size());
            const auto viewportGlobal = QRect(
                canvas->viewport()->mapToGlobal(QPoint(0, 0)),
                canvas->viewport()->size());
            if (!presetPalette->isVisible()
                || !busToolbarAction
                || !busToolbarAction->isVisible()
                || paletteGlobal.intersects(viewportGlobal)
                || !contextLabel->text().contains(QStringLiteral("Beat"))) {
                fail(QStringLiteral("Bus editor is not fixed in the toolbar with an explicit Beat target"));
                return;
            }
            presetButtons.back()->click();
            QCoreApplication::processEvents();
            auto* busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            const auto presetSegment = busAfterPreset
                ? std::find_if(
                      busAfterPreset->segments.begin(),
                      busAfterPreset->segments.end(),
                      [](const wave::Segment& segment) {
                          const auto preset = segment.extensions.find(
                              "waveWorkbench.busPreset");
                          return preset != segment.extensions.end()
                              && preset->second == "\"dont-care\"";
                      })
                : std::vector<wave::Segment>::iterator{};
            const auto expectedBeat = wave::toTicks(
                10,
                wave::TimeUnit::Nanosecond,
                window.project().timeBase).value_or(0);
            const auto presetSelection = canvas->selectedTimeRange();
            if (!busAfterPreset
                || presetSegment == busAfterPreset->segments.end()
                || presetSegment->end - presetSegment->start != expectedBeat
                || presetSegment->value != "0bxxxxxxxx"
                || canvas->selectedSegmentId()
                    != QString::fromStdString(presetSegment->id)
                || !presetSelection
                || *presetSelection
                    != std::pair<wave::Tick, wave::Tick>{
                        presetSegment->start,
                        presetSegment->end,
                    }) {
                fail(QStringLiteral("Bus Don't care button did not create one semantic beat"));
                return;
            }
            const auto presetStart = presetSegment->start;
            undoAction->trigger();
            QCoreApplication::processEvents();
            busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!busAfterPreset || !busAfterPreset->segments.empty()) {
                fail(QStringLiteral("Bus Don't care undo failed"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();
            busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!busAfterPreset || busAfterPreset->segments.empty()) {
                fail(QStringLiteral("Bus Don't care redo failed"));
                return;
            }

            const auto timelineDuration = window.project().scenarios.front().duration;
            const auto tickX = [canvas, timelineDuration](const wave::Tick tick) {
                return 190 + static_cast<int>(std::llround(
                    static_cast<double>(tick)
                    / static_cast<double>(timelineDuration)
                    * static_cast<double>(canvas->viewport()->width() - 190)))
                    - canvas->horizontalScrollBar()->value();
            };
            const auto forwardCopyStart = presetStart + 3 * expectedBeat;
            const auto copyStart = forwardCopyStart + expectedBeat <= timelineDuration
                ? forwardCopyStart
                : presetStart - 3 * expectedBeat;
            const QPoint copySourcePoint(
                tickX(presetStart + expectedBeat / 2),
                presetBusY);
            const QPoint copyTargetPoint(
                tickX(copyStart + expectedBeat / 2),
                presetBusY);
            sendMouse(
                QEvent::MouseButtonPress,
                copySourcePoint,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::ControlModifier);
            sendMouse(
                QEvent::MouseMove,
                copyTargetPoint,
                Qt::NoButton,
                Qt::LeftButton,
                Qt::ControlModifier);
            QCoreApplication::processEvents();
            const auto copyPreview = canvas->selectedTimeRange();
            if (!copyPreview
                || copyPreview->first != copyStart
                || copyPreview->second != copyStart + expectedBeat) {
                fail(QStringLiteral("Ctrl+drag did not show a one-beat Sync copy preview"));
                return;
            }
            if (!window.statusBar()->currentMessage().contains(
                    QStringLiteral("width %1").arg(QString::fromStdString(
                        wave::formatTick(expectedBeat, window.project().timeBase))))) {
                fail(QStringLiteral("Segment drag preview did not expose its exact width"));
                return;
            }
            if (!canvasAddLaneScreenshotPath.isEmpty()) {
                auto copyPreviewScreenshotPath = canvasAddLaneScreenshotPath;
                const auto suffix = copyPreviewScreenshotPath.lastIndexOf(QLatin1Char('.'));
                if (suffix >= 0) {
                    copyPreviewScreenshotPath.insert(
                        suffix,
                        QStringLiteral("-ctrl-drag-copy-preview"));
                } else {
                    copyPreviewScreenshotPath.append(
                        QStringLiteral("-ctrl-drag-copy-preview.png"));
                }
                if (!window.grab().save(copyPreviewScreenshotPath)) {
                    fail(QStringLiteral("Cannot save Ctrl+drag copy preview screenshot"));
                    return;
                }
            }
            sendMouse(
                QEvent::MouseButtonRelease,
                copyTargetPoint,
                Qt::LeftButton,
                Qt::NoButton,
                Qt::ControlModifier);
            QCoreApplication::processEvents();
            busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            const auto sourceAfterCopy = busAfterPreset
                ? std::find_if(
                      busAfterPreset->segments.begin(),
                      busAfterPreset->segments.end(),
                      [presetStart](const wave::Segment& segment) {
                          return segment.start <= presetStart
                              && presetStart < segment.end;
                      })
                : std::vector<wave::Segment>::iterator{};
            const auto targetAfterCopy = busAfterPreset
                ? std::find_if(
                      busAfterPreset->segments.begin(),
                      busAfterPreset->segments.end(),
                      [copyStart](const wave::Segment& segment) {
                          return segment.start <= copyStart
                              && copyStart < segment.end;
                      })
                : std::vector<wave::Segment>::iterator{};
            if (!busAfterPreset
                || sourceAfterCopy == busAfterPreset->segments.end()
                || targetAfterCopy == busAfterPreset->segments.end()
                || sourceAfterCopy->extensions.find("waveWorkbench.busPreset")
                    == sourceAfterCopy->extensions.end()
                || targetAfterCopy->extensions.find("waveWorkbench.busPreset")
                    == targetAfterCopy->extensions.end()) {
                fail(QStringLiteral("Ctrl+drag copy did not retain source and semantic metadata"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!busAfterPreset
                || std::any_of(
                    busAfterPreset->segments.begin(),
                    busAfterPreset->segments.end(),
                    [copyStart](const wave::Segment& segment) {
                        return segment.start <= copyStart && copyStart < segment.end;
                    })) {
                fail(QStringLiteral("Ctrl+drag copy undo did not remove only the target"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();

            const auto chooseDuplicateAfter = [&application, canvas](
                                                  const QPoint& position) {
                bool handled = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&application, &handled] {
                        auto* menu = qobject_cast<QMenu*>(
                            QApplication::activePopupWidget());
                        QAction* duplicate = nullptr;
                        if (menu
                            && menu->objectName()
                                == QStringLiteral("WaveformContextMenu")) {
                            duplicate = menu->findChild<QAction*>(
                                QStringLiteral("DuplicateSegmentAfterAction"));
                            if (!duplicate) {
                                for (auto* action : menu->actions()) {
                                    if (action
                                        && action->objectName()
                                            == QStringLiteral(
                                                "DuplicateSegmentAfterAction")) {
                                        duplicate = action;
                                        break;
                                    }
                                }
                            }
                        }
                        if (!menu || !duplicate) {
                            if (menu) menu->close();
                            return;
                        }
                        menu->setActiveAction(duplicate);
                        handled = true;
                        QKeyEvent enter(
                            QEvent::KeyPress,
                            Qt::Key_Return,
                            Qt::NoModifier);
                        QCoreApplication::sendEvent(menu, &enter);
                    });
                QContextMenuEvent context(
                    QContextMenuEvent::Mouse,
                    position,
                    canvas->viewport()->mapToGlobal(position));
                QCoreApplication::sendEvent(canvas->viewport(), &context);
                QCoreApplication::processEvents();
                return handled;
            };
            const auto beforeAdjacentDuplicate =
                window.project().scenarios.front();
            if (!chooseDuplicateAfter(copySourcePoint)) {
                fail(QStringLiteral("Bus context menu did not expose duplicate-after"));
                return;
            }
            const auto adjacentStart = presetStart + expectedBeat;
            busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            const auto adjacentDuplicate = busAfterPreset
                ? std::find_if(
                      busAfterPreset->segments.begin(),
                      busAfterPreset->segments.end(),
                      [adjacentStart](const wave::Segment& candidate) {
                          return candidate.start <= adjacentStart
                              && adjacentStart < candidate.end;
                      })
                : std::vector<wave::Segment>::iterator{};
            if (!busAfterPreset
                || adjacentDuplicate == busAfterPreset->segments.end()
                || adjacentDuplicate->value != "0bxxxxxxxx"
                || adjacentDuplicate->extensions.find(
                       "waveWorkbench.busPreset")
                    == adjacentDuplicate->extensions.end()
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("segment duplicated"))) {
                fail(QStringLiteral("Duplicate-after did not copy the adjacent value and metadata"));
                return;
            }
            const auto afterAdjacentDuplicate =
                window.project().scenarios.front();
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (window.project().scenarios.front() != beforeAdjacentDuplicate) {
                fail(QStringLiteral("Duplicate-after was not one atomic Undo"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();
            if (window.project().scenarios.front() != afterAdjacentDuplicate) {
                fail(QStringLiteral("Duplicate-after atomic Redo failed"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();

            const auto beforeExactSegmentSelection =
                window.project().scenarios.front();
            sendMouse(
                QEvent::MouseButtonPress,
                copySourcePoint,
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                copySourcePoint,
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            const auto exactSegmentSelectionStatus =
                window.statusBar()->currentMessage();
            if (window.project().scenarios.front()
                    != beforeExactSegmentSelection
                || canvas->selectedTimeRange()
                    != std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{
                            presetStart,
                            presetStart + expectedBeat}}
                || !exactSegmentSelectionStatus.contains(
                    QString::fromStdString(quickBus.name))
                || !exactSegmentSelectionStatus.contains(
                    QStringLiteral("value 0bxxxxxxxx"))
                || !exactSegmentSelectionStatus.contains(
                    QStringLiteral("width %1").arg(QString::fromStdString(
                        wave::formatTick(
                            expectedBeat,
                            window.project().timeBase))))
                || !exactSegmentSelectionStatus.contains(
                    QStringLiteral("Ctrl+D copies after"))
                || !exactSegmentSelectionStatus.contains(
                    QStringLiteral("Ctrl+Shift+D copies before"))) {
                qCritical().noquote()
                    << "Exact Segment selection diagnostics"
                    << "modelChanged"
                    << (window.project().scenarios.front()
                        != beforeExactSegmentSelection)
                    << "range"
                    << (canvas->selectedTimeRange()
                            ? QStringLiteral("%1-%2")
                                  .arg(canvas->selectedTimeRange()->first)
                                  .arg(canvas->selectedTimeRange()->second)
                            : QStringLiteral("<none>"))
                    << "expected"
                    << QStringLiteral("%1-%2")
                           .arg(presetStart)
                           .arg(presetStart + expectedBeat)
                    << "status" << exactSegmentSelectionStatus;
                fail(QStringLiteral(
                    "Single-click Segment selection did not provide exact, history-free feedback"));
                return;
            }
            const auto laneCountBeforeSegmentShortcut =
                window.project().scenarios.front().lanes.size();
            const auto beforeSegmentShortcut =
                window.project().scenarios.front();
            duplicateLaneAction->trigger();
            QCoreApplication::processEvents();
            busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            const auto shortcutDuplicate = busAfterPreset
                ? std::find_if(
                      busAfterPreset->segments.begin(),
                      busAfterPreset->segments.end(),
                      [adjacentStart](const wave::Segment& candidate) {
                          return candidate.start <= adjacentStart
                              && adjacentStart < candidate.end;
                      })
                : std::vector<wave::Segment>::iterator{};
            if (window.project().scenarios.front().lanes.size()
                    != laneCountBeforeSegmentShortcut
                || !busAfterPreset
                || shortcutDuplicate == busAfterPreset->segments.end()
                || shortcutDuplicate->value != "0bxxxxxxxx"
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("segment duplicated"))) {
                fail(QStringLiteral("Ctrl+D copied the lane instead of the selected Segment"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (window.project().scenarios.front() != beforeSegmentShortcut) {
                fail(QStringLiteral("Ctrl+D Segment duplicate was not one atomic Undo"));
                return;
            }

            if (presetStart < expectedBeat) {
                fail(QStringLiteral("Segment fixture has no room for duplicate-before"));
                return;
            }
            sendMouse(
                QEvent::MouseButtonPress,
                copySourcePoint,
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                copySourcePoint,
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            const auto beforePreviousDuplicate =
                window.project().scenarios.front();
            sendKey(
                canvas,
                Qt::Key_D,
                Qt::ControlModifier | Qt::ShiftModifier);
            QCoreApplication::processEvents();
            const auto previousStart = presetStart - expectedBeat;
            busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            const auto previousDuplicate = busAfterPreset
                ? std::find_if(
                      busAfterPreset->segments.begin(),
                      busAfterPreset->segments.end(),
                      [previousStart](const wave::Segment& candidate) {
                          return candidate.start <= previousStart
                              && previousStart < candidate.end;
                      })
                : std::vector<wave::Segment>::iterator{};
            if (!busAfterPreset
                || previousDuplicate == busAfterPreset->segments.end()
                || previousDuplicate->value != "0bxxxxxxxx"
                || previousDuplicate->extensions.find(
                       "waveWorkbench.busPreset")
                    == previousDuplicate->extensions.end()
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("duplicated before"))) {
                fail(QStringLiteral("Ctrl+Shift+D did not duplicate the Segment before"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (window.project().scenarios.front() != beforePreviousDuplicate) {
                fail(QStringLiteral("Duplicate-before was not one atomic Undo"));
                return;
            }

            bool duplicateDirectionsVisible = false;
            QTimer::singleShot(
                0,
                &application,
                [&duplicateDirectionsVisible] {
                    auto* menu = qobject_cast<QMenu*>(
                        QApplication::activePopupWidget());
                    auto hasBefore = false;
                    auto hasAfter = false;
                    if (menu
                        && menu->objectName()
                            == QStringLiteral("WaveformContextMenu")) {
                        for (auto* action : menu->actions()) {
                            if (!action) continue;
                            hasBefore = hasBefore
                                || action->objectName()
                                    == QStringLiteral(
                                        "DuplicateSegmentBeforeAction");
                            hasAfter = hasAfter
                                || action->objectName()
                                    == QStringLiteral(
                                        "DuplicateSegmentAfterAction");
                        }
                    }
                    duplicateDirectionsVisible = hasBefore && hasAfter;
                    if (menu) menu->close();
                });
            QContextMenuEvent duplicateDirectionsContext(
                QContextMenuEvent::Mouse,
                copySourcePoint,
                canvas->viewport()->mapToGlobal(copySourcePoint));
            QCoreApplication::sendEvent(
                canvas->viewport(),
                &duplicateDirectionsContext);
            QCoreApplication::processEvents();
            if (!duplicateDirectionsVisible) {
                fail(QStringLiteral("Segment context menu does not expose both duplicate directions"));
                return;
            }

            const auto beforeSegmentNavigation =
                window.project().scenarios.front();
            busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            const auto navigationSource = busAfterPreset
                ? std::find_if(
                      busAfterPreset->segments.begin(),
                      busAfterPreset->segments.end(),
                      [presetStart](const wave::Segment& candidate) {
                          return candidate.start <= presetStart
                              && presetStart < candidate.end;
                      })
                : std::vector<wave::Segment>::iterator{};
            if (!busAfterPreset
                || navigationSource == busAfterPreset->segments.end()
                || std::next(navigationSource) == busAfterPreset->segments.end()) {
                fail(QStringLiteral("Segment navigation fixture is incomplete"));
                return;
            }
            const auto navigationTargetRange = std::pair{
                std::next(navigationSource)->start,
                std::next(navigationSource)->end,
            };
            sendKey(canvas, Qt::Key_Tab, Qt::ControlModifier);
            QCoreApplication::processEvents();
            if (canvas->selectedTimeRange()
                    != std::optional<std::pair<wave::Tick, wave::Tick>>{
                        navigationTargetRange}
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Next Segment"))) {
                fail(QStringLiteral("Ctrl+Tab did not select the next explicit Segment"));
                return;
            }
            sendKey(
                canvas,
                Qt::Key_Tab,
                Qt::ControlModifier | Qt::ShiftModifier);
            QCoreApplication::processEvents();
            if (canvas->selectedTimeRange()
                    != std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{
                            navigationSource->start,
                            navigationSource->end}}
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Previous Segment"))
                || window.project().scenarios.front()
                    != beforeSegmentNavigation) {
                fail(QStringLiteral("Ctrl+Shift+Tab did not return to the previous Segment"));
                return;
            }
            sendKey(
                canvas,
                Qt::Key_Tab,
                Qt::ControlModifier | Qt::ShiftModifier);
            QCoreApplication::processEvents();
            if (!window.statusBar()->currentMessage().contains(
                    QStringLiteral("No previous Segment"))
                || window.project().scenarios.front()
                    != beforeSegmentNavigation) {
                fail(QStringLiteral("Segment navigation boundary changed the model"));
                return;
            }

            const auto nudgeSourceValue = navigationSource->value;
            const auto nudgeSourceExtensions = navigationSource->extensions;
            const auto beforeSegmentNudge =
                window.project().scenarios.front();
            sendKey(
                canvas,
                Qt::Key_Right,
                Qt::AltModifier);
            QCoreApplication::processEvents();
            busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            const auto nudgedLater = busAfterPreset
                ? std::find_if(
                      busAfterPreset->segments.begin(),
                      busAfterPreset->segments.end(),
                      [presetStart, expectedBeat](
                          const wave::Segment& candidate) {
                          return candidate.start == presetStart + expectedBeat
                              && candidate.end
                                  == presetStart + 2 * expectedBeat;
                      })
                : std::vector<wave::Segment>::iterator{};
            if (!busAfterPreset
                || nudgedLater == busAfterPreset->segments.end()
                || nudgedLater->value != nudgeSourceValue
                || nudgedLater->extensions != nudgeSourceExtensions
                || canvas->selectedTimeRange()
                    != std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{
                            presetStart + expectedBeat,
                            presetStart + 2 * expectedBeat}}
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("nudged later"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("step %1").arg(
                        QString::fromStdString(wave::formatTick(
                            expectedBeat,
                            window.project().timeBase))))) {
                fail(QStringLiteral("Alt+Right did not nudge the selected Segment by one Sync beat"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (window.project().scenarios.front() != beforeSegmentNudge) {
                fail(QStringLiteral("Alt+Right Segment nudge was not one atomic Undo"));
                return;
            }

            sendMouse(
                QEvent::MouseButtonPress,
                copySourcePoint,
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                copySourcePoint,
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            sendKey(
                canvas,
                Qt::Key_Left,
                Qt::AltModifier);
            QCoreApplication::processEvents();
            busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            const auto nudgedEarlier = busAfterPreset
                ? std::find_if(
                      busAfterPreset->segments.begin(),
                      busAfterPreset->segments.end(),
                      [presetStart, expectedBeat](
                          const wave::Segment& candidate) {
                          return candidate.start == presetStart - expectedBeat
                              && candidate.end == presetStart;
                      })
                : std::vector<wave::Segment>::iterator{};
            if (!busAfterPreset
                || nudgedEarlier == busAfterPreset->segments.end()
                || nudgedEarlier->value != nudgeSourceValue
                || nudgedEarlier->extensions != nudgeSourceExtensions
                || canvas->selectedTimeRange()
                    != std::optional<std::pair<wave::Tick, wave::Tick>>{
                        std::pair<wave::Tick, wave::Tick>{
                            presetStart - expectedBeat,
                            presetStart}}
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("nudged earlier"))) {
                fail(QStringLiteral("Alt+Left did not nudge the selected Segment by one Sync beat"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (window.project().scenarios.front() != beforeSegmentNudge) {
                fail(QStringLiteral("Alt+Left Segment nudge was not one atomic Undo"));
                return;
            }

            const QPoint finalBeatPoint(
                tickX(timelineDuration - expectedBeat / 2),
                presetBusY);
            sendMouse(
                QEvent::MouseButtonPress,
                finalBeatPoint,
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                finalBeatPoint,
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            if (!presetPalette->isVisible()) {
                fail(QStringLiteral("Could not target the final Bus beat"));
                return;
            }
            presetButtons.back()->click();
            QCoreApplication::processEvents();
            const auto beforeBoundaryDuplicate =
                window.project().scenarios.front();
            const auto undoTextBeforeBoundary = undoAction->text();
            if (!chooseDuplicateAfter(finalBeatPoint)
                || window.project().scenarios.front() != beforeBoundaryDuplicate
                || undoAction->text() != undoTextBeforeBoundary
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("exceeds End"))) {
                fail(QStringLiteral("Boundary duplicate did not remain history-free with feedback"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();

            const auto edgeX = 190 + static_cast<int>(std::llround(
                static_cast<double>(presetStart)
                / static_cast<double>(window.project().scenarios.front().duration)
                * static_cast<double>(canvas->viewport()->width() - 190)))
                - canvas->horizontalScrollBar()->value();
            sendMouse(
                QEvent::MouseMove,
                QPoint(edgeX + 24, presetBusY),
                Qt::NoButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            if (canvas->cursorTick() % expectedBeat != 0) {
                fail(QStringLiteral("Default Sync mode did not hard-quantize the edit cursor to a beat"));
                return;
            }

            asyncTimingAction->trigger();
            QCoreApplication::processEvents();
            if (!asyncTimingAction->isChecked()
                || asyncTimingAction->text() != QStringLiteral("Async")
                || !canvas->asynchronousEditing()) {
                fail(QStringLiteral("Async mode did not activate from the toolbar"));
                return;
            }
            auto capturedNearbyEdge = false;
            for (auto offset = -16; offset <= 16 && !capturedNearbyEdge; ++offset) {
                sendMouse(
                    QEvent::MouseMove,
                    QPoint(edgeX + offset, presetBusY),
                    Qt::NoButton,
                    Qt::NoButton);
                QCoreApplication::processEvents();
                capturedNearbyEdge = canvas->cursorTick() == presetStart;
            }
            if (!capturedNearbyEdge) {
                fail(QStringLiteral("Async light snapping did not capture a nearby signal edge"));
                return;
            }
            sendMouse(
                QEvent::MouseMove,
                QPoint(edgeX + 24, presetBusY),
                Qt::NoButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            if (canvas->cursorTick() == presetStart) {
                fail(QStringLiteral("Async light snapping remained active outside its pixel radius"));
                return;
            }

            constexpr wave::Tick rulerTick = 104'000;
            const auto rulerX = 190 + static_cast<int>(std::llround(
                static_cast<double>(rulerTick)
                / static_cast<double>(window.project().scenarios.front().duration)
                * static_cast<double>(canvas->viewport()->width() - 190)))
                - canvas->horizontalScrollBar()->value();
            auto capturedNearbyTick = false;
            for (auto offset = -16; offset <= 16 && !capturedNearbyTick; ++offset) {
                sendMouse(
                    QEvent::MouseMove,
                    QPoint(rulerX + offset, presetBusY),
                    Qt::NoButton,
                    Qt::NoButton);
                QCoreApplication::processEvents();
                capturedNearbyTick = canvas->cursorTick() == rulerTick;
            }
            if (!capturedNearbyTick) {
                fail(QStringLiteral("Async light snapping did not capture a nearby ruler tick"));
                return;
            }
            asyncTimingAction->trigger();
            QCoreApplication::processEvents();
            if (canvas->asynchronousEditing()
                || asyncTimingAction->text() != QStringLiteral("Sync")) {
                fail(QStringLiteral("Sync mode did not restore from the toolbar"));
                return;
            }
            bool waveformMenuHandled = false;
            QTimer::singleShot(0, &application, [&application, &waveformMenuHandled] {
                auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                QAction* setZ = nullptr;
                if (menu && menu->objectName() == QStringLiteral("WaveformContextMenu")) {
                    for (auto* action : menu->actions()) {
                        if (action && action->text() == QStringLiteral("Set beat to Z")) {
                            setZ = action;
                            break;
                        }
                    }
                }
                if (!menu || !setZ) {
                    qCritical().noquote() << "Waveform context menu Bus actions are missing";
                    if (menu) menu->close();
                    application.exit(4);
                    return;
                }
                menu->setActiveAction(setZ);
                waveformMenuHandled = true;
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(menu, &enter);
            });
            const QPoint waveformContextPoint(rulerX + 90, presetBusY);
            QContextMenuEvent waveformContext(
                QContextMenuEvent::Mouse,
                waveformContextPoint,
                canvas->viewport()->mapToGlobal(waveformContextPoint));
            QCoreApplication::sendEvent(canvas->viewport(), &waveformContext);
            QCoreApplication::processEvents();
            auto* busAfterContext = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            const auto zSegment = busAfterContext
                ? std::find_if(
                      busAfterContext->segments.begin(),
                      busAfterContext->segments.end(),
                      [](const wave::Segment& segment) {
                          const auto preset = segment.extensions.find(
                              "waveWorkbench.busPreset");
                          return preset != segment.extensions.end()
                              && preset->second == "\"z\"";
                      })
                : std::vector<wave::Segment>::iterator{};
            if (!waveformMenuHandled
                || !busAfterContext
                || zSegment == busAfterContext->segments.end()) {
                fail(QStringLiteral("Waveform right-click did not apply the selected Bus value"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();

            canvas->revealLocation(QString::fromStdString(quickBit.id), 0);
            QCoreApplication::processEvents();
            auto bitY = laneCenter(quickBit.id);
            window.activateWindow();
            canvas->setFocus(Qt::OtherFocusReason);
            QCoreApplication::processEvents();
            sendMouse(
                QEvent::MouseButtonDblClick,
                QPoint(80, bitY),
                Qt::LeftButton,
                Qt::LeftButton);
            QCoreApplication::processEvents();
            auto* renameEdit = canvas->findChild<QLineEdit*>(QStringLiteral("LaneRenameEdit"));
            if (!renameEdit) {
                fail(QStringLiteral("Double-click signal rename editor is missing"));
                return;
            }
            if (!renameEdit->isVisible()) {
                fail(QStringLiteral("Double-click signal rename editor is not visible"));
                return;
            }
            if (QApplication::activeModalWidget()) {
                fail(QStringLiteral("Double-click signal rename opened a modal widget"));
                return;
            }
            if (!renameEdit->hasFocus()) {
                const auto* focus = QApplication::focusWidget();
                fail(QStringLiteral("Double-click signal rename focus remained on %1")
                         .arg(focus ? focus->objectName() : QStringLiteral("<none>")));
                return;
            }
            const auto renameEntryStatus = window.statusBar()->currentMessage();
            if (!renameEntryStatus.contains(QStringLiteral("Rename signal"))
                || !renameEntryStatus.contains(QString::fromStdString(quickBit.name))
                || !renameEntryStatus.contains(QStringLiteral("Enter"))
                || !renameEntryStatus.contains(QStringLiteral("Esc"))) {
                fail(QStringLiteral("Inline rename entry did not identify its signal and recovery keys"));
                return;
            }
            renameEdit->setText(QStringLiteral("cancelled_bit"));
            sendKey(renameEdit, Qt::Key_Escape);
            QCoreApplication::processEvents();
            auto* renamedBit = wave::findLane(
                window.project().scenarios.front(), quickBit.id);
            const auto renameCancelStatus = window.statusBar()->currentMessage();
            if (renameEdit->isVisible()
                || !renamedBit
                || renamedBit->name != quickBit.name
                || !renameCancelStatus.contains(QStringLiteral("Rename cancelled"))
                || !renameCancelStatus.contains(QStringLiteral("Selected signal"))
                || !renameCancelStatus.contains(QString::fromStdString(quickBit.name))
                || !renameCancelStatus.contains(QStringLiteral("Delete removes signal"))
                || !renameCancelStatus.contains(QStringLiteral("F2 renames"))) {
                fail(QStringLiteral("Escape did not cancel rename and restore the signal target"));
                return;
            }

            clickHeader(QPoint(80, bitY));
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_F2);
            QCoreApplication::processEvents();
            if (!renameEdit->isVisible()
                || !renameEdit->hasFocus()
                || QApplication::activeModalWidget()) {
                fail(QStringLiteral("F2 did not open inline signal rename"));
                return;
            }
            renameEdit->setText(QString::fromStdString(quickClock.name));
            sendKey(renameEdit, Qt::Key_Return);
            QCoreApplication::processEvents();
            renamedBit = wave::findLane(window.project().scenarios.front(), quickBit.id);
            if (!renameEdit->isVisible()
                || !renameEdit->hasFocus()
                || !renamedBit
                || renamedBit->name != quickBit.name
                || !renameEdit->toolTip().contains(QStringLiteral("already uses"))) {
                fail(QStringLiteral("Duplicate inline signal name was not recoverable in place"));
                return;
            }
            const auto laneCountBeforeBlockedAdd = window.project().scenarios.front().lanes.size();
            addButtons.at(1)->click();
            QCoreApplication::processEvents();
            if (!renameEdit->isVisible()
                || !renameEdit->hasFocus()
                || window.project().scenarios.front().lanes.size() != laneCountBeforeBlockedAdd) {
                fail(QStringLiteral("Invalid inline rename did not block quick-add safely"));
                return;
            }

            renameEdit->setText(QStringLiteral(" "));
            sendKey(renameEdit, Qt::Key_Return);
            QCoreApplication::processEvents();
            if (!renameEdit->isVisible()
                || !renameEdit->hasFocus()
                || !renameEdit->toolTip().contains(QStringLiteral("empty"))) {
                fail(QStringLiteral("Empty inline signal name was not recoverable in place"));
                return;
            }
            renameEdit->setText(QStringLiteral("renamed_bit"));
            sendKey(renameEdit, Qt::Key_Return);
            QCoreApplication::processEvents();
            renamedBit = wave::findLane(window.project().scenarios.front(), quickBit.id);
            if (renameEdit->isVisible()
                || !renamedBit
                || renamedBit->name != "renamed_bit"
                || !window.statusBar()->currentMessage().contains(QStringLiteral("Ctrl+Z"))) {
                fail(QStringLiteral("Inline signal rename did not commit with visible undo feedback"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            renamedBit = wave::findLane(window.project().scenarios.front(), quickBit.id);
            if (!renamedBit || renamedBit->name != quickBit.name) {
                fail(QStringLiteral("Inline signal rename was not one undoable command"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();
            renamedBit = wave::findLane(window.project().scenarios.front(), quickBit.id);
            if (!renamedBit || renamedBit->name != "renamed_bit") {
                fail(QStringLiteral("Inline signal rename could not be redone"));
                return;
            }

            canvas->zoomIn();
            canvas->zoomIn();
            canvas->zoomIn();
            QCoreApplication::processEvents();
            if (canvas->horizontalScrollBar()->maximum() <= 0) {
                fail(QStringLiteral("Rename viewport regression setup could not create horizontal scroll"));
                return;
            }
            canvas->horizontalScrollBar()->setValue(canvas->horizontalScrollBar()->maximum());
            const auto renameScrollPosition = canvas->horizontalScrollBar()->value();

            clickHeader(QPoint(80, bitY));
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_F2);
            QCoreApplication::processEvents();
            renameEdit->setText(QStringLiteral("blurred_bit"));
            clickHeader(QPoint(80, bitY));
            QCoreApplication::processEvents();
            renamedBit = wave::findLane(window.project().scenarios.front(), quickBit.id);
            if (renameEdit->isVisible()
                || !renamedBit
                || renamedBit->name != "blurred_bit"
                || canvas->horizontalScrollBar()->value() != renameScrollPosition) {
                fail(QStringLiteral("Click-submitted rename changed the viewport or result"));
                return;
            }
            canvas->fitScenario();
            QCoreApplication::processEvents();
            undoAction->trigger();
            QCoreApplication::processEvents();
            renamedBit = wave::findLane(window.project().scenarios.front(), quickBit.id);
            if (!renamedBit || renamedBit->name != "renamed_bit") {
                fail(QStringLiteral("Blur-submitted rename was not undoable"));
                return;
            }

            clickHeader(QPoint(80, bitY));
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_F2);
            QCoreApplication::processEvents();
            renameEdit->setText(QStringLiteral("focusout_bit"));
            addButtons.at(0)->setFocus(Qt::OtherFocusReason);
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            renamedBit = wave::findLane(window.project().scenarios.front(), quickBit.id);
            if (renameEdit->isVisible() || !renamedBit || renamedBit->name != "focusout_bit") {
                fail(QStringLiteral("Real focus loss did not submit inline signal rename"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            renamedBit = wave::findLane(window.project().scenarios.front(), quickBit.id);
            if (!renamedBit || renamedBit->name != "renamed_bit") {
                fail(QStringLiteral("Focus-loss rename was not undoable"));
                return;
            }

            const auto editFromContext = [
                                             &application,
                                             canvas,
                                             &laneCenter,
                                             &window](
                                             const std::string& laneId,
                                             const std::function<bool(QDialog*)>& editor) {
                canvas->revealLocation(QString::fromStdString(laneId), 0);
                QCoreApplication::processEvents();
                const auto y = laneCenter(laneId);
                if (y < 40 || y >= canvas->viewport()->height()) return false;
                bool menuHandled = false;
                bool dialogHandled = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&application, &menuHandled, &dialogHandled, editor] {
                        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                        auto* action = menu
                            ? menu->findChild<QAction*>(
                                  QStringLiteral("QuickLaneParametersAction"))
                            : nullptr;
                        if (!menu
                            || menu->objectName() != QStringLiteral("LaneHeaderContextMenu")
                            || !action) {
                            if (menu) menu->close();
                            return;
                        }
                        QTimer::singleShot(0, &application, [&dialogHandled, editor] {
                            auto* dialog = qobject_cast<QDialog*>(
                                QApplication::activeModalWidget());
                            if (!dialog || !editor(dialog)) {
                                if (dialog) dialog->reject();
                                return;
                            }
                            auto* buttons = dialog->findChild<QDialogButtonBox*>();
                            auto* ok = buttons
                                ? buttons->button(QDialogButtonBox::Ok)
                                : nullptr;
                            if (!ok) {
                                dialog->reject();
                                return;
                            }
                            dialogHandled = true;
                            ok->click();
                        });
                        action->trigger();
                        menuHandled = true;
                        menu->close();
                    });
                const QPoint localPosition(80, y);
                QContextMenuEvent contextEvent(
                    QContextMenuEvent::Mouse,
                    localPosition,
                    canvas->viewport()->mapToGlobal(localPosition));
                QCoreApplication::sendEvent(canvas->viewport(), &contextEvent);
                QCoreApplication::processEvents();
                return menuHandled && dialogHandled;
            };

            const auto busWidthBeforeEdit = quickBus.width;
            if (!editFromContext(quickBus.id, [](QDialog* dialog) {
                    auto* width = dialog->findChild<QSpinBox*>(
                        QStringLiteral("BusWidthSpin"));
                    if (dialog->objectName() != QStringLiteral("QuickLaneParametersDialog")
                        || !width) {
                        return false;
                    }
                    width->setValue(16);
                    return true;
                })) {
                fail(QStringLiteral("Bus right-click parameter editor did not open"));
                return;
            }
            const auto* editedBus = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!editedBus
                || editedBus->width != 16
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("width 16"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Z"))) {
                fail(QStringLiteral("Bus right-click parameters lacked exact result feedback"));
                return;
            }
            if (!editFromContext(quickBus.id, [](QDialog* dialog) {
                    const auto* width = dialog->findChild<QSpinBox*>(
                        QStringLiteral("BusWidthSpin"));
                    return dialog->objectName()
                               == QStringLiteral("QuickLaneParametersDialog")
                        && width
                        && width->value() == 16;
                })) {
                fail(QStringLiteral("Unchanged Bus parameters could not be confirmed"));
                return;
            }
            const auto unchangedBusMessage = window.statusBar()->currentMessage();
            editedBus = wave::findLane(window.project().scenarios.front(), quickBus.id);
            if (!editedBus
                || editedBus->width != 16
                || !unchangedBusMessage.contains(
                    QStringLiteral("no properties changed"))
                || unchangedBusMessage.contains(QStringLiteral("Ctrl+Z"))) {
                fail(QStringLiteral("Unchanged Bus parameters produced misleading feedback"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            editedBus = wave::findLane(window.project().scenarios.front(), quickBus.id);
            if (!editedBus || editedBus->width != busWidthBeforeEdit) {
                fail(QStringLiteral("Unchanged Bus parameters inserted an empty Undo entry"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();
            editedBus = wave::findLane(window.project().scenarios.front(), quickBus.id);
            if (!editedBus || editedBus->width != 16) {
                fail(QStringLiteral("Unchanged Bus parameters damaged the real Redo entry"));
                return;
            }

            if (!editFromContext(quickBit.id, [](QDialog* dialog) {
                    auto* height = dialog->findChild<QSpinBox*>(
                        QStringLiteral("LaneHeightSpin"));
                    if (dialog->objectName() != QStringLiteral("QuickLaneParametersDialog")
                        || !height) {
                        return false;
                    }
                    height->setValue(64);
                    return true;
                })) {
                fail(QStringLiteral("Bit right-click parameter editor did not open"));
                return;
            }
            renamedBit = wave::findLane(window.project().scenarios.front(), quickBit.id);
            if (!renamedBit
                || renamedBit->height != 64
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("height 64"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Z"))) {
                fail(QStringLiteral("Bit right-click parameters lacked exact result feedback"));
                return;
            }

            bool invalidBitColorRetained = false;
            if (!editFromContext(
                    quickBit.id,
                    [&application, &invalidBitColorRetained](QDialog* dialog) {
                        auto* color = dialog->findChild<QLineEdit*>(
                            QStringLiteral("LaneColorEdit"));
                        if (!color
                            || dialog->objectName()
                                != QStringLiteral("QuickLaneParametersDialog")) {
                            return false;
                        }
                        color->setText(QStringLiteral("not-a-color"));
                        QTimer::singleShot(
                            0,
                            &application,
                            [dialog, &invalidBitColorRetained] {
                                auto* active = QApplication::activeModalWidget();
                                if (auto* warning = qobject_cast<QMessageBox*>(active)) {
                                    warning->accept();
                                    return;
                                }
                                auto* color = dialog->findChild<QLineEdit*>(
                                    QStringLiteral("LaneColorEdit"));
                                auto* height = dialog->findChild<QSpinBox*>(
                                    QStringLiteral("LaneHeightSpin"));
                                auto* error = dialog->findChild<QLabel*>(
                                    QStringLiteral("QuickLaneParameterError"));
                                auto* buttons = dialog->findChild<QDialogButtonBox*>();
                                auto* ok = buttons
                                    ? buttons->button(QDialogButtonBox::Ok)
                                    : nullptr;
                                if (active != dialog
                                    || !color
                                    || !height
                                    || !error
                                    || !ok
                                    || !error->isVisible()
                                    || !error->text().contains(QStringLiteral("valid HTML color"))
                                    || color->text() != QStringLiteral("not-a-color")
                                    || !color->hasFocus()
                                    || height->value() != 64) {
                                    dialog->reject();
                                    return;
                                }
                                invalidBitColorRetained = true;
                                color->setText(QStringLiteral("#123456"));
                                ok->click();
                            });
                        return true;
                    })) {
                fail(QStringLiteral("Invalid Bit color editor did not open"));
                return;
            }
            renamedBit = wave::findLane(window.project().scenarios.front(), quickBit.id);
            if (!invalidBitColorRetained
                || !renamedBit
                || renamedBit->color != "#123456"
                || renamedBit->height != 64
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("color #123456"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Z"))) {
                fail(QStringLiteral("Invalid Bit color was not recoverable in place"));
                return;
            }

            const auto* clockBeforeEdit = wave::findClock(
                window.project(), quickClock.clockDomainId);
            if (!clockBeforeEdit) {
                fail(QStringLiteral("Quick Clock disappeared before parameter editing"));
                return;
            }
            const auto clockPeriodBeforeEdit = clockBeforeEdit->period;
            bool invalidClockRateRetained = false;
            if (!editFromContext(
                    quickClock.id,
                    [&application, &invalidClockRateRetained](QDialog* dialog) {
                        auto* mode = dialog->findChild<QComboBox*>(
                            QStringLiteral("ClockRateMode"));
                        auto* value = dialog->findChild<QLineEdit*>(
                            QStringLiteral("ClockRateValue"));
                        if (dialog->objectName()
                                != QStringLiteral("QuickClockParametersDialog")
                            || !mode
                            || !value) {
                            return false;
                        }
                        mode->setCurrentIndex(mode->findData(QStringLiteral("period")));
                        value->setText(QStringLiteral("not-a-time"));
                        QTimer::singleShot(
                            0,
                            &application,
                            [dialog, &invalidClockRateRetained] {
                                auto* active = QApplication::activeModalWidget();
                                if (auto* warning = qobject_cast<QMessageBox*>(active)) {
                                    warning->accept();
                                    return;
                                }
                                auto* mode = dialog->findChild<QComboBox*>(
                                    QStringLiteral("ClockRateMode"));
                                auto* value = dialog->findChild<QLineEdit*>(
                                    QStringLiteral("ClockRateValue"));
                                auto* error = dialog->findChild<QLabel*>(
                                    QStringLiteral("QuickLaneParameterError"));
                                auto* buttons = dialog->findChild<QDialogButtonBox*>();
                                auto* ok = buttons
                                    ? buttons->button(QDialogButtonBox::Ok)
                                    : nullptr;
                                if (active != dialog
                                    || !mode
                                    || !value
                                    || !error
                                    || !ok
                                    || !error->isVisible()
                                    || error->text().isEmpty()
                                    || value->text() != QStringLiteral("not-a-time")
                                    || !value->hasFocus()
                                    || mode->currentData().toString()
                                        != QStringLiteral("period")) {
                                    dialog->reject();
                                    return;
                                }
                                invalidClockRateRetained = true;
                                value->setText(QStringLiteral("12000 ticks"));
                                ok->click();
                            });
                        return true;
                    })) {
                fail(QStringLiteral("Clock right-click frequency/period editor did not open"));
                return;
            }
            const auto* editedClock = wave::findClock(
                window.project(), quickClock.clockDomainId);
            if (!invalidClockRateRetained
                || !editedClock
                || editedClock->period != 12'000
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("period 12 ns"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Z"))) {
                fail(QStringLiteral("Clock right-click period lacked exact result feedback"));
                return;
            }
            if (!editFromContext(quickClock.id, [](QDialog* dialog) {
                    const auto* mode = dialog->findChild<QComboBox*>(
                        QStringLiteral("ClockRateMode"));
                    const auto* value = dialog->findChild<QLineEdit*>(
                        QStringLiteral("ClockRateValue"));
                    return dialog->objectName()
                               == QStringLiteral("QuickClockParametersDialog")
                        && mode
                        && value;
                })) {
                fail(QStringLiteral("Unchanged Clock parameters could not be confirmed"));
                return;
            }
            const auto unchangedClockMessage = window.statusBar()->currentMessage();
            editedClock = wave::findClock(window.project(), quickClock.clockDomainId);
            if (!editedClock
                || editedClock->period != 12'000
                || !unchangedClockMessage.contains(
                    QStringLiteral("no properties changed"))
                || unchangedClockMessage.contains(QStringLiteral("Ctrl+Z"))) {
                fail(QStringLiteral("Unchanged Clock parameters produced misleading feedback"));
                return;
            }
            if (!canvasAddLaneScreenshotPath.isEmpty()) {
                auto parameterScreenshotPath = canvasAddLaneScreenshotPath;
                const auto suffix = parameterScreenshotPath.lastIndexOf(QLatin1Char('.'));
                if (suffix >= 0) {
                    parameterScreenshotPath.insert(
                        suffix,
                        QStringLiteral("-quick-parameters-no-effect-feedback"));
                } else {
                    parameterScreenshotPath.append(
                        QStringLiteral("-quick-parameters-no-effect-feedback.png"));
                }
                if (!window.grab().save(parameterScreenshotPath)) {
                    fail(QStringLiteral("Cannot save quick-parameter feedback screenshot"));
                    return;
                }
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            editedClock = wave::findClock(window.project(), quickClock.clockDomainId);
            if (!editedClock || editedClock->period != clockPeriodBeforeEdit) {
                fail(QStringLiteral("Unchanged Clock parameters inserted an empty Undo entry"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();
            editedClock = wave::findClock(window.project(), quickClock.clockDomainId);
            if (!editedClock || editedClock->period != 12'000) {
                fail(QStringLiteral("Unchanged Clock parameters damaged the real Redo entry"));
                return;
            }

            const auto laneOrder = [&window] {
                std::vector<std::string> ids;
                for (const auto& lane : window.project().scenarios.front().lanes) {
                    ids.push_back(lane.id);
                }
                return ids;
            };
            const auto orderBeforeDrag = laneOrder();
            const auto clockPosition = std::find(
                orderBeforeDrag.begin(), orderBeforeDrag.end(), quickClock.id);
            const auto busPosition = std::find(
                orderBeforeDrag.begin(), orderBeforeDrag.end(), quickBus.id);
            if (clockPosition == orderBeforeDrag.end()
                || busPosition == orderBeforeDrag.end()) {
                fail(QStringLiteral("Quick lane disappeared before lane drag"));
                return;
            }
            const auto expectedDrop = static_cast<std::size_t>(
                std::distance(orderBeforeDrag.begin(), clockPosition));
            const auto sourcePosition = static_cast<std::size_t>(
                std::distance(orderBeforeDrag.begin(), busPosition));
            canvas->revealLocation(QString::fromStdString(quickBus.id), 0);
            QCoreApplication::processEvents();
            const auto busY = laneCenter(quickBus.id);
            const auto clockTop = laneScreenTop(quickClock.id);
            if (busY < 40 || clockTop < 40) {
                fail(QStringLiteral("Quick lanes are not visible for header drag"));
                return;
            }
            canvas->setTool(wave::WaveCanvas::Tool::Marker);
            sendMouse(
                QEvent::MouseButtonPress,
                QPoint(80, busY),
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseMove,
                QPoint(80, clockTop + 3),
                Qt::NoButton,
                Qt::LeftButton);
            QCoreApplication::processEvents();
            if (!canvas->laneDropDestinationIndex()
                || *canvas->laneDropDestinationIndex() != expectedDrop) {
                sendMouse(
                    QEvent::MouseButtonRelease,
                    QPoint(80, clockTop + 3),
                    Qt::LeftButton,
                    Qt::NoButton);
                fail(QStringLiteral("Header drag did not begin before Escape cancellation"));
                return;
            }
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_Escape);
            QCoreApplication::processEvents();
            sendMouse(
                QEvent::MouseButtonRelease,
                QPoint(80, clockTop + 3),
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            if (laneOrder() != orderBeforeDrag || canvas->laneDropDestinationIndex()) {
                fail(QStringLiteral("Escape did not cancel the pending header drag"));
                return;
            }
            canvas->setTool(wave::WaveCanvas::Tool::WaveEdit);

            sendMouse(
                QEvent::MouseButtonPress,
                QPoint(80, busY),
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseMove,
                QPoint(80, clockTop + 3),
                Qt::NoButton,
                Qt::LeftButton);
            QCoreApplication::processEvents();
            if (!canvas->laneDropDestinationIndex()
                || *canvas->laneDropDestinationIndex() != expectedDrop) {
                sendMouse(
                    QEvent::MouseButtonRelease,
                    QPoint(80, clockTop + 3),
                    Qt::LeftButton,
                    Qt::NoButton);
                fail(QStringLiteral("Header drag did not expose the expected insertion target"));
                return;
            }
            if (!canvasAddLaneScreenshotPath.isEmpty()
                && !window.grab().save(canvasAddLaneScreenshotPath)) {
                sendMouse(
                    QEvent::MouseButtonRelease,
                    QPoint(80, clockTop + 3),
                    Qt::LeftButton,
                    Qt::NoButton);
                fail(QStringLiteral("Cannot save signal-management insertion screenshot"));
                return;
            }
            sendMouse(
                QEvent::MouseButtonRelease,
                QPoint(80, clockTop + 3),
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            const auto orderAfterDrag = laneOrder();
            const auto expectedDragStatus = QStringLiteral(
                "Moved %1: position %2 -> %3. Ctrl+Z to undo.")
                                                .arg(QString::fromStdString(quickBus.name))
                                                .arg(sourcePosition + 1)
                                                .arg(expectedDrop + 1);
            if (orderAfterDrag.at(expectedDrop) != quickBus.id
                || window.statusBar()->currentMessage() != expectedDragStatus) {
                fail(QStringLiteral("Header drag committed the wrong lane order or feedback"));
                return;
            }

            const auto busYAfterDrag = laneCenter(quickBus.id);
            if (busYAfterDrag < 40) {
                fail(QStringLiteral("Reordered Bus is not visible for same-slot drag"));
                return;
            }
            sendMouse(
                QEvent::MouseButtonPress,
                QPoint(80, busYAfterDrag),
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseMove,
                QPoint(105, busYAfterDrag),
                Qt::NoButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                QPoint(105, busYAfterDrag),
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            const auto expectedNoOpStatus = QStringLiteral(
                "%1 remains at position %2. No order changed.")
                                                .arg(QString::fromStdString(quickBus.name))
                                                .arg(expectedDrop + 1);
            if (laneOrder() != orderAfterDrag
                || canvas->laneDropDestinationIndex()
                || window.statusBar()->currentMessage() != expectedNoOpStatus) {
                fail(QStringLiteral("Same-slot header drag changed order or ended silently"));
                return;
            }

            undoAction->trigger();
            QCoreApplication::processEvents();
            if (laneOrder() != orderBeforeDrag
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Undid Move lane"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Y"))) {
                fail(QStringLiteral("Header drag was not one undoable MoveLaneCommand with recovery feedback"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();
            if (laneOrder() != orderAfterDrag
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Redid Move lane"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Z"))) {
                fail(QStringLiteral("Header drag redo or recovery feedback failed"));
                return;
            }

            auto* transactionPanel = canvas->findChild<QWidget*>(
                QStringLiteral("QuickLaneSetupPanel"));
            auto* transactionName = canvas->findChild<QLineEdit*>(
                QStringLiteral("QuickLaneNameEdit"));
            auto* transactionError = canvas->findChild<QLabel*>(
                QStringLiteral("QuickLaneSetupError"));
            auto* transactionBusPalette = window.findChild<QWidget*>(QStringLiteral("BusPresetPalette"));
            auto* transactionBusPreset = window.findChild<QToolButton*>(QStringLiteral("BusPresetXButton"));
            if (!transactionPanel
                || !transactionName
                || !transactionError
                || !transactionBusPalette
                || !transactionBusPreset) {
                fail(QStringLiteral("Quick signal transaction controls are unavailable"));
                return;
            }
            const auto movedBusY = laneCenter(quickBus.id);
            sendMouse(
                QEvent::MouseButtonPress,
                QPoint(360, movedBusY),
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                QPoint(360, movedBusY),
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            if (!transactionBusPalette->isVisible()) {
                fail(QStringLiteral("Could not expose the Bus palette before quick signal setup"));
                return;
            }
            transactionBusPreset->click();
            QCoreApplication::processEvents();
            const auto* transactionBusLane = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!transactionBusLane || transactionBusLane->segments.empty()) {
                fail(QStringLiteral("Could not create a Bus segment for transaction isolation"));
                return;
            }
            const auto transactionBusSegments = transactionBusLane->segments;
            sendMouse(
                QEvent::MouseButtonPress,
                QPoint(360, movedBusY),
                Qt::LeftButton,
                Qt::LeftButton);
            sendMouse(
                QEvent::MouseButtonRelease,
                QPoint(360, movedBusY),
                Qt::LeftButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            if (!transactionBusPalette->isVisible()) {
                fail(QStringLiteral("Could not reopen the Bus palette before quick signal setup"));
                return;
            }
            addButtons.at(1)->click();
            QCoreApplication::processEvents();
            if (!transactionPanel->isVisible() || transactionBusPalette->isVisible()) {
                fail(QStringLiteral("Quick signal setup did not replace the Bus palette"));
                return;
            }
            const auto invalidTransactionLaneId =
                window.project().scenarios.front().lanes.back().id;
            transactionName->clear();
            transactionName->setModified(true);
            transactionBusPreset->click();
            QCoreApplication::processEvents();
            bool doubleClickEditorOpened = false;
            QTimer::singleShot(0, &application, [&doubleClickEditorOpened] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                auto* edit = dialog ? dialog->findChild<QLineEdit*>() : nullptr;
                if (!dialog || !edit) {
                    if (dialog) dialog->reject();
                    return;
                }
                doubleClickEditorOpened = true;
                edit->setText(QStringLiteral("0x1"));
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
            });
            sendMouse(
                QEvent::MouseButtonDblClick,
                QPoint(360, movedBusY),
                Qt::LeftButton,
                Qt::LeftButton);
            QCoreApplication::processEvents();
            const auto* guardedBusLane = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            const auto busSegmentsUnchanged = guardedBusLane
                && guardedBusLane->segments.size() == transactionBusSegments.size()
                && std::equal(
                    guardedBusLane->segments.begin(),
                    guardedBusLane->segments.end(),
                    transactionBusSegments.begin(),
                    [](const wave::Segment& current, const wave::Segment& expected) {
                        return current.id == expected.id
                            && current.start == expected.start
                            && current.end == expected.end
                            && current.value == expected.value;
                    });
            if (doubleClickEditorOpened
                || !transactionPanel->isVisible()
                || !busSegmentsUnchanged) {
                fail(QStringLiteral("Bus controls or double-click bypassed quick signal isolation"));
                return;
            }
            bool unexpectedRemovalDialog = false;
            QTimer::singleShot(0, &application, [&unexpectedRemovalDialog] {
                auto* confirmation = qobject_cast<QMessageBox*>(
                    QApplication::activeModalWidget());
                if (!confirmation) return;
                unexpectedRemovalDialog = true;
                confirmation->reject();
            });
            if (!QMetaObject::invokeMethod(
                    canvas,
                    "removeLaneRequested",
                    Qt::DirectConnection,
                    Q_ARG(QString, QString::fromStdString(quickBit.id)))) {
                fail(QStringLiteral("Could not invoke quick-transaction removal guard"));
                return;
            }
            QCoreApplication::processEvents();
            if (unexpectedRemovalDialog
                || !transactionPanel->isVisible()
                || transactionError->text().isEmpty()
                || !wave::findLane(window.project().scenarios.front(), quickBit.id)
                || !wave::findLane(
                    window.project().scenarios.front(), invalidTransactionLaneId)) {
                fail(QStringLiteral("Invalid quick signal guard state: dialog=%1 panel=%2 error=%3 original=%4 pending=%5")
                         .arg(unexpectedRemovalDialog)
                         .arg(transactionPanel->isVisible())
                         .arg(!transactionError->text().isEmpty())
                         .arg(wave::findLane(window.project().scenarios.front(), quickBit.id) != nullptr)
                         .arg(wave::findLane(window.project().scenarios.front(), invalidTransactionLaneId) != nullptr));
                return;
            }
            sendKey(transactionName, Qt::Key_Escape);
            QCoreApplication::processEvents();
            if (transactionPanel->isVisible()
                || wave::findLane(
                    window.project().scenarios.front(), invalidTransactionLaneId)) {
                fail(QStringLiteral("Escape did not recover the blocked quick signal transaction"));
                return;
            }

            addButtons.at(1)->click();
            QCoreApplication::processEvents();
            if (!transactionPanel->isVisible()) {
                fail(QStringLiteral("Could not begin valid quick signal transaction"));
                return;
            }
            const auto committedTransactionLaneId =
                window.project().scenarios.front().lanes.back().id;
            transactionName->setText(QStringLiteral("transaction_bit"));
            transactionName->setModified(true);
            bool deleteConfirmed = false;
            QTimer::singleShot(0, &application, [&deleteConfirmed] {
                auto* confirmation = qobject_cast<QMessageBox*>(
                    QApplication::activeModalWidget());
                auto* yes = confirmation ? confirmation->button(QMessageBox::Yes) : nullptr;
                if (!confirmation || !yes) {
                    if (confirmation) confirmation->reject();
                    return;
                }
                deleteConfirmed = true;
                yes->click();
            });
            if (!QMetaObject::invokeMethod(
                    canvas,
                    "removeLaneRequested",
                    Qt::DirectConnection,
                    Q_ARG(QString, QString::fromStdString(quickBit.id)))) {
                fail(QStringLiteral("Could not invoke committed quick-transaction removal"));
                return;
            }
            QCoreApplication::processEvents();
            const auto* committedTransactionLane = wave::findLane(
                window.project().scenarios.front(), committedTransactionLaneId);
            if (!deleteConfirmed
                || transactionPanel->isVisible()
                || wave::findLane(window.project().scenarios.front(), quickBit.id)
                || !committedTransactionLane
                || committedTransactionLane->name != "transaction_bit") {
                fail(QStringLiteral("Valid quick signal was not committed before lane removal"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (!wave::findLane(window.project().scenarios.front(), quickBit.id)
                || !wave::findLane(
                    window.project().scenarios.front(), committedTransactionLaneId)) {
                fail(QStringLiteral("Undo did not restore the removal after quick signal commit"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (wave::findLane(
                    window.project().scenarios.front(), committedTransactionLaneId)
                || laneOrder() != orderAfterDrag) {
                fail(QStringLiteral("Second Undo did not remove the committed quick signal"));
                return;
            }

            const auto* busSourcePointer = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!busSourcePointer || busSourcePointer->segments.empty()) {
                fail(QStringLiteral("Bus source has no waveform to duplicate"));
                return;
            }
            const auto busSource = *busSourcePointer;
            const auto orderBeforeDuplicate = laneOrder();
            const auto busSourcePosition = std::find(
                orderBeforeDuplicate.begin(),
                orderBeforeDuplicate.end(),
                quickBus.id);
            if (busSourcePosition == orderBeforeDuplicate.end()) {
                fail(QStringLiteral("Bus source is missing from display order before duplicate"));
                return;
            }
            const auto busSourceIndex = static_cast<std::size_t>(
                std::distance(orderBeforeDuplicate.begin(), busSourcePosition));
            const auto laneCountBeforeDuplicate = orderBeforeDuplicate.size();
            const auto clockCountBeforeBusDuplicate = window.project().clockDomains.size();
            const auto eventCountBeforeDuplicate =
                window.project().scenarios.front().events.size();
            const auto relationCountBeforeDuplicate =
                window.project().scenarios.front().relations.size();

            canvas->revealLocation(
                QString::fromStdString(quickBus.id),
                busSource.segments.front().start);
            QCoreApplication::processEvents();
            const auto duplicateBusY = laneCenter(quickBus.id);
            bool duplicateContextHandled = false;
            QTimer::singleShot(
                0,
                &application,
                [&application, &duplicateContextHandled] {
                    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                    auto* action = menu
                        ? menu->findChild<QAction*>(
                              QStringLiteral("DuplicateLaneContextAction"))
                        : nullptr;
                    if (!menu
                        || menu->objectName()
                            != QStringLiteral("LaneHeaderContextMenu")
                        || !action
                        || action->text() != QStringLiteral("Duplicate signal")) {
                        if (menu) menu->close();
                        return;
                    }
                    duplicateContextHandled = true;
                    action->trigger();
                    menu->close();
                });
            const QPoint duplicateContextPoint(80, duplicateBusY);
            QContextMenuEvent duplicateContextEvent(
                QContextMenuEvent::Mouse,
                duplicateContextPoint,
                canvas->viewport()->mapToGlobal(duplicateContextPoint));
            QCoreApplication::sendEvent(canvas->viewport(), &duplicateContextEvent);
            QCoreApplication::processEvents();

            const auto& lanesAfterBusDuplicate =
                window.project().scenarios.front().lanes;
            if (!duplicateContextHandled
                || lanesAfterBusDuplicate.size() != laneCountBeforeDuplicate + 1
                || busSourceIndex + 1 >= lanesAfterBusDuplicate.size()) {
                fail(QStringLiteral("Signal header context action did not duplicate one adjacent lane"));
                return;
            }
            const auto duplicatedBus = lanesAfterBusDuplicate.at(busSourceIndex + 1);
            const auto duplicateNameCount = std::count_if(
                lanesAfterBusDuplicate.begin(),
                lanesAfterBusDuplicate.end(),
                [&duplicatedBus](const wave::Lane& lane) {
                    return QString::compare(
                               QString::fromStdString(lane.name),
                               QString::fromStdString(duplicatedBus.name),
                               Qt::CaseInsensitive)
                        == 0;
                });
            auto busSegmentsCopied = duplicatedBus.segments.size()
                == busSource.segments.size();
            for (std::size_t index = 0;
                 busSegmentsCopied && index < busSource.segments.size();
                 ++index) {
                const auto& sourceSegment = busSource.segments.at(index);
                const auto& copySegment = duplicatedBus.segments.at(index);
                busSegmentsCopied = copySegment.id != sourceSegment.id
                    && copySegment.start == sourceSegment.start
                    && copySegment.end == sourceSegment.end
                    && copySegment.value == sourceSegment.value
                    && copySegment.extensions == sourceSegment.extensions;
            }
            const auto duplicateStatus = window.statusBar()->currentMessage();
            if (duplicatedBus.id == busSource.id
                || duplicatedBus.name.find(busSource.name + "_copy") != 0
                || duplicateNameCount != 1
                || duplicatedBus.kind != busSource.kind
                || duplicatedBus.width != busSource.width
                || duplicatedBus.isSigned != busSource.isSigned
                || duplicatedBus.radix != busSource.radix
                || duplicatedBus.enumMap != busSource.enumMap
                || duplicatedBus.clockDomainId != busSource.clockDomainId
                || duplicatedBus.groupId != busSource.groupId
                || duplicatedBus.height != busSource.height
                || !duplicatedBus.visible
                || !QColor(QString::fromStdString(duplicatedBus.color)).isValid()
                || QString::compare(
                       QString::fromStdString(duplicatedBus.color),
                       QString::fromStdString(busSource.color),
                       Qt::CaseInsensitive)
                    == 0
                || !busSegmentsCopied
                || window.project().clockDomains.size()
                    != clockCountBeforeBusDuplicate
                || window.project().scenarios.front().events.size()
                    != eventCountBeforeDuplicate
                || window.project().scenarios.front().relations.size()
                    != relationCountBeforeDuplicate
                || canvas->selectedLaneId()
                    != QString::fromStdString(duplicatedBus.id)
                || !duplicateStatus.contains(
                    QString::fromStdString(busSource.name))
                || !duplicateStatus.contains(
                    QString::fromStdString(duplicatedBus.name))
                || !duplicateStatus.contains(QStringLiteral("below the source"))
                || !duplicateStatus.contains(QStringLiteral("Ctrl+Z"))
                || !undoAction->text().contains(QStringLiteral("Duplicate lane"))
                || QApplication::activeModalWidget()) {
                fail(QStringLiteral("Bus duplicate did not preserve waveform/properties with independent identity and feedback"));
                return;
            }

            if (!canvasAddLaneScreenshotPath.isEmpty()) {
                auto duplicateScreenshotPath = canvasAddLaneScreenshotPath;
                const auto suffix = duplicateScreenshotPath.lastIndexOf(QLatin1Char('.'));
                if (suffix >= 0) {
                    duplicateScreenshotPath.insert(
                        suffix,
                        QStringLiteral("-duplicate-signal"));
                } else {
                    duplicateScreenshotPath.append(
                        QStringLiteral("-duplicate-signal.png"));
                }
                if (!window.grab().save(duplicateScreenshotPath)) {
                    fail(QStringLiteral("Cannot save duplicated-signal screenshot"));
                    return;
                }
            }

            undoAction->trigger();
            QCoreApplication::processEvents();
            const auto* busAfterDuplicateUndo = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (wave::findLane(
                    window.project().scenarios.front(), duplicatedBus.id)
                || laneOrder() != orderBeforeDuplicate
                || !busAfterDuplicateUndo
                || *busAfterDuplicateUndo != busSource
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Undid Duplicate lane"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Y"))) {
                fail(QStringLiteral("Bus duplicate was not removed by one Undo"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();
            const auto* busAfterDuplicateRedo = wave::findLane(
                window.project().scenarios.front(), duplicatedBus.id);
            const auto orderAfterDuplicateRedo = laneOrder();
            if (!busAfterDuplicateRedo
                || busSourceIndex + 1 >= orderAfterDuplicateRedo.size()
                || orderAfterDuplicateRedo.at(busSourceIndex + 1)
                    != duplicatedBus.id
                || *busAfterDuplicateRedo != duplicatedBus
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Redid Duplicate lane"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Ctrl+Z"))) {
                fail(QStringLiteral("Bus duplicate Redo did not restore stable identity and position"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();

            canvas->revealLocation(QString::fromStdString(quickBus.id), 0);
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_Right, Qt::ShiftModifier);
            QCoreApplication::processEvents();
            const auto laneCountBeforeRangeGuard =
                window.project().scenarios.front().lanes.size();
            duplicateLaneAction->trigger();
            QCoreApplication::processEvents();
            if (!canvas->hasExplicitRangeSelection()
                || window.project().scenarios.front().lanes.size()
                    != laneCountBeforeRangeGuard
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("Esc clears"))
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("duplicating a whole signal"))) {
                fail(QStringLiteral("Duplicate action discarded an explicit range instead of explaining recovery"));
                return;
            }
            sendKey(canvas, Qt::Key_Escape);
            QCoreApplication::processEvents();

            const auto* clockSourceLanePointer = wave::findLane(
                window.project().scenarios.front(), quickClock.id);
            const auto* clockSourceDomainPointer = clockSourceLanePointer
                ? wave::findClock(
                      window.project(),
                      clockSourceLanePointer->clockDomainId)
                : nullptr;
            if (!clockSourceLanePointer || !clockSourceDomainPointer) {
                fail(QStringLiteral("Clock source is unavailable before Ctrl+D duplicate"));
                return;
            }
            const auto clockSourceLane = *clockSourceLanePointer;
            const auto clockSourceDomain = *clockSourceDomainPointer;
            const auto clockOrderBeforeDuplicate = laneOrder();
            const auto clockSourcePosition = std::find(
                clockOrderBeforeDuplicate.begin(),
                clockOrderBeforeDuplicate.end(),
                quickClock.id);
            if (clockSourcePosition == clockOrderBeforeDuplicate.end()) {
                fail(QStringLiteral("Clock source is missing from display order"));
                return;
            }
            const auto clockSourceIndex = static_cast<std::size_t>(
                std::distance(
                    clockOrderBeforeDuplicate.begin(),
                    clockSourcePosition));
            const auto clockDomainCountBeforeDuplicate =
                window.project().clockDomains.size();
            canvas->revealLocation(QString::fromStdString(quickClock.id), 0);
            QCoreApplication::processEvents();
            clickHeader(QPoint(80, laneCenter(quickClock.id)));
            canvas->setFocus(Qt::OtherFocusReason);
            sendKey(canvas, Qt::Key_D, Qt::ControlModifier);
            QCoreApplication::processEvents();

            const auto clockOrderAfterDuplicate = laneOrder();
            if (clockOrderAfterDuplicate.size()
                    != clockOrderBeforeDuplicate.size() + 1
                || clockSourceIndex + 1 >= clockOrderAfterDuplicate.size()) {
                fail(QStringLiteral("Ctrl+D did not add one adjacent Clock duplicate"));
                return;
            }
            const auto duplicatedClockLaneId =
                clockOrderAfterDuplicate.at(clockSourceIndex + 1);
            const auto* duplicatedClockLane = wave::findLane(
                window.project().scenarios.front(), duplicatedClockLaneId);
            const auto* duplicatedClockDomain = duplicatedClockLane
                ? wave::findClock(window.project(), duplicatedClockLane->clockDomainId)
                : nullptr;
            if (!duplicatedClockLane
                || duplicatedClockLane->id == clockSourceLane.id
                || duplicatedClockLane->clockDomainId
                    == clockSourceLane.clockDomainId
                || duplicatedClockLane->name.find(clockSourceLane.name + "_copy")
                    != 0
                || QString::compare(
                       QString::fromStdString(duplicatedClockLane->color),
                       QString::fromStdString(clockSourceLane.color),
                       Qt::CaseInsensitive)
                    == 0
                || !duplicatedClockDomain
                || duplicatedClockDomain->id == clockSourceDomain.id
                || duplicatedClockDomain->name != duplicatedClockLane->name
                || duplicatedClockDomain->period != clockSourceDomain.period
                || duplicatedClockDomain->phase != clockSourceDomain.phase
                || duplicatedClockDomain->dutyCycle != clockSourceDomain.dutyCycle
                || duplicatedClockDomain->activeEdge != clockSourceDomain.activeEdge
                || duplicatedClockDomain->resetRelation
                    != clockSourceDomain.resetRelation
                || window.project().clockDomains.size()
                    != clockDomainCountBeforeDuplicate + 1
                || !window.statusBar()->currentMessage().contains(
                    QStringLiteral("independent clock settings"))) {
                fail(QStringLiteral("Ctrl+D Clock duplicate did not create an adjacent independent clock"));
                return;
            }
            const auto duplicatedClockDomainId = duplicatedClockDomain->id;
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (wave::findLane(
                    window.project().scenarios.front(), duplicatedClockLaneId)
                || wave::findClock(
                    window.project(), duplicatedClockDomainId)
                || window.project().clockDomains.size()
                    != clockDomainCountBeforeDuplicate
                || !wave::findClock(window.project(), clockSourceDomain.id)
                || *wave::findClock(window.project(), clockSourceDomain.id)
                    != clockSourceDomain) {
                fail(QStringLiteral("Clock duplicate Undo did not remove only the clone and domain"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();
            if (!wave::findLane(
                    window.project().scenarios.front(), duplicatedClockLaneId)
                || window.project().clockDomains.size()
                    != clockDomainCountBeforeDuplicate + 1) {
                fail(QStringLiteral("Clock duplicate Redo did not restore lane and domain"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();

            canvas->revealLocation(QString::fromStdString(quickBit.id), 0);
            QCoreApplication::processEvents();
            bitY = laneCenter(quickBit.id);
            const auto* selectedDeleteLane = wave::findLane(
                window.project().scenarios.front(), quickBit.id);
            if (!selectedDeleteLane) {
                fail(QStringLiteral("Signal for selected-header Delete is missing"));
                return;
            }
            const auto selectedDeleteName = QString::fromStdString(
                selectedDeleteLane->name);
            clickHeader(QPoint(80, bitY));
            const auto selectedHeaderStatus = window.statusBar()->currentMessage();
            if (!selectedHeaderStatus.contains(QStringLiteral("Selected signal"))
                || !selectedHeaderStatus.contains(selectedDeleteName)
                || !selectedHeaderStatus.contains(QStringLiteral("Delete removes signal"))
                || !selectedHeaderStatus.contains(QStringLiteral("F2 renames"))) {
                fail(QStringLiteral("Signal header selection did not identify the target and keys"));
                return;
            }
            bool selectedDeleteConfirmed = false;
            QTimer::singleShot(0, &application, [&selectedDeleteConfirmed] {
                auto* confirmation = qobject_cast<QMessageBox*>(
                    QApplication::activeModalWidget());
                auto* yes = confirmation ? confirmation->button(QMessageBox::Yes) : nullptr;
                if (!confirmation || !yes) {
                    if (confirmation) confirmation->reject();
                    return;
                }
                selectedDeleteConfirmed = true;
                yes->click();
            });
            sendKey(canvas, Qt::Key_Delete);
            QCoreApplication::processEvents();
            if (!selectedDeleteConfirmed
                || wave::findLane(window.project().scenarios.front(), quickBit.id)) {
                fail(QStringLiteral("Selected header Delete did not remove the signal"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (!wave::findLane(window.project().scenarios.front(), quickBit.id)) {
                fail(QStringLiteral("Deleted signal was not restored by Ctrl+Z"));
                return;
            }

            window.hide();
            application.exit(0);
        });
    } else if (cursorModeSmoke) {
        QTimer::singleShot(
            0,
            &window,
            [&application, &window, cursorModeScreenshotPath] {
                auto* canvas = window.findChild<wave::WaveCanvas*>();
                auto* measureAction = window.findChild<QAction*>(
                    QStringLiteral("MeasureToolAction"));
                if (!canvas || !measureAction
                    || measureAction->shortcut().matches(QKeySequence(Qt::CTRL | Qt::Key_M))
                        != QKeySequence::ExactMatch) {
                    qCritical().noquote() << "Measure controls are missing";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto sendMouse = [canvas](
                                           const QEvent::Type type,
                                           const QPoint position,
                                           const Qt::MouseButton button,
                                           const Qt::MouseButtons buttons,
                                           const Qt::KeyboardModifiers modifiers) {
                    const QPointF localPosition(position);
                    const QPointF globalPosition(
                        canvas->viewport()->mapToGlobal(position));
                    QMouseEvent mouseEvent(
                        type,
                        localPosition,
                        globalPosition,
                        button,
                        buttons,
                        modifiers);
                    QCoreApplication::sendEvent(canvas->viewport(), &mouseEvent);
                };
                const auto click = [&sendMouse](
                                       const QPoint position,
                                       const Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
                    sendMouse(
                        QEvent::MouseButtonPress,
                        position,
                        Qt::LeftButton,
                        Qt::LeftButton,
                        modifiers);
                    sendMouse(
                        QEvent::MouseButtonRelease,
                        position,
                        Qt::LeftButton,
                        Qt::NoButton,
                        modifiers);
                };
                const auto drag = [&sendMouse](
                                      const QPoint start,
                                      const QPoint end,
                                      const Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
                    sendMouse(
                        QEvent::MouseButtonPress,
                        start,
                        Qt::LeftButton,
                        Qt::LeftButton,
                        modifiers);
                    sendMouse(
                        QEvent::MouseMove,
                        end,
                        Qt::NoButton,
                        Qt::LeftButton,
                        modifiers);
                    sendMouse(
                        QEvent::MouseButtonRelease,
                        end,
                        Qt::LeftButton,
                        Qt::NoButton,
                        modifiers);
                };
                const auto sendKey = [canvas](const int key) {
                    QKeyEvent keyEvent(QEvent::KeyPress, key, Qt::NoModifier);
                    QCoreApplication::sendEvent(canvas, &keyEvent);
                };

                const auto viewportWidth = canvas->viewport()->width();
                const auto waveformLeft = 210;
                const auto usableWidth = viewportWidth - waveformLeft - 30;
                if (usableWidth < 320) {
                    qCritical().noquote() << "Cursor smoke canvas is too narrow";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto pointAt = [=](const double fraction) {
                    return QPoint(
                        waveformLeft + static_cast<int>(usableWidth * fraction),
                        90);
                };
                const auto point1 = pointAt(0.18);
                const auto point2 = pointAt(0.35);
                const auto point3 = pointAt(0.57);
                const auto point4 = pointAt(0.82);
                const auto formatTime = [&window](const wave::Tick tick) {
                    return QString::fromStdString(wave::formatTick(
                        tick,
                        window.project().timeBase));
                };
                const auto laneCenter = [&window, canvas](const std::string& laneId) {
                    auto y = 40 - canvas->verticalScrollBar()->value();
                    for (const auto& lane : window.project().scenarios.front().lanes) {
                        if (!lane.visible) continue;
                        const auto height = std::clamp(lane.height, 30, 240);
                        if (lane.id == laneId) return y + height / 2;
                        y += height;
                    }
                    return -1;
                };

                canvas->fitScenario();
                canvas->verticalScrollBar()->setValue(0);
                QCoreApplication::processEvents();
                const auto dataY = laneCenter("lane-data");
                auto* busPresetPalette = window.findChild<QWidget*>(QStringLiteral("BusPresetPalette"));
                if (dataY < 0 || !busPresetPalette) {
                    qCritical().noquote() << "Measure isolation controls are missing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                click(QPoint(point2.x(), dataY));
                QCoreApplication::processEvents();
                if (!busPresetPalette->isVisible()) {
                    qCritical().noquote() << "Bus direct controls could not be exposed before Measure";
                    window.hide();
                    application.exit(4);
                    return;
                }
                measureAction->trigger();
                QCoreApplication::processEvents();
                if (!measureAction->isChecked()
                    || canvas->tool() != wave::WaveCanvas::Tool::Marker
                    || busPresetPalette->isVisible()) {
                    qCritical().noquote()
                        << "Measure mode did not activate with direct-edit overlays isolated";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const QPoint busMeasurePoint(point1.x(), dataY);
                click(busMeasurePoint);
                QCoreApplication::processEvents();
                if (!canvas->movableCursorTick() || busPresetPalette->isVisible()) {
                    qCritical().noquote()
                        << "Measuring a Bus reopened its direct-value controls";
                    window.hide();
                    application.exit(4);
                    return;
                }

                bool measureWaveformMenuShown = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&measureWaveformMenuShown] {
                        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                        if (menu
                            && menu->objectName()
                                == QStringLiteral("WaveformContextMenu")) {
                            measureWaveformMenuShown = true;
                            menu->close();
                        }
                    });
                QContextMenuEvent measureContext(
                    QContextMenuEvent::Mouse,
                    busMeasurePoint,
                    canvas->viewport()->mapToGlobal(busMeasurePoint));
                QCoreApplication::sendEvent(canvas->viewport(), &measureContext);
                QCoreApplication::processEvents();
                if (measureWaveformMenuShown
                    || busPresetPalette->isVisible()
                    || canvas->tool() != wave::WaveCanvas::Tool::Marker
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Measure active"))) {
                    qCritical().noquote()
                        << "Measure exposed waveform editing through the context menu";
                    window.hide();
                    application.exit(4);
                    return;
                }

                click(point1);
                const auto clickStatus = window.statusBar()->currentMessage();
                if (!canvas->movableCursorTick()
                    || canvas->temporaryCursorTick()
                    || !clickStatus.contains(QStringLiteral("Cursor"))
                    || !clickStatus.contains(formatTime(*canvas->movableCursorTick()))
                    || !clickStatus.contains(QStringLiteral("values shown beside signals"))
                    || clickStatus.contains(QStringLiteral("Reference"))) {
                    qCritical().noquote()
                        << "Left click did not report the final movable cursor"
                        << clickStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto beforeArrow = *canvas->movableCursorTick();
                sendKey(Qt::Key_Right);
                const auto arrowCursorStatus = window.statusBar()->currentMessage();
                if (!canvas->movableCursorTick()
                    || *canvas->movableCursorTick() <= beforeArrow
                    || !arrowCursorStatus.contains(
                        formatTime(*canvas->movableCursorTick()))
                    || !arrowCursorStatus.contains(
                        QStringLiteral("values shown beside signals"))) {
                    qCritical().noquote()
                        << "Right arrow did not report the final movable cursor"
                        << arrowCursorStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto movableBeforeShift = *canvas->movableCursorTick();
                click(point4, Qt::ShiftModifier);
                const auto shiftStatus = window.statusBar()->currentMessage();
                if (!canvas->temporaryCursorTick()
                    || *canvas->movableCursorTick() != movableBeforeShift
                    || *canvas->temporaryCursorTick() <= *canvas->movableCursorTick()
                    || !shiftStatus.contains(QStringLiteral("Reference"))
                    || !shiftStatus.contains(formatTime(*canvas->temporaryCursorTick()))
                    || !shiftStatus.contains(QStringLiteral("Cursor"))
                    || !shiftStatus.contains(formatTime(*canvas->movableCursorTick()))
                    || !shiftStatus.contains(QString::fromUtf8("Δ -"))) {
                    qCritical().noquote()
                        << "Shift click did not report reference, cursor, and negative delta"
                        << shiftStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }

                drag(point4, point3);
                const auto negativeDragStatus = window.statusBar()->currentMessage();
                if (!canvas->movableCursorTick()
                    || !canvas->temporaryCursorTick()
                    || *canvas->movableCursorTick() >= *canvas->temporaryCursorTick()
                    || !negativeDragStatus.contains(QStringLiteral("Reference"))
                    || !negativeDragStatus.contains(
                        formatTime(*canvas->temporaryCursorTick()))
                    || !negativeDragStatus.contains(QStringLiteral("Cursor"))
                    || !negativeDragStatus.contains(
                        formatTime(*canvas->movableCursorTick()))
                    || !negativeDragStatus.contains(QString::fromUtf8("Δ -"))) {
                    qCritical().noquote()
                        << "Direct drag did not report the final negative measurement"
                        << negativeDragStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }

                drag(point3, point4);
                const auto positiveDragStatus = window.statusBar()->currentMessage();
                if (!canvas->movableCursorTick()
                    || !canvas->temporaryCursorTick()
                    || *canvas->movableCursorTick() <= *canvas->temporaryCursorTick()
                    || !positiveDragStatus.contains(QStringLiteral("Reference"))
                    || !positiveDragStatus.contains(
                        formatTime(*canvas->temporaryCursorTick()))
                    || !positiveDragStatus.contains(QStringLiteral("Cursor"))
                    || !positiveDragStatus.contains(
                        formatTime(*canvas->movableCursorTick()))
                    || !positiveDragStatus.contains(QString::fromUtf8("Δ +"))) {
                    qCritical().noquote()
                        << "Direct drag did not report the final positive measurement"
                        << positiveDragStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto originalMarkerCount =
                    window.project().scenarios.front().markers.size();
                click(point3, Qt::ControlModifier);
                if (window.project().scenarios.front().markers.size()
                        != originalMarkerCount + 1
                    || canvas->selectedMarkerId().isEmpty()) {
                    qCritical().noquote() << "Control click did not create a locked cursor";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto lockedId = canvas->selectedMarkerId().toStdString();
                const auto markerById = [&window, &lockedId]() -> const wave::Marker* {
                    const auto& markers = window.project().scenarios.front().markers;
                    const auto marker = std::find_if(
                        markers.begin(),
                        markers.end(),
                        [&lockedId](const wave::Marker& candidate) {
                            return candidate.id == lockedId;
                        });
                    return marker == markers.end() ? nullptr : &*marker;
                };
                const auto* locked = markerById();
                if (!locked || locked->start != locked->end) {
                    qCritical().noquote() << "Locked cursor is not a point marker";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto lockedName = QString::fromStdString(locked->name);
                const auto createdStatus = window.statusBar()->currentMessage();
                if (!createdStatus.contains(QStringLiteral("Created"))
                    || !createdStatus.contains(lockedName)
                    || !createdStatus.contains(formatTime(locked->start))
                    || !createdStatus.contains(QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Locked cursor creation result or recovery feedback is missing"
                        << createdStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }
                click(QPoint(80, dataY));
                QCoreApplication::processEvents();
                const auto* selectedDataLane = wave::findLane(
                    window.project().scenarios.front(), "lane-data");
                const auto markerRetargetStatus = window.statusBar()->currentMessage();
                if (!canvas->selectedMarkerId().isEmpty()
                    || busPresetPalette->isVisible()
                    || !selectedDataLane
                    || !markerRetargetStatus.contains(QStringLiteral("Selected signal"))
                    || !markerRetargetStatus.contains(
                        QString::fromStdString(selectedDataLane->name))
                    || !markerRetargetStatus.contains(
                        QStringLiteral("Delete removes signal"))
                    || !markerRetargetStatus.contains(QStringLiteral("F2 renames"))
                    || !markerRetargetStatus.contains(
                        QStringLiteral("locked cursor/range deselected"))) {
                    qCritical().noquote()
                        << "Signal header did not replace the locked-cursor target and feedback"
                        << markerRetargetStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }
                click(point3);
                if (canvas->selectedMarkerId().toStdString() != lockedId) {
                    qCritical().noquote()
                        << "Locked cursor could not be reselected after the signal header";
                    window.hide();
                    application.exit(4);
                    return;
                }

                bool laneHeaderMenuShown = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&laneHeaderMenuShown] {
                        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                        if (menu
                            && menu->objectName()
                                == QStringLiteral("LaneHeaderContextMenu")) {
                            laneHeaderMenuShown = true;
                            menu->close();
                        }
                    });
                const QPoint dataHeaderPoint(80, dataY);
                QContextMenuEvent laneHeaderContext(
                    QContextMenuEvent::Mouse,
                    dataHeaderPoint,
                    canvas->viewport()->mapToGlobal(dataHeaderPoint));
                QCoreApplication::sendEvent(canvas->viewport(), &laneHeaderContext);
                QCoreApplication::processEvents();
                const auto contextRetargetStatus = window.statusBar()->currentMessage();
                if (!laneHeaderMenuShown
                    || !canvas->selectedMarkerId().isEmpty()
                    || !contextRetargetStatus.contains(QStringLiteral("Selected signal"))
                    || !contextRetargetStatus.contains(
                        QString::fromStdString(selectedDataLane->name))
                    || !contextRetargetStatus.contains(
                        QStringLiteral("Delete removes signal"))
                    || !contextRetargetStatus.contains(QStringLiteral("F2 renames"))
                    || !contextRetargetStatus.contains(
                        QStringLiteral("locked cursor/range deselected"))) {
                    qCritical().noquote()
                        << "Cancelling the lane context menu did not restore its target feedback"
                        << contextRetargetStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }
                click(point3);
                if (canvas->selectedMarkerId().toStdString() != lockedId) {
                    qCritical().noquote()
                        << "Locked cursor could not be reselected after cancelling the lane menu";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto lockedBeforeDrag = locked->start;
                const auto markerX = [canvas, &window](const wave::Tick tick) {
                    const auto duration =
                        window.project().scenarios.front().duration;
                    return 190 + static_cast<int>(
                        static_cast<double>(tick) / static_cast<double>(duration)
                        * static_cast<double>(canvas->viewport()->width() - 190));
                };
                drag(QPoint(markerX(lockedBeforeDrag), point3.y()), point4);
                locked = markerById();
                const auto dragStatus = window.statusBar()->currentMessage();
                if (!locked
                    || locked->start <= lockedBeforeDrag
                    || !dragStatus.contains(QStringLiteral("Moved"))
                    || !dragStatus.contains(lockedName)
                    || !dragStatus.contains(formatTime(lockedBeforeDrag))
                    || !dragStatus.contains(formatTime(locked->start))
                    || !dragStatus.contains(QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Dragging a selected locked cursor lacked exact result feedback"
                        << dragStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto lockedBeforeArrow = locked->start;
                sendKey(Qt::Key_Left);
                locked = markerById();
                const auto arrowStatus = window.statusBar()->currentMessage();
                if (!locked
                    || locked->start >= lockedBeforeArrow
                    || !arrowStatus.contains(QStringLiteral("Moved"))
                    || !arrowStatus.contains(lockedName)
                    || !arrowStatus.contains(formatTime(lockedBeforeArrow))
                    || !arrowStatus.contains(formatTime(locked->start))
                    || !arrowStatus.contains(QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Locked cursor arrow move lacked exact result feedback"
                        << arrowStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }

                click(QPoint(markerX(locked->start), point3.y()));
                locked = markerById();
                const auto selectionStatus = window.statusBar()->currentMessage();
                if (!locked
                    || canvas->selectedMarkerId().toStdString() != lockedId
                    || !selectionStatus.contains(QStringLiteral("Selected"))
                    || !selectionStatus.contains(lockedName)
                    || !selectionStatus.contains(formatTime(locked->start))
                    || !selectionStatus.contains(QStringLiteral("Delete"))) {
                    qCritical().noquote()
                        << "Locked cursor selection did not explain available editing"
                        << selectionStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto deletedStart = locked->start;
                sendKey(Qt::Key_Delete);
                const auto deleteStatus = window.statusBar()->currentMessage();
                if (window.project().scenarios.front().markers.size()
                        != originalMarkerCount
                    || !deleteStatus.contains(QStringLiteral("Deleted"))
                    || !deleteStatus.contains(lockedName)
                    || !deleteStatus.contains(formatTime(deletedStart))
                    || !deleteStatus.contains(QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Delete did not report the locked cursor result or recovery"
                        << deleteStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || !markerById()
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Undid"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Y"))) {
                    qCritical().noquote()
                        << "Locked cursor deletion Undo or recovery feedback failed"
                        << window.statusBar()->currentMessage();
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
                    || markerById()
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Redid"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Locked cursor deletion Redo or recovery feedback failed"
                        << window.statusBar()->currentMessage();
                    window.hide();
                    application.exit(4);
                    return;
                }

                drag(point1, point2, Qt::ControlModifier);
                const auto& markersAfterRange =
                    window.project().scenarios.front().markers;
                if (markersAfterRange.size() != originalMarkerCount + 1
                    || markersAfterRange.back().start == markersAfterRange.back().end) {
                    qCritical().noquote() << "Control drag did not create a locked range";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto rangeId = markersAfterRange.back().id;
                const auto rangeName = QString::fromStdString(
                    markersAfterRange.back().name);
                const auto rangeStart = markersAfterRange.back().start;
                const auto rangeEnd = markersAfterRange.back().end;
                const auto rangeStatus = window.statusBar()->currentMessage();
                if (!rangeStatus.contains(QStringLiteral("Created"))
                    || !rangeStatus.contains(rangeName)
                    || !rangeStatus.contains(formatTime(rangeStart))
                    || !rangeStatus.contains(formatTime(rangeEnd))
                    || !rangeStatus.contains(QString::fromUtf8("Δ"))
                    || !rangeStatus.contains(QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Locked range creation result or width feedback is missing"
                        << rangeStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto rangeById = [&window, &rangeId]() -> const wave::Marker* {
                    const auto& markers = window.project().scenarios.front().markers;
                    const auto marker = std::find_if(
                        markers.begin(),
                        markers.end(),
                        [&rangeId](const wave::Marker& candidate) {
                            return candidate.id == rangeId;
                        });
                    return marker == markers.end() ? nullptr : &*marker;
                };
                auto* range = rangeById();
                auto boundaryMoves = 0;
                while (range && range->start > 0 && boundaryMoves < 100) {
                    sendKey(Qt::Key_Left);
                    range = rangeById();
                    ++boundaryMoves;
                }
                if (!range || range->start != 0) {
                    qCritical().noquote() << "Locked range did not reach the left boundary";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_Left);
                range = rangeById();
                const auto boundaryStatus = window.statusBar()->currentMessage();
                if (!range
                    || range->start != 0
                    || !boundaryStatus.contains(rangeName)
                    || !boundaryStatus.contains(QStringLiteral("timeline boundary reached"))
                    || !boundaryStatus.contains(QStringLiteral("no position changed"))) {
                    qCritical().noquote()
                        << "Locked range boundary move ended silently or changed the model"
                        << boundaryStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || !(range = rangeById())
                    || range->start <= 0
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Y"))) {
                    qCritical().noquote()
                        << "Boundary no-op inserted history or blocked the last real marker Undo"
                        << window.statusBar()->currentMessage();
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
                    || !(range = rangeById())
                    || range->start != 0
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Locked range boundary move Redo failed"
                        << window.statusBar()->currentMessage();
                    window.hide();
                    application.exit(4);
                    return;
                }

                click(point3, Qt::ControlModifier);
                const auto firstRecreatedId = canvas->selectedMarkerId().toStdString();
                const auto firstRecreated = std::find_if(
                    window.project().scenarios.front().markers.begin(),
                    window.project().scenarios.front().markers.end(),
                    [&firstRecreatedId](const wave::Marker& marker) {
                        return marker.id == firstRecreatedId;
                    });
                if (firstRecreatedId.empty()
                    || firstRecreated
                        == window.project().scenarios.front().markers.end()) {
                    qCritical().noquote() << "First marker for name-reuse smoke is missing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto firstRecreatedMarker = *firstRecreated;

                click(point4, Qt::ControlModifier);
                const auto survivingId = canvas->selectedMarkerId().toStdString();
                const auto surviving = std::find_if(
                    window.project().scenarios.front().markers.begin(),
                    window.project().scenarios.front().markers.end(),
                    [&survivingId](const wave::Marker& marker) {
                        return marker.id == survivingId;
                    });
                if (survivingId.empty()
                    || surviving == window.project().scenarios.front().markers.end()
                    || QString::compare(
                           QString::fromStdString(firstRecreatedMarker.name),
                           QString::fromStdString(surviving->name),
                           Qt::CaseInsensitive)
                        == 0) {
                    qCritical().noquote() << "Consecutive locked cursors were not uniquely named";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto survivingMarker = *surviving;

                click(QPoint(
                    markerX(firstRecreatedMarker.start),
                    point3.y()));
                if (canvas->selectedMarkerId().toStdString() != firstRecreatedId) {
                    qCritical().noquote() << "Cannot select the earlier marker for name-reuse smoke";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_Delete);
                const auto deletedEarlier = std::none_of(
                    window.project().scenarios.front().markers.begin(),
                    window.project().scenarios.front().markers.end(),
                    [&firstRecreatedId](const wave::Marker& marker) {
                        return marker.id == firstRecreatedId;
                    });
                if (!deletedEarlier) {
                    qCritical().noquote() << "Earlier marker was not deleted before name reuse";
                    window.hide();
                    application.exit(4);
                    return;
                }

                click(point2, Qt::ControlModifier);
                const auto replacementId = canvas->selectedMarkerId().toStdString();
                const auto replacement = std::find_if(
                    window.project().scenarios.front().markers.begin(),
                    window.project().scenarios.front().markers.end(),
                    [&replacementId](const wave::Marker& marker) {
                        return marker.id == replacementId;
                    });
                const auto replacementStatus = window.statusBar()->currentMessage();
                if (replacementId.empty()
                    || replacement == window.project().scenarios.front().markers.end()
                    || QString::compare(
                           QString::fromStdString(replacement->name),
                           QString::fromStdString(survivingMarker.name),
                           Qt::CaseInsensitive)
                        == 0
                    || !replacementStatus.contains(QStringLiteral("Created"))
                    || !replacementStatus.contains(
                        QString::fromStdString(replacement->name))
                    || !replacementStatus.contains(QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Locked cursor name was reused after deleting an earlier marker"
                        << replacementStatus;
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto& uniqueMarkers =
                    window.project().scenarios.front().markers;
                for (auto left = uniqueMarkers.begin(); left != uniqueMarkers.end(); ++left) {
                    for (auto right = std::next(left); right != uniqueMarkers.end(); ++right) {
                        if (QString::compare(
                                QString::fromStdString(left->name),
                                QString::fromStdString(right->name),
                                Qt::CaseInsensitive)
                            == 0) {
                            qCritical().noquote()
                                << "Marker names are not unique after deletion and recreation";
                            window.hide();
                            application.exit(4);
                            return;
                        }
                    }
                }

                drag(point4, point3);
                QCoreApplication::processEvents();
                if (!cursorModeScreenshotPath.isEmpty()
                    && !window.grab().save(cursorModeScreenshotPath)) {
                    qCritical().noquote() << "Cannot save cursor mode screenshot";
                    window.hide();
                    application.exit(3);
                    return;
                }

                sendKey(Qt::Key_Escape);
                QCoreApplication::processEvents();
                if (measureAction->isChecked()
                    || canvas->tool() != wave::WaveCanvas::Tool::WaveEdit
                    || canvas->movableCursorTick()
                    || canvas->temporaryCursorTick()
                    || !canvas->selectedMarkerId().isEmpty()) {
                    qCritical().noquote() << "Escape did not return Measure to direct editing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                measureAction->trigger();
                measureAction->trigger();
                if (measureAction->isChecked()
                    || canvas->tool() != wave::WaveCanvas::Tool::WaveEdit) {
                    qCritical().noquote() << "Second Measure click did not return to direct editing";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto interruptedPanIsolated = [&](const bool exitWithEscape) {
                    canvas->fitScenario();
                    canvas->zoomIn();
                    canvas->zoomIn();
                    canvas->zoomIn();
                    const auto maximum = canvas->horizontalScrollBar()->maximum();
                    if (maximum <= 0) return false;
                    canvas->horizontalScrollBar()->setValue(maximum / 2);
                    measureAction->trigger();
                    QCoreApplication::processEvents();
                    if (!measureAction->isChecked()
                        || canvas->tool() != wave::WaveCanvas::Tool::Marker) {
                        return false;
                    }
                    sendMouse(
                        QEvent::MouseButtonPress,
                        point2,
                        Qt::MiddleButton,
                        Qt::MiddleButton,
                        Qt::NoModifier);
                    if (exitWithEscape) {
                        sendKey(Qt::Key_Escape);
                    } else {
                        measureAction->trigger();
                    }
                    QCoreApplication::processEvents();
                    const auto positionAfterExit = canvas->horizontalScrollBar()->value();
                    sendMouse(
                        QEvent::MouseMove,
                        point4,
                        Qt::NoButton,
                        Qt::MiddleButton,
                        Qt::NoModifier);
                    sendMouse(
                        QEvent::MouseButtonRelease,
                        point4,
                        Qt::MiddleButton,
                        Qt::NoButton,
                        Qt::NoModifier);
                    QCoreApplication::processEvents();
                    return !measureAction->isChecked()
                        && canvas->tool() == wave::WaveCanvas::Tool::WaveEdit
                        && canvas->horizontalScrollBar()->value() == positionAfterExit;
                };
                if (!interruptedPanIsolated(false)) {
                    qCritical().noquote()
                        << "Measure toggle left an interrupted pan active in direct editing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!interruptedPanIsolated(true)) {
                    qCritical().noquote()
                        << "Measure Escape left an interrupted pan active in direct editing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                canvas->fitScenario();
                window.hide();
                application.exit(0);
            });
    } else if (waveEditSmoke) {
        QTimer::singleShot(
            0,
            &window,
            [&application, &window, waveEditScreenshotPath] {
                auto* canvas = window.findChild<wave::WaveCanvas*>();
                auto* measureAction = window.findChild<QAction*>(
                    QStringLiteral("MeasureToolAction"));
                if (!canvas
                    || !measureAction
                    || measureAction->isChecked()
                    || window.findChild<QAction*>(QStringLiteral("WaveEditToolAction"))
                    || canvas->tool() != wave::WaveCanvas::Tool::WaveEdit) {
                    qCritical().noquote() << "Default direct waveform editing is unavailable";
                    window.hide();
                    application.exit(4);
                    return;
                }

                canvas->fitScenario();

                const auto settleLayouts = [] {
                    for (auto pass = 0; pass < 3; ++pass) {
                        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
                        QCoreApplication::processEvents();
                    }
                };
                settleLayouts();
                auto* waveformToolbar = window.findChild<QToolBar*>(
                    QStringLiteral("WaveformToolbar"));
                auto* rangeToolbarAction = window.findChild<QAction*>(
                    QStringLiteral("RangeEditToolbarAction"));
                if (!waveformToolbar
                    || !rangeToolbarAction
                    || rangeToolbarAction->isVisible()
                    || waveformToolbar->isMovable()
                    || waveformToolbar->isFloatable()
                    || waveformToolbar->toggleViewAction()->isVisible()
                    || waveformToolbar->toggleViewAction()->isEnabled()) {
                    qCritical().noquote()
                        << "Fixed waveform toolbar baseline is not available";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto stableToolbarHeight = waveformToolbar->height();
                const auto stableViewportTop =
                    canvas->viewport()->mapToGlobal(QPoint{}).y();
                const auto stableVerticalLayout = [&] {
                    return waveformToolbar->height() == stableToolbarHeight
                        && canvas->viewport()->mapToGlobal(QPoint{}).y()
                            == stableViewportTop;
                };
                const auto widgetGlobalRect = [](const QWidget* widget) {
                    return QRect(widget->mapToGlobal(QPoint{}), widget->size());
                };
                const auto sendMouse = [canvas](
                                           const QEvent::Type type,
                                           const QPoint position,
                                           const Qt::MouseButton button,
                                           const Qt::MouseButtons buttons,
                                           const Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
                    const QPointF localPosition(position);
                    const QPointF globalPosition(
                        canvas->viewport()->mapToGlobal(position));
                    QMouseEvent mouseEvent(
                        type,
                        localPosition,
                        globalPosition,
                        button,
                        buttons,
                        modifiers);
                    QCoreApplication::sendEvent(canvas->viewport(), &mouseEvent);
                };
                const auto click = [&sendMouse](const QPoint position) {
                    sendMouse(
                        QEvent::MouseButtonPress,
                        position,
                        Qt::LeftButton,
                        Qt::LeftButton);
                    sendMouse(
                        QEvent::MouseButtonRelease,
                        position,
                        Qt::LeftButton,
                        Qt::NoButton);
                };
                const auto clickWidget = [&window](QWidget* widget) {
                    if (!widget) return false;
                    const QPoint localPosition = widget->rect().center();
                    const QPoint globalPosition = widget->mapToGlobal(localPosition);
                    auto* hit = window.childAt(window.mapFromGlobal(globalPosition));
                    if (hit != widget && (!hit || !widget->isAncestorOf(hit))) return false;
                    const QPointF localPoint(localPosition);
                    const QPointF globalPoint(globalPosition);
                    QMouseEvent press(
                        QEvent::MouseButtonPress,
                        localPoint,
                        globalPoint,
                        Qt::LeftButton,
                        Qt::LeftButton,
                        Qt::NoModifier);
                    QCoreApplication::sendEvent(widget, &press);
                    QMouseEvent release(
                        QEvent::MouseButtonRelease,
                        localPoint,
                        globalPoint,
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
                    QCoreApplication::sendEvent(widget, &release);
                    QCoreApplication::processEvents();
                    return true;
                };
                const auto drag = [&sendMouse](const QPoint start, const QPoint end) {
                    sendMouse(
                        QEvent::MouseButtonPress,
                        start,
                        Qt::LeftButton,
                        Qt::LeftButton);
                    sendMouse(
                        QEvent::MouseMove,
                        end,
                        Qt::NoButton,
                        Qt::LeftButton);
                    sendMouse(
                        QEvent::MouseButtonRelease,
                        end,
                        Qt::LeftButton,
                        Qt::NoButton);
                };
                const auto dragModified = [&sendMouse](
                                              const QPoint start,
                                              const QPoint end,
                                              const Qt::KeyboardModifiers modifiers) {
                    sendMouse(
                        QEvent::MouseButtonPress,
                        start,
                        Qt::LeftButton,
                        Qt::LeftButton,
                        modifiers);
                    sendMouse(
                        QEvent::MouseMove,
                        end,
                        Qt::NoButton,
                        Qt::LeftButton,
                        modifiers);
                    sendMouse(
                        QEvent::MouseButtonRelease,
                        end,
                        Qt::LeftButton,
                        Qt::NoButton,
                        modifiers);
                };
                const auto sendKey = [canvas](const int key) {
                    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
                    QCoreApplication::sendEvent(canvas, &press);
                    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
                    QCoreApplication::sendEvent(canvas, &release);
                    QCoreApplication::processEvents();
                };

                auto& scenario = window.project().scenarios.front();
                const auto xAtTick = [canvas, &scenario](const wave::Tick tick) {
                    return 190 + static_cast<int>(
                        static_cast<double>(tick)
                        / static_cast<double>(scenario.duration)
                        * static_cast<double>(canvas->viewport()->width() - 190));
                };
                const auto laneCenterY = [&scenario](const std::string& laneId) {
                    auto y = 40;
                    for (const auto& lane : scenario.lanes) {
                        if (!lane.visible) continue;
                        const auto height = std::clamp(lane.height, 30, 240);
                        if (lane.id == laneId) return y + height / 2;
                        y += height;
                    }
                    return -1;
                };
                const auto valueAt = [](const wave::Lane& lane, const wave::Tick tick) {
                    const auto iterator = std::find_if(
                        lane.segments.begin(),
                        lane.segments.end(),
                        [tick](const wave::Segment& segment) {
                            return segment.start <= tick && tick < segment.end;
                        });
                    return iterator == lane.segments.end()
                        ? std::string{}
                        : iterator->value;
                };
                const auto tickAtPoint = [canvas, &sendMouse](const QPoint& point) {
                    sendMouse(
                        QEvent::MouseMove,
                        point,
                        Qt::NoButton,
                        Qt::NoButton);
                    QCoreApplication::processEvents();
                    return canvas->cursorTick();
                };
                const auto chooseWaveformAction =
                    [&application, canvas, &settleLayouts](
                        const QPoint& position,
                        const QString& actionText) {
                    bool handled = false;
                    QTimer::singleShot(
                        0,
                        &application,
                        [&application, &handled, actionText] {
                            auto* menu = qobject_cast<QMenu*>(
                                QApplication::activePopupWidget());
                            QAction* target = nullptr;
                            if (menu
                                && menu->objectName()
                                    == QStringLiteral("WaveformContextMenu")) {
                                for (auto* action : menu->actions()) {
                                    if (action && action->text() == actionText) {
                                        target = action;
                                        break;
                                    }
                                }
                            }
                            if (!menu || !target) {
                                if (menu) menu->close();
                                return;
                            }
                            menu->setActiveAction(target);
                            handled = true;
                            QKeyEvent enter(
                                QEvent::KeyPress,
                                Qt::Key_Return,
                                Qt::NoModifier);
                            QCoreApplication::sendEvent(menu, &enter);
                        });
                    QContextMenuEvent context(
                        QContextMenuEvent::Mouse,
                        position,
                        canvas->viewport()->mapToGlobal(position));
                    QCoreApplication::sendEvent(canvas->viewport(), &context);
                    settleLayouts();
                    return handled;
                };

                const auto clockY = laneCenterY("lane-clk");
                const auto dataY = laneCenterY("lane-data");
                const auto resetY = laneCenterY("lane-reset");
                const auto requestY = laneCenterY("lane-request");
                const auto acknowledgeY = laneCenterY("lane-ack");
                if (clockY < 0 || dataY < 0 || resetY < 0
                    || requestY < 0 || acknowledgeY < 0) {
                    qCritical().noquote() << "Wave Edit smoke lanes are missing";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto beforeClockRunWorkflow = scenario;
                const QPoint clockContextPoint(xAtTick(65'000), clockY);
                if (!chooseWaveformAction(
                        clockContextPoint,
                        QStringLiteral("Gate for one period"))) {
                    qCritical().noquote()
                        << "Clock context menu does not expose one-period Gate";
                    window.hide();
                    application.exit(4);
                    return;
                }
                auto* clockLane = wave::findLane(scenario, "lane-clk");
                if (!clockLane
                    || valueAt(*clockLane, 65'000) != "gated"
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("= gated"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Clock one-period Gate did not report its exact result";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto afterClockGate = scenario;

                if (!chooseWaveformAction(
                        clockContextPoint,
                        QStringLiteral("Run for one period"))) {
                    qCritical().noquote()
                        << "Clock context menu does not expose one-period Run";
                    window.hide();
                    application.exit(4);
                    return;
                }
                clockLane = wave::findLane(scenario, "lane-clk");
                const auto clockRunRange = canvas->selectedTimeRange();
                if (!clockLane
                    || !valueAt(*clockLane, 65'000).empty()
                    || !clockRunRange
                    || *clockRunRange
                        != std::pair<wave::Tick, wave::Tick>{60'000, 70'000}
                    || !canvas->selectedSegmentId().isEmpty()
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("restored normal clock waveform"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Clock Run did not clear, select, and report one period";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto afterClockRun = scenario;

                if (!chooseWaveformAction(
                        clockContextPoint,
                        QStringLiteral("Run for one period"))) {
                    qCritical().noquote()
                        << "Clock no-effect Run action was not invoked";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto repeatedClockRunMessage =
                    window.statusBar()->currentMessage();
                if (scenario != afterClockRun
                    || canvas->selectedTimeRange()
                        != std::optional<std::pair<wave::Tick, wave::Tick>>{
                            std::pair<wave::Tick, wave::Tick>{60'000, 70'000}}
                    || !canvas->selectedSegmentId().isEmpty()
                    || !repeatedClockRunMessage.contains(
                        QStringLiteral("already uses normal clock waveform"))
                    || !repeatedClockRunMessage.contains(
                        QStringLiteral("no values changed"))
                    || repeatedClockRunMessage.contains(QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Repeated Clock Run created a false edit or misleading feedback";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto clockRunScreenshotPath = waveEditScreenshotPath;
                    const auto suffix =
                        clockRunScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        clockRunScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-clock-run-no-effect-feedback"));
                    } else {
                        clockRunScreenshotPath.append(
                            QStringLiteral("-clock-run-no-effect-feedback.png"));
                    }
                    if (!window.grab().save(clockRunScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save Clock Run feedback screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != afterClockGate) {
                    qCritical().noquote()
                        << "Repeated Clock Run inserted an empty Undo entry";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
                    || scenario != afterClockRun) {
                    qCritical().noquote()
                        << "Repeated Clock Run damaged the real Redo entry";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != afterClockGate
                    || !QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeClockRunWorkflow) {
                    qCritical().noquote()
                        << "Clock Run workflow did not restore its initial Scenario";
                    window.hide();
                    application.exit(4);
                    return;
                }

                click(QPoint(xAtTick(110'000), dataY));
                if (canvas->selectedSegmentId()
                    != QStringLiteral("segment-data-payload")) {
                    qCritical().noquote() << "Single click did not select the bus segment";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto resizedStartTick = tickAtPoint(
                    QPoint(xAtTick(90'000), dataY));
                drag(
                    QPoint(xAtTick(80'000), dataY),
                    QPoint(xAtTick(90'000), dataY));
                auto* dataLane = wave::findLane(scenario, "lane-data");
                auto payload = dataLane
                    ? std::find_if(
                          dataLane->segments.begin(),
                          dataLane->segments.end(),
                          [](const wave::Segment& segment) {
                              return segment.id == "segment-data-payload";
                          })
                    : std::vector<wave::Segment>::iterator{};
                if (!dataLane || payload == dataLane->segments.end()
                    || payload->start != resizedStartTick) {
                    qCritical().noquote() << "Dragging the left boundary did not resize the segment";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto resizedEndTick = tickAtPoint(
                    QPoint(xAtTick(160'000), dataY));
                drag(
                    QPoint(xAtTick(150'000), dataY),
                    QPoint(xAtTick(160'000), dataY));
                dataLane = wave::findLane(scenario, "lane-data");
                payload = std::find_if(
                    dataLane->segments.begin(),
                    dataLane->segments.end(),
                    [](const wave::Segment& segment) {
                        return segment.id == "segment-data-payload";
                    });
                if (payload == dataLane->segments.end() || payload->end != resizedEndTick) {
                    qCritical().noquote() << "Dragging the right boundary did not resize the segment";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto moveStartBefore = payload->start;
                const auto moveWidth = payload->end - payload->start;
                const auto moveGrabTick = payload->start + moveWidth / 2;
                drag(
                    QPoint(xAtTick(moveGrabTick), dataY),
                    QPoint(xAtTick(moveGrabTick + 10'000), dataY));
                dataLane = wave::findLane(scenario, "lane-data");
                payload = std::find_if(
                    dataLane->segments.begin(),
                    dataLane->segments.end(),
                    [](const wave::Segment& segment) {
                        return segment.id == "segment-data-payload";
                    });
                if (payload == dataLane->segments.end()
                    || payload->start <= moveStartBefore
                    || payload->end - payload->start != moveWidth
                    || canvas->selectedSegmentId()
                        != QStringLiteral("segment-data-payload")) {
                    qCritical().noquote() << "Dragging the segment body did not move one Segment";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto payloadEditTick = payload->start + moveWidth / 2;

                window.activateWindow();
                canvas->setFocus(Qt::OtherFocusReason);
                QCoreApplication::processEvents();
                sendMouse(
                    QEvent::MouseButtonDblClick,
                    QPoint(xAtTick(payloadEditTick), dataY),
                    Qt::LeftButton,
                    Qt::LeftButton);
                sendMouse(
                    QEvent::MouseButtonRelease,
                    QPoint(xAtTick(payloadEditTick), dataY),
                    Qt::LeftButton,
                    Qt::NoButton);
                QCoreApplication::processEvents();
                auto* segmentEditor = window.findChild<QLineEdit*>(
                    QStringLiteral("BusPresetValueEdit"));
                auto* segmentContext = window.findChild<QLabel*>(
                    QStringLiteral("BusPresetContextLabel"));
                auto* segmentPalette = window.findChild<QWidget*>(
                    QStringLiteral("BusPresetPalette"));
                if (!segmentEditor
                    || !segmentContext
                    || !segmentPalette
                    || !segmentPalette->isVisible()
                    || !segmentEditor->hasFocus()
                    || segmentEditor->text() != QStringLiteral("0x35")
                    || !segmentContext->text().contains(QStringLiteral("Segment"))) {
                    qCritical().noquote()
                        << "Double click editor diagnostics"
                        << "editor" << static_cast<bool>(segmentEditor)
                        << "context" << static_cast<bool>(segmentContext)
                        << "palette" << static_cast<bool>(segmentPalette)
                        << "visible" << (segmentPalette && segmentPalette->isVisible())
                        << "focus" << (segmentEditor && segmentEditor->hasFocus())
                        << "text" << (segmentEditor ? segmentEditor->text() : QStringLiteral("<missing>"))
                        << "contextText" << (segmentContext ? segmentContext->text() : QStringLiteral("<missing>"))
                        << "focusWidget" << (QApplication::focusWidget() ? QApplication::focusWidget()->objectName() : QStringLiteral("<none>"))
                        << "editorEnabled" << (segmentEditor && segmentEditor->isEnabled())
                        << "selected" << canvas->selectedSegmentId()
                        << "tick" << payloadEditTick;
                    window.hide();
                    application.exit(4);
                    return;
                }
                segmentEditor->setText(QStringLiteral("0x2a"));
                segmentEditor->setModified(true);
                QKeyEvent segmentEnter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(segmentEditor, &segmentEnter);
                QCoreApplication::processEvents();
                dataLane = wave::findLane(scenario, "lane-data");
                if (segmentPalette->isVisible()
                    || !dataLane
                    || valueAt(*dataLane, payloadEditTick) != "0x2a"
                    || canvas->selectedSegmentId()
                        != QStringLiteral("segment-data-payload")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("data[7:0]"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("= 0x2a"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Double click did not edit the existing Segment inline";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto payloadEvent = std::find_if(
                    scenario.events.begin(),
                    scenario.events.end(),
                    [](const wave::Event& event) {
                        return event.linkedSegmentId == "segment-data-payload";
                    });
                const auto* existingRelation = wave::findRelation(
                    scenario,
                    "relation-req-ack");
                if (payloadEvent == scenario.events.end() || !existingRelation) {
                    qCritical().noquote()
                        << "Segment clear relation feedback fixture is missing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                auto clearFeedbackRelation = *existingRelation;
                clearFeedbackRelation.id = "relation-segment-clear-feedback";
                clearFeedbackRelation.sourceEventId = payloadEvent->id;
                clearFeedbackRelation.description =
                    "temporary Segment clear feedback relation";
                auto& segmentClearScenario =
                    const_cast<wave::Scenario&>(scenario);
                segmentClearScenario.relations.push_back(clearFeedbackRelation);
                const auto beforeSelectedSegmentClear = scenario;

                canvas->setFocus(Qt::OtherFocusReason);
                sendKey(Qt::Key_Delete);
                dataLane = wave::findLane(scenario, "lane-data");
                const auto segmentClearMessage = window.statusBar()->currentMessage();
                if (!dataLane
                    || !valueAt(*dataLane, payloadEditTick).empty()
                    || !canvas->selectedSegmentId().isEmpty()
                    || wave::findRelation(
                        scenario,
                        "relation-segment-clear-feedback")
                    || !segmentClearMessage.contains(QStringLiteral("data[7:0]"))
                    || !segmentClearMessage.contains(QStringLiteral("implicit X"))
                    || !segmentClearMessage.contains(
                        QStringLiteral("removed 1 relation"))
                    || !segmentClearMessage.contains(
                        QStringLiteral("Ctrl+Z restores waveform and relations"))) {
                    qCritical().noquote() << "Delete did not clear the selected Segment";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto segmentClearScreenshotPath = waveEditScreenshotPath;
                    const auto suffix =
                        segmentClearScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        segmentClearScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-segment-clear-feedback"));
                    } else {
                        segmentClearScreenshotPath.append(
                            QStringLiteral("-segment-clear-feedback.png"));
                    }
                    if (!window.grab().save(segmentClearScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save Segment clear feedback screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                const auto afterSelectedSegmentClear = scenario;
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)) {
                    qCritical().noquote() << "Cannot undo selected Segment deletion";
                    window.hide();
                    application.exit(4);
                    return;
                }
                dataLane = wave::findLane(scenario, "lane-data");
                if (!dataLane
                    || valueAt(*dataLane, payloadEditTick) != "0x2a"
                    || scenario != beforeSelectedSegmentClear
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Undid Clear lane range"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Y"))) {
                    qCritical().noquote() << "Selected Segment deletion undo failed";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
                    || scenario != afterSelectedSegmentClear
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Redid Clear lane range"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote() << "Selected Segment deletion redo failed";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeSelectedSegmentClear) {
                    qCritical().noquote()
                        << "Selected Segment deletion final recovery failed";
                    window.hide();
                    application.exit(4);
                    return;
                }
                std::erase_if(
                    segmentClearScenario.relations,
                    [](const wave::Relation& relation) {
                        return relation.id == "relation-segment-clear-feedback";
                    });

                const auto beforePulse = scenario;
                bool pulseMenuHandled = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&application, &pulseMenuHandled] {
                        auto* menu = qobject_cast<QMenu*>(
                            QApplication::activePopupWidget());
                        QAction* pulse = nullptr;
                        if (menu
                            && menu->objectName()
                                == QStringLiteral("WaveformContextMenu")) {
                            for (auto* action : menu->actions()) {
                                if (action
                                    && action->text()
                                        == QStringLiteral("Insert one-beat pulse")) {
                                    pulse = action;
                                    break;
                                }
                            }
                        }
                        if (!menu || !pulse) {
                            qCritical().noquote()
                                << "Bit context menu does not expose one-beat Pulse";
                            if (menu) menu->close();
                            application.exit(4);
                            return;
                        }
                        menu->setActiveAction(pulse);
                        pulseMenuHandled = true;
                        QKeyEvent enter(
                            QEvent::KeyPress,
                            Qt::Key_Return,
                            Qt::NoModifier);
                        QCoreApplication::sendEvent(menu, &enter);
                    });
                const QPoint pulseContextPoint(xAtTick(60'000), requestY);
                QContextMenuEvent pulseContext(
                    QContextMenuEvent::Mouse,
                    pulseContextPoint,
                    canvas->viewport()->mapToGlobal(pulseContextPoint));
                QCoreApplication::sendEvent(canvas->viewport(), &pulseContext);
                settleLayouts();
                auto* pulseLane = wave::findLane(scenario, "lane-request");
                const auto pulseRange = canvas->selectedTimeRange();
                if (!pulseMenuHandled
                    || !pulseLane
                    || valueAt(*pulseLane, 55'000) != "0"
                    || valueAt(*pulseLane, 65'000) != "1"
                    || valueAt(*pulseLane, 75'000) != "0"
                    || pulseRange
                        != std::optional<std::pair<wave::Tick, wave::Tick>>{
                            std::pair<wave::Tick, wave::Tick>{60'000, 70'000}}
                    || !canvas->selectedSegmentId().isEmpty()
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("req"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("pulse = 1"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "One-beat Pulse did not report or select its exact result"
                        << "handled=" << pulseMenuHandled
                        << "v55=" << (pulseLane ? QString::fromStdString(valueAt(*pulseLane, 55'000)) : QStringLiteral("<missing>"))
                        << "v65=" << (pulseLane ? QString::fromStdString(valueAt(*pulseLane, 65'000)) : QStringLiteral("<missing>"))
                        << "v75=" << (pulseLane ? QString::fromStdString(valueAt(*pulseLane, 75'000)) : QStringLiteral("<missing>"))
                        << "range=" << (pulseRange ? QStringLiteral("%1-%2").arg(pulseRange->first).arg(pulseRange->second) : QStringLiteral("<none>"))
                        << "segment=" << canvas->selectedSegmentId()
                        << "status=" << window.statusBar()->currentMessage();
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto pulseScreenshotPath = waveEditScreenshotPath;
                    const auto suffix =
                        pulseScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        pulseScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-pulse-feedback"));
                    } else {
                        pulseScreenshotPath.append(
                            QStringLiteral("-pulse-feedback.png"));
                    }
                    if (!window.grab().save(pulseScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save one-beat Pulse feedback screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                const auto afterPulse = scenario;
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforePulse) {
                    qCritical().noquote() << "One-beat Pulse undo was not exact";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
                    || scenario != afterPulse) {
                    qCritical().noquote() << "One-beat Pulse redo was not exact";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforePulse) {
                    qCritical().noquote() << "One-beat Pulse final recovery failed";
                    window.hide();
                    application.exit(4);
                    return;
                }

                sendMouse(
                    QEvent::MouseMove,
                    QPoint(xAtTick(65'000), requestY),
                    Qt::NoButton,
                    Qt::NoButton);
                QCoreApplication::processEvents();
                const auto hoveredBeat = canvas->hoveredBitBeatRange();
                if (canvas->hoveredBitBeatLaneId() != QStringLiteral("lane-request")
                    || !hoveredBeat
                    || *hoveredBeat != std::pair<wave::Tick, wave::Tick>{60'000, 70'000}) {
                    qCritical().noquote() << "Hover did not identify exactly one bit beat";
                    window.hide();
                    application.exit(4);
                    return;
                }

                click(QPoint(xAtTick(65'000), requestY));
                auto* requestLane = wave::findLane(scenario, "lane-request");
                const auto firstClickRange = canvas->selectedTimeRange();
                if (!requestLane
                    || valueAt(*requestLane, 65'000) != "1"
                    || valueAt(*requestLane, 55'000) != "0"
                    || !canvas->selectedSegmentId().isEmpty()
                    || !firstClickRange
                    || *firstClickRange
                        != std::pair<wave::Tick, wave::Tick>{60'000, 70'000}) {
                    qCritical().noquote() << "Single click did not toggle exactly one bit beat";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto toggleDependencyEvent = std::find_if(
                    scenario.events.begin(),
                    scenario.events.end(),
                    [](const wave::Event& event) {
                        return event.waveformLinked
                            && event.laneId == "lane-request"
                            && event.tick == 60'000;
                    });
                const auto* toggleRelationTemplate = wave::findRelation(
                    scenario,
                    "relation-req-ack");
                if (toggleDependencyEvent == scenario.events.end()
                    || !toggleRelationTemplate) {
                    qCritical().noquote()
                        << "One-beat toggle dependency fixture is missing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                auto toggleDependencyRelation = *toggleRelationTemplate;
                toggleDependencyRelation.id = "relation-bit-toggle-feedback";
                toggleDependencyRelation.sourceEventId = toggleDependencyEvent->id;
                toggleDependencyRelation.description =
                    "temporary one-beat toggle feedback relation";
                segmentClearScenario.relations.push_back(toggleDependencyRelation);
                const auto beforeDependencyToggle = scenario;

                click(QPoint(xAtTick(65'000), requestY));
                requestLane = wave::findLane(scenario, "lane-request");
                const auto secondClickRange = canvas->selectedTimeRange();
                const auto dependencyToggleMessage =
                    window.statusBar()->currentMessage();
                if (!requestLane
                    || valueAt(*requestLane, 55'000) != "0"
                    || valueAt(*requestLane, 65'000) != "0"
                    || valueAt(*requestLane, 75'000) != "0"
                    || !canvas->selectedSegmentId().isEmpty()
                    || !secondClickRange
                    || *secondClickRange
                        != std::pair<wave::Tick, wave::Tick>{60'000, 70'000}
                    || wave::findRelation(
                        scenario,
                        "relation-bit-toggle-feedback")
                    || !dependencyToggleMessage.contains(
                        QStringLiteral("removed 1 relation"))
                    || !dependencyToggleMessage.contains(
                        QStringLiteral("Ctrl+Z restores waveform and relations"))) {
                    qCritical().noquote()
                        << "Repeated click did not report its dependent Relation cleanup";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto afterDependencyToggle = scenario;
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeDependencyToggle
                    || !wave::findRelation(
                        scenario,
                        "relation-bit-toggle-feedback")) {
                    qCritical().noquote()
                        << "One-beat toggle did not restore its dependent Relation";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
                    || scenario != afterDependencyToggle
                    || wave::findRelation(
                        scenario,
                        "relation-bit-toggle-feedback")) {
                    qCritical().noquote()
                        << "One-beat toggle did not reproduce dependency cleanup";
                    window.hide();
                    application.exit(4);
                    return;
                }

                canvas->setFocus(Qt::OtherFocusReason);
                sendKey(Qt::Key_X);
                requestLane = wave::findLane(scenario, "lane-request");
                if (!requestLane || valueAt(*requestLane, 65'000) != "X") {
                    qCritical().noquote() << "X key did not set the selected Bit beat to X";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_Z);
                requestLane = wave::findLane(scenario, "lane-request");
                if (!requestLane || valueAt(*requestLane, 65'000) != "Z") {
                    qCritical().noquote() << "Z key did not set the selected Bit beat to Z";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_0);
                requestLane = wave::findLane(scenario, "lane-request");
                const auto bitBeatRange =
                    std::pair<wave::Tick, wave::Tick>{60'000, 70'000};
                const auto hasBeatScopedSelection = [&canvas, &bitBeatRange] {
                    const auto range = canvas->selectedTimeRange();
                    return range
                        && *range == bitBeatRange
                        && canvas->selectedSegmentId().isEmpty();
                };
                if (!requestLane
                    || valueAt(*requestLane, 65'000) != "0"
                    || !hasBeatScopedSelection()) {
                    qCritical().noquote()
                        << "0 key did not keep the selected Bit beat scoped to one beat";
                    window.hide();
                    application.exit(4);
                    return;
                }

                bool bitSetMenuHandled = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&application, &bitSetMenuHandled] {
                        auto* menu = qobject_cast<QMenu*>(
                            QApplication::activePopupWidget());
                        QAction* setX = nullptr;
                        if (menu
                            && menu->objectName()
                                == QStringLiteral("WaveformContextMenu")) {
                            for (auto* action : menu->actions()) {
                                if (action
                                    && action->text()
                                        == QStringLiteral("Set beat to X")) {
                                    setX = action;
                                    break;
                                }
                            }
                        }
                        if (!menu || !setX) {
                            qCritical().noquote()
                                << "Bit context menu does not expose one-beat X";
                            if (menu) menu->close();
                            application.exit(4);
                            return;
                        }
                        menu->setActiveAction(setX);
                        bitSetMenuHandled = true;
                        QKeyEvent enter(
                            QEvent::KeyPress,
                            Qt::Key_Return,
                            Qt::NoModifier);
                        QCoreApplication::sendEvent(menu, &enter);
                    });
                const QPoint bitContextPoint(xAtTick(65'000), requestY);
                QContextMenuEvent bitSetContext(
                    QContextMenuEvent::Mouse,
                    bitContextPoint,
                    canvas->viewport()->mapToGlobal(bitContextPoint));
                QCoreApplication::sendEvent(canvas->viewport(), &bitSetContext);
                settleLayouts();
                requestLane = wave::findLane(scenario, "lane-request");
                if (!bitSetMenuHandled
                    || !requestLane
                    || valueAt(*requestLane, 55'000) != "0"
                    || valueAt(*requestLane, 65'000) != "X"
                    || valueAt(*requestLane, 75'000) != "0"
                    || !hasBeatScopedSelection()) {
                    qCritical().noquote()
                        << "Bit context value expanded one beat to a merged Segment";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto beforeRepeatedBitWrite = scenario;
                canvas->setFocus(Qt::OtherFocusReason);
                sendKey(Qt::Key_X);
                requestLane = wave::findLane(scenario, "lane-request");
                const auto repeatedBitWriteMessage =
                    window.statusBar()->currentMessage();
                if (!requestLane
                    || scenario != beforeRepeatedBitWrite
                    || valueAt(*requestLane, 65'000) != "X"
                    || !hasBeatScopedSelection()
                    || !repeatedBitWriteMessage.contains(QStringLiteral("req"))
                    || !repeatedBitWriteMessage.contains(QStringLiteral("already = X"))
                    || !repeatedBitWriteMessage.contains(
                        QStringLiteral("no values changed"))
                    || repeatedBitWriteMessage.contains(QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Repeated Bit write created a false edit or misleading feedback";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto noEffectScreenshotPath = waveEditScreenshotPath;
                    const auto suffix =
                        noEffectScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        noEffectScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-no-effect-write-feedback"));
                    } else {
                        noEffectScreenshotPath.append(
                            QStringLiteral("-no-effect-write-feedback.png"));
                    }
                    if (!window.grab().save(noEffectScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save no-effect write feedback screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)) {
                    qCritical().noquote()
                        << "No-effect write removed the preceding real Undo entry";
                    window.hide();
                    application.exit(4);
                    return;
                }
                requestLane = wave::findLane(scenario, "lane-request");
                if (!requestLane
                    || valueAt(*requestLane, 65'000) != "0"
                    || !hasBeatScopedSelection()) {
                    qCritical().noquote()
                        << "One Undo stopped on a no-effect write";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
                    || scenario != beforeRepeatedBitWrite
                    || !hasBeatScopedSelection()) {
                    qCritical().noquote()
                        << "No-effect write damaged the preceding Redo entry";
                    window.hide();
                    application.exit(4);
                    return;
                }

                if (!waveEditScreenshotPath.isEmpty()) {
                    auto bitBeatScreenshotPath = waveEditScreenshotPath;
                    const auto suffix = bitBeatScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        bitBeatScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-bit-beat-selection"));
                    } else {
                        bitBeatScreenshotPath.append(
                            QStringLiteral("-bit-beat-selection.png"));
                    }
                    if (!window.grab().save(bitBeatScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save Bit beat selection screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }

                QEvent bitSelectionLeave(QEvent::Leave);
                QCoreApplication::sendEvent(canvas->viewport(), &bitSelectionLeave);
                QCoreApplication::processEvents();
                if (!canvas->hoveredBitBeatLaneId().isEmpty()
                    || canvas->hoveredBitBeatRange()
                    || !hasBeatScopedSelection()) {
                    qCritical().noquote()
                        << "Bit beat selection did not survive pointer leave independently of hover";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto persistentBeatScreenshotPath = waveEditScreenshotPath;
                    const auto suffix =
                        persistentBeatScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        persistentBeatScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-bit-beat-persistent-selection"));
                    } else {
                        persistentBeatScreenshotPath.append(
                            QStringLiteral("-bit-beat-persistent-selection.png"));
                    }
                    if (!window.grab().save(persistentBeatScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save persistent Bit beat selection screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }

                const auto clearDependencyEvent = std::find_if(
                    scenario.events.begin(),
                    scenario.events.end(),
                    [](const wave::Event& event) {
                        return event.waveformLinked
                            && event.laneId == "lane-request"
                            && event.tick == 60'000;
                    });
                const auto* clearRelationTemplate = wave::findRelation(
                    scenario,
                    "relation-req-ack");
                if (clearDependencyEvent == scenario.events.end()
                    || !clearRelationTemplate) {
                    qCritical().noquote()
                        << "One-beat Clear dependency fixture is missing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                auto clearDependencyRelation = *clearRelationTemplate;
                clearDependencyRelation.id = "relation-bit-clear-feedback";
                clearDependencyRelation.sourceEventId = clearDependencyEvent->id;
                clearDependencyRelation.description =
                    "temporary one-beat Clear feedback relation";
                segmentClearScenario.relations.push_back(clearDependencyRelation);
                const auto beforeBitBeatClear = scenario;
                bool bitClearMenuHandled = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&application, &bitClearMenuHandled] {
                        auto* menu = qobject_cast<QMenu*>(
                            QApplication::activePopupWidget());
                        QAction* clearBeat = nullptr;
                        if (menu
                            && menu->objectName()
                                == QStringLiteral("WaveformContextMenu")) {
                            for (auto* action : menu->actions()) {
                                if (action
                                    && action->text()
                                        == QStringLiteral("Clear beat to implicit 0")) {
                                    clearBeat = action;
                                    break;
                                }
                            }
                        }
                        if (!menu || !clearBeat) {
                            qCritical().noquote()
                                << "Bit context menu does not expose one-beat Clear";
                            if (menu) menu->close();
                            application.exit(4);
                            return;
                        }
                        menu->setActiveAction(clearBeat);
                        bitClearMenuHandled = true;
                        QKeyEvent enter(
                            QEvent::KeyPress,
                            Qt::Key_Return,
                            Qt::NoModifier);
                        QCoreApplication::sendEvent(menu, &enter);
                    });
                QContextMenuEvent bitClearContext(
                    QContextMenuEvent::Mouse,
                    bitContextPoint,
                    canvas->viewport()->mapToGlobal(bitContextPoint));
                QCoreApplication::sendEvent(canvas->viewport(), &bitClearContext);
                settleLayouts();
                requestLane = wave::findLane(scenario, "lane-request");
                if (!bitClearMenuHandled
                    || !requestLane
                    || valueAt(*requestLane, 55'000) != "0"
                    || !valueAt(*requestLane, 65'000).empty()
                    || valueAt(*requestLane, 75'000) != "0"
                    || !hasBeatScopedSelection()
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("implicit 0"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("removed 1 relation"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z restores waveform and relations"))
                    || wave::findRelation(
                        scenario,
                        "relation-bit-clear-feedback")) {
                    qCritical().noquote()
                        << "Bit context Clear did not report its dependent Relation cleanup";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto dependencyFeedbackScreenshotPath = waveEditScreenshotPath;
                    const auto suffix =
                        dependencyFeedbackScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        dependencyFeedbackScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-bit-dependency-feedback"));
                    } else {
                        dependencyFeedbackScreenshotPath.append(
                            QStringLiteral("-bit-dependency-feedback.png"));
                    }
                    if (!window.grab().save(dependencyFeedbackScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save Bit dependency feedback screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                const auto afterBitBeatClear = scenario;
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeBitBeatClear
                    || !hasBeatScopedSelection()) {
                    qCritical().noquote()
                        << "One-beat Bit Clear undo did not preserve selection";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
                    || scenario != afterBitBeatClear
                    || !hasBeatScopedSelection()) {
                    qCritical().noquote() << "One-beat Bit Clear redo failed";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_X);
                sendKey(Qt::Key_Delete);
                requestLane = wave::findLane(scenario, "lane-request");
                if (!requestLane
                    || !valueAt(*requestLane, 65'000).empty()
                    || !hasBeatScopedSelection()
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Delete did not clear exactly the selected Bit beat";
                    window.hide();
                    application.exit(4);
                    return;
                }

                dragModified(
                    QPoint(xAtTick(20'000), requestY),
                    QPoint(xAtTick(40'000), acknowledgeY),
                    Qt::ShiftModifier);
                settleLayouts();
                const auto multiLaneRange = canvas->selectedTimeRange();
                const auto multiLaneIds = canvas->selectedLaneIds();
                if (!multiLaneRange
                    || multiLaneRange->second <= multiLaneRange->first
                    || multiLaneIds.size() != 2
                    || !multiLaneIds.contains(QStringLiteral("lane-request"))
                    || !multiLaneIds.contains(QStringLiteral("lane-ack"))) {
                    qCritical().noquote() << "Shift-drag did not select a multi-lane time range";
                    window.hide();
                    application.exit(4);
                    return;
                }
                auto* rangePalette = window.findChild<QFrame*>(
                    QStringLiteral("RangeEditPalette"));
                auto* rangeContext = window.findChild<QLabel*>(
                    QStringLiteral("RangeEditContextLabel"));
                auto* rangeCopyButton = window.findChild<QToolButton*>(
                    QStringLiteral("RangeEditCopyButton"));
                auto* rangeCutButton = window.findChild<QToolButton*>(
                    QStringLiteral("RangeEditCutButton"));
                auto* rangePasteButton = window.findChild<QToolButton*>(
                    QStringLiteral("RangeEditPasteButton"));
                auto* rangeClearButton = window.findChild<QToolButton*>(
                    QStringLiteral("RangeEditClearButton"));
                auto* rangeOneButton = window.findChild<QToolButton*>(
                    QStringLiteral("RangeEditOneButton"));
                auto* rangeXButton = window.findChild<QToolButton*>(
                    QStringLiteral("RangeEditXButton"));

                if (!canvas->hasExplicitRangeSelection()
                    || !rangePalette
                    || !rangePalette->isVisibleTo(&window)
                    || !rangeContext
                    || !rangeContext->text().contains(QStringLiteral("Bit"))
                    || !rangeCopyButton
                    || !rangeCopyButton->isVisibleTo(&window)
                    || !rangeCopyButton->isEnabled()
                    || !rangeCutButton
                    || !rangeCutButton->isVisibleTo(&window)
                    || !rangeCutButton->isEnabled()
                    || !rangePasteButton
                    || !rangePasteButton->isVisibleTo(&window)
                    || !rangeClearButton
                    || !rangeClearButton->isVisibleTo(&window)
                    || !rangeClearButton->isEnabled()
                    || !rangeOneButton
                    || !rangeOneButton->isEnabled()
                    || !rangeXButton
                    || !rangeXButton->isEnabled()
                    || !waveformToolbar
                    || !rangeToolbarAction
                    || !rangeToolbarAction->isVisible()
                    || waveformToolbar->widgetForAction(rangeToolbarAction) != rangePalette
                    || !waveformToolbar->isAncestorOf(rangePalette)
                    || !stableVerticalLayout()
                    || waveformToolbar->actions().indexOf(measureAction)
                        >= waveformToolbar->actions().indexOf(rangeToolbarAction)
                    || widgetGlobalRect(rangePalette).intersects(
                        widgetGlobalRect(canvas->viewport()))) {
                    qCritical().noquote()
                        << "Released Bit range is not actionable in the fixed toolbar"
                        << "paletteVisible" << (rangePalette && rangePalette->isVisibleTo(&window))
                        << "actionVisible" << (rangeToolbarAction && rangeToolbarAction->isVisible())
                        << "hosted" << (waveformToolbar && rangeToolbarAction
                            && waveformToolbar->widgetForAction(rangeToolbarAction) == rangePalette)
                        << "ancestor" << (waveformToolbar && rangePalette
                            && waveformToolbar->isAncestorOf(rangePalette))
                        << "measureIndex" << (waveformToolbar
                            ? waveformToolbar->actions().indexOf(measureAction) : -1)
                        << "rangeIndex" << (waveformToolbar && rangeToolbarAction
                            ? waveformToolbar->actions().indexOf(rangeToolbarAction) : -1)
                        << "intersects" << (rangePalette
                            && widgetGlobalRect(rangePalette).intersects(
                                widgetGlobalRect(canvas->viewport())));
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto rangePaletteRectBeforeViewChange = widgetGlobalRect(rangePalette);
                canvas->zoomIn();
                settleLayouts();
                if (!rangePalette->isVisibleTo(&window)
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != multiLaneRange
                    || canvas->horizontalScrollBar()->maximum() <= 0
                    || !stableVerticalLayout()
                    || widgetGlobalRect(rangePalette) != rangePaletteRectBeforeViewChange
                    || widgetGlobalRect(rangePalette).intersects(
                        widgetGlobalRect(canvas->viewport()))) {
                    qCritical().noquote()
                        << "Range toolbar moved, hid or lost selection during zoom";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto horizontalMaximum = canvas->horizontalScrollBar()->maximum();
                canvas->horizontalScrollBar()->setValue(horizontalMaximum);
                settleLayouts();
                if (canvas->horizontalScrollBar()->value() != horizontalMaximum
                    || !rangePalette->isVisibleTo(&window)
                    || canvas->selectedTimeRange() != multiLaneRange
                    || !stableVerticalLayout()
                    || widgetGlobalRect(rangePalette) != rangePaletteRectBeforeViewChange
                    || widgetGlobalRect(rangePalette).intersects(
                        widgetGlobalRect(canvas->viewport()))) {
                    qCritical().noquote()
                        << "Range toolbar moved or lost selection during waveform scrolling";
                    window.hide();
                    application.exit(4);
                    return;
                }
                canvas->fitScenario();
                settleLayouts();
                if (!rangePalette->isVisibleTo(&window)
                    || canvas->selectedTimeRange() != multiLaneRange
                    || !stableVerticalLayout()
                    || widgetGlobalRect(rangePalette) != rangePaletteRectBeforeViewChange
                    || widgetGlobalRect(rangePalette).intersects(
                        widgetGlobalRect(canvas->viewport()))) {
                    qCritical().noquote()
                        << "Range toolbar moved or lost selection after Fit scenario";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto beforeRangeBoundaryAdjustment = scenario;
                drag(
                    QPoint(xAtTick(multiLaneRange->first), requestY),
                    QPoint(xAtTick(30'000), requestY));
                settleLayouts();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange()
                        != std::optional<std::pair<wave::Tick, wave::Tick>>{
                            std::pair<wave::Tick, wave::Tick>{30'000, 40'000}}
                    || canvas->selectedLaneIds() != multiLaneIds
                    || !rangePalette->isVisibleTo(&window)
                    || scenario != beforeRangeBoundaryAdjustment
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Adjusted range"))) {
                    qCritical().noquote()
                        << "Dragging the left range handle did not preserve context and model";
                    window.hide();
                    application.exit(4);
                    return;
                }
                drag(
                    QPoint(xAtTick(40'000), acknowledgeY),
                    QPoint(xAtTick(50'000), acknowledgeY));
                settleLayouts();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange()
                        != std::optional<std::pair<wave::Tick, wave::Tick>>{
                            std::pair<wave::Tick, wave::Tick>{30'000, 50'000}}
                    || canvas->selectedLaneIds() != multiLaneIds
                    || !rangePalette->isVisibleTo(&window)
                    || scenario != beforeRangeBoundaryAdjustment
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Adjusted range"))) {
                    qCritical().noquote()
                        << "Dragging the right range handle did not preserve context and model";
                    window.hide();
                    application.exit(4);
                    return;
                }
                dragModified(
                    QPoint(xAtTick(20'000), requestY),
                    QPoint(xAtTick(40'000), acknowledgeY),
                    Qt::ShiftModifier);
                settleLayouts();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != multiLaneRange
                    || canvas->selectedLaneIds() != multiLaneIds
                    || scenario != beforeRangeBoundaryAdjustment
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("width 20 ns"))) {
                    qCritical().noquote()
                        << "Re-selecting the original range did not expose its exact width";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto beforeSafeRangeRetarget = scenario;
                click(QPoint(48, acknowledgeY));
                settleLayouts();
                if (canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange()
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-ack")}
                    || rangePalette->isVisibleTo(&window)
                    || rangeToolbarAction->isVisible()
                    || scenario != beforeSafeRangeRetarget
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("range cleared"),
                        Qt::CaseInsensitive)) {
                    qCritical().noquote()
                        << "One lane-header click did not dismiss the range and select the lane";
                    window.hide();
                    application.exit(4);
                    return;
                }
                dragModified(
                    QPoint(xAtTick(20'000), requestY),
                    QPoint(xAtTick(40'000), acknowledgeY),
                    Qt::ShiftModifier);
                settleLayouts();
                click(QPoint(xAtTick(60'000), 20));
                settleLayouts();
                if (canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange()
                    || !canvas->selectedLaneIds().isEmpty()
                    || canvas->cursorTick() != 60'000
                    || rangePalette->isVisibleTo(&window)
                    || rangeToolbarAction->isVisible()
                    || scenario != beforeSafeRangeRetarget
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("range cleared"),
                        Qt::CaseInsensitive)) {
                    qCritical().noquote()
                        << "One ruler click did not dismiss the range and move the edit cursor";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto retargetScreenshotPath = waveEditScreenshotPath;
                    const auto suffix = retargetScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        retargetScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-range-ruler-retarget"));
                    } else {
                        retargetScreenshotPath.append(
                            QStringLiteral("-range-ruler-retarget.png"));
                    }
                    if (!window.grab().save(retargetScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save range ruler retarget screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                dragModified(
                    QPoint(xAtTick(20'000), requestY),
                    QPoint(xAtTick(40'000), acknowledgeY),
                    Qt::ShiftModifier);
                settleLayouts();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != multiLaneRange
                    || canvas->selectedLaneIds() != multiLaneIds
                    || scenario != beforeSafeRangeRetarget) {
                    qCritical().noquote()
                        << "Re-selecting the range after safe retarget checks failed";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto rangeScreenshotPath = waveEditScreenshotPath;
                    const auto suffix = rangeScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        rangeScreenshotPath.insert(suffix, QStringLiteral("-range-selection"));
                    } else {
                        rangeScreenshotPath.append(QStringLiteral("-range-selection.png"));
                    }
                    if (!window.grab().save(rangeScreenshotPath)) {
                        qCritical().noquote() << "Cannot save persistent range screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }

                const auto beforeBitRangeAssignment = scenario;
                const auto* beforeRequestLane = wave::findLane(
                    beforeBitRangeAssignment, "lane-request");
                const auto* beforeAcknowledgeLane = wave::findLane(
                    beforeBitRangeAssignment, "lane-ack");
                sendKey(Qt::Key_1);
                requestLane = wave::findLane(scenario, "lane-request");
                auto* acknowledgeLane = wave::findLane(scenario, "lane-ack");
                if (!beforeRequestLane
                    || !beforeAcknowledgeLane
                    || !requestLane
                    || !acknowledgeLane
                    || valueAt(*requestLane, multiLaneRange->first + 1) != "1"
                    || valueAt(*requestLane, multiLaneRange->second - 1) != "1"
                    || valueAt(*acknowledgeLane, multiLaneRange->first + 1) != "1"
                    || valueAt(*acknowledgeLane, multiLaneRange->second - 1) != "1"
                    || valueAt(*requestLane, multiLaneRange->first - 1)
                        != valueAt(*beforeRequestLane, multiLaneRange->first - 1)
                    || valueAt(*requestLane, multiLaneRange->second)
                        != valueAt(*beforeRequestLane, multiLaneRange->second)
                    || valueAt(*acknowledgeLane, multiLaneRange->first - 1)
                        != valueAt(*beforeAcknowledgeLane, multiLaneRange->first - 1)
                    || valueAt(*acknowledgeLane, multiLaneRange->second)
                        != valueAt(*beforeAcknowledgeLane, multiLaneRange->second)
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != multiLaneRange
                    || canvas->selectedLaneIds() != multiLaneIds) {
                    qCritical().noquote() << "1 key did not assign the complete Bit selection";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto afterBitRangeAssignment = scenario;
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeBitRangeAssignment) {
                    qCritical().noquote() << "Bit range assignment was not one atomic undo";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
                    || scenario != afterBitRangeAssignment) {
                    qCritical().noquote() << "Bit range assignment atomic redo failed";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeBitRangeAssignment
                    || !canvas->hasExplicitRangeSelection()) {
                    qCritical().noquote() << "Bit range selection was not retained through undo";
                    window.hide();
                    application.exit(4);
                    return;
                }
                rangeXButton->click();
                QCoreApplication::processEvents();
                requestLane = wave::findLane(scenario, "lane-request");
                acknowledgeLane = wave::findLane(scenario, "lane-ack");
                if (!requestLane
                    || !acknowledgeLane
                    || valueAt(*requestLane, multiLaneRange->first + 1) != "X"
                    || valueAt(*requestLane, multiLaneRange->second - 1) != "X"
                    || valueAt(*acknowledgeLane, multiLaneRange->first + 1) != "X"
                    || valueAt(*acknowledgeLane, multiLaneRange->second - 1) != "X") {
                    qCritical().noquote() << "Bit range palette did not assign both complete ranges";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto beforeRepeatedRangeWrite = scenario;
                rangeXButton->click();
                QCoreApplication::processEvents();
                const auto repeatedRangeWriteMessage =
                    window.statusBar()->currentMessage();
                if (scenario != beforeRepeatedRangeWrite
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != multiLaneRange
                    || canvas->selectedLaneIds() != multiLaneIds
                    || !repeatedRangeWriteMessage.contains(
                        QStringLiteral("already = X"))
                    || !repeatedRangeWriteMessage.contains(
                        QStringLiteral("no values changed"))
                    || repeatedRangeWriteMessage.contains(QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Repeated range write created a false edit or misleading feedback";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeBitRangeAssignment) {
                    qCritical().noquote() << "Bit range palette assignment was not atomic";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto* relationFixture = wave::findRelation(
                    scenario,
                    "relation-req-ack");
                if (!relationFixture) {
                    qCritical().noquote() << "Range relation fixture is missing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto relationSourceId = relationFixture->sourceEventId;
                const auto relationTargetId = relationFixture->targetEventId;
                dragModified(
                    QPoint(xAtTick(110'000), requestY),
                    QPoint(xAtTick(120'000), acknowledgeY),
                    Qt::ShiftModifier);
                rangeXButton->click();
                QCoreApplication::processEvents();
                relationFixture = wave::findRelation(scenario, "relation-req-ack");
                if (!relationFixture
                    || relationFixture->sourceEventId != relationSourceId
                    || relationFixture->targetEventId != relationTargetId
                    || !wave::findEvent(scenario, relationFixture->sourceEventId)
                    || !wave::findEvent(scenario, relationFixture->targetEventId)) {
                    qCritical().noquote()
                        << "Range edit did not preserve a relation whose edges remain";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeBitRangeAssignment) {
                    qCritical().noquote()
                        << "Relation-preserving range edit undo was not exact";
                    window.hide();
                    application.exit(4);
                    return;
                }

                dragModified(
                    QPoint(xAtTick(80'000), requestY),
                    QPoint(xAtTick(150'000), acknowledgeY),
                    Qt::ShiftModifier);
                rangeXButton->click();
                QCoreApplication::processEvents();
                const auto relationCleanupMessage = window.statusBar()->currentMessage();
                if (wave::findRelation(scenario, "relation-req-ack")
                    || !relationCleanupMessage.contains(
                        QStringLiteral("removed 1 relation"),
                        Qt::CaseInsensitive)
                    || !relationCleanupMessage.contains(
                        QStringLiteral("Ctrl+Z restores waveform and relations"),
                        Qt::CaseInsensitive)) {
                    qCritical().noquote()
                        << "Removed waveform edge did not clean and report its relation";
                    window.hide();
                    application.exit(4);
                    return;
                }
                for (const auto& relation : scenario.relations) {
                    if (!wave::findEvent(scenario, relation.sourceEventId)
                        || (!relation.targetEventId.empty()
                            && !wave::findEvent(scenario, relation.targetEventId))) {
                        qCritical().noquote()
                            << "Range edit left a dangling relation";
                        window.hide();
                        application.exit(4);
                        return;
                    }
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto cleanupScreenshotPath = waveEditScreenshotPath;
                    const auto suffix = cleanupScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        cleanupScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-relation-cleanup"));
                    } else {
                        cleanupScreenshotPath.append(
                            QStringLiteral("-relation-cleanup.png"));
                    }
                    if (!window.grab().save(cleanupScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save relation cleanup screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeBitRangeAssignment
                    || !wave::findRelation(scenario, "relation-req-ack")) {
                    qCritical().noquote()
                        << "Dependent relation cleanup was not undoable";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto beforeRangeDismissClick = scenario;
                const auto rangeBeforeDismiss = canvas->selectedTimeRange();
                if (!rangeBeforeDismiss) {
                    qCritical().noquote() << "Range disappeared before old overlay hit test";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const QPoint oldOverlayPoint(
                    xAtTick(rangeBeforeDismiss->first) + 20,
                    resetY);
                if (!canvas->viewport()->rect().contains(oldOverlayPoint)) {
                    qCritical().noquote() << "Old overlay hit-test point is outside the viewport";
                    window.hide();
                    application.exit(4);
                    return;
                }
                click(oldOverlayPoint);
                settleLayouts();
                if (scenario != beforeRangeDismissClick
                    || canvas->hasExplicitRangeSelection()
                    || rangePalette->isVisibleTo(&window)
                    || rangeToolbarAction->isVisible()
                    || !stableVerticalLayout()
                    || canvas->selectedTimeRange()) {
                    qCritical().noquote()
                        << "Old overlay area did not clear the range without editing";
                    window.hide();
                    application.exit(4);
                    return;
                }

                drag(
                    QPoint(xAtTick(25'000), requestY),
                    QPoint(xAtTick(45'000), requestY));
                requestLane = wave::findLane(scenario, "lane-request");
                if (!requestLane
                    || valueAt(*requestLane, 15'000) != "0"
                    || valueAt(*requestLane, 25'000) != "1"
                    || valueAt(*requestLane, 35'000) != "1"
                    || valueAt(*requestLane, 45'000) != "1") {
                    qCritical().noquote() << "Drag selection did not toggle all covered bit beats";
                    window.hide();
                    application.exit(4);
                    return;
                }

                dataLane = wave::findLane(scenario, "lane-data");
                if (!dataLane) {
                    qCritical().noquote() << "Primary Bus lane disappeared before range editing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                auto smallBus = *dataLane;
                smallBus.id = "lane-data-small";
                smallBus.name = "data_small";
                smallBus.width = 4;
                smallBus.segments.clear();
                auto& mutableScenarioFixture = const_cast<wave::Scenario&>(scenario);
                const auto dataIterator = std::find_if(
                    mutableScenarioFixture.lanes.begin(),
                    mutableScenarioFixture.lanes.end(),
                    [](const wave::Lane& lane) { return lane.id == "lane-data"; });
                mutableScenarioFixture.lanes.insert(
                    std::next(dataIterator),
                    std::move(smallBus));
                canvas->refreshModel();
                QCoreApplication::processEvents();
                const auto smallBusY = laneCenterY("lane-data-small");
                if (smallBusY < 0) {
                    qCritical().noquote() << "Secondary Bus lane is not visible";
                    window.hide();
                    application.exit(4);
                    return;
                }

                dragModified(
                    QPoint(xAtTick(20'000), dataY),
                    QPoint(xAtTick(50'000), smallBusY),
                    Qt::ShiftModifier);
                settleLayouts();
                const auto busRange = canvas->selectedTimeRange();
                auto* rangeValueEdit = window.findChild<QLineEdit*>(
                    QStringLiteral("RangeEditValueEdit"));
                auto* rangeZeroButton = window.findChild<QToolButton*>(
                    QStringLiteral("RangeEditZeroButton"));
                auto* rangeZButton = window.findChild<QToolButton*>(
                    QStringLiteral("RangeEditZButton"));
                auto* rangeDontCareButton = window.findChild<QToolButton*>(
                    QStringLiteral("RangeEditDontCareButton"));
                if (!busRange
                    || busRange->second <= busRange->first
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedLaneIds()
                        != QStringList{
                            QStringLiteral("lane-data"),
                            QStringLiteral("lane-data-small")}
                    || !rangePalette->isVisibleTo(&window)
                    || !rangeToolbarAction->isVisible()
                    || !rangeContext->text().contains(QStringLiteral("Bus"))
                    || !rangeValueEdit
                    || !rangeValueEdit->isVisible()
                    || !rangeZeroButton
                    || !rangeZeroButton->isVisible()
                    || !rangeZButton
                    || !rangeZButton->isVisible()
                    || !rangeDontCareButton
                    || !rangeDontCareButton->isVisible()
                    || !rangeDontCareButton->isEnabled()
                    || !stableVerticalLayout()) {
                    qCritical().noquote() << "Bus range toolbar is not actionable";
                    window.hide();
                    application.exit(4);
                    return;
                }

                auto* cutRangeAction = window.findChild<QAction*>(
                    QStringLiteral("CutRangeAction"));
                const auto beforeTextCut = scenario;
                rangeValueEdit->setText(QStringLiteral("0xa5"));
                rangeValueEdit->selectAll();
                rangeValueEdit->setFocus(Qt::OtherFocusReason);
                QCoreApplication::processEvents();
                if (!cutRangeAction) {
                    qCritical().noquote() << "Cut range menu action is missing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                cutRangeAction->trigger();
                QCoreApplication::processEvents();
                if (!rangeValueEdit->text().isEmpty()
                    || scenario != beforeTextCut
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != busRange) {
                    qCritical().noquote()
                        << "Ctrl+X route replaced text Cut with a waveform edit";
                    window.hide();
                    application.exit(4);
                    return;
                }
                rangeValueEdit->setModified(false);
                canvas->viewport()->setFocus(Qt::OtherFocusReason);
                const auto fullWindowSize = window.size();
                window.resize(960, fullWindowSize.height());
                settleLayouts();
                const auto toolbarGlobalRect = widgetGlobalRect(waveformToolbar);
                const auto rangePaletteGlobalRect = widgetGlobalRect(rangePalette);
                const std::array<QWidget*, 10> visibleRangeControls{
                    rangeContext,
                    rangeCopyButton,
                    rangeCutButton,
                    rangePasteButton,
                    rangeClearButton,
                    rangeValueEdit,
                    rangeZeroButton,
                    rangeXButton,
                    rangeZButton,
                    rangeDontCareButton,
                };
                auto* measureWidget = waveformToolbar->widgetForAction(measureAction);
                const auto clippedRangeControl = std::any_of(
                    visibleRangeControls.begin(),
                    visibleRangeControls.end(),
                    [&window,
                     &toolbarGlobalRect,
                     &rangePaletteGlobalRect,
                     &widgetGlobalRect](const QWidget* control) {
                        if (!control || !control->isVisibleTo(&window)) return true;
                        const auto controlRect = widgetGlobalRect(control);
                        auto* hit = window.childAt(
                            window.mapFromGlobal(controlRect.center()));
                        return !toolbarGlobalRect.contains(controlRect)
                            || !rangePaletteGlobalRect.contains(controlRect)
                            || (hit != control
                                && (!hit || !control->isAncestorOf(hit)));
                    });
                if (window.width() != 960
                    || window.height() != fullWindowSize.height()
                    || !rangePalette->isVisibleTo(&window)
                    || !rangeToolbarAction->isVisible()
                    || !stableVerticalLayout()
                    || !toolbarGlobalRect.contains(rangePaletteGlobalRect)
                    || rangePaletteGlobalRect.intersects(
                        widgetGlobalRect(canvas->viewport()))
                    || clippedRangeControl
                    || !measureWidget
                    || !measureWidget->isVisibleTo(&window)
                    || !toolbarGlobalRect.contains(widgetGlobalRect(measureWidget))
                    || widgetGlobalRect(measureWidget).left()
                        >= rangePaletteGlobalRect.left()) {
                    qCritical().noquote()
                        << "Range toolbar is clipped or displaced at the minimum window size";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto beforeBusRangeAssignment = scenario;
                const auto* beforeDataLane = wave::findLane(
                    beforeBusRangeAssignment, "lane-data");
                const auto* beforeSmallDataLane = wave::findLane(
                    beforeBusRangeAssignment, "lane-data-small");
                if (!clickWidget(rangeDontCareButton)) {
                    qCritical().noquote()
                        << "Don't care button is not hit-testable in the range toolbar";
                    window.hide();
                    application.exit(4);
                    return;
                }
                dataLane = wave::findLane(scenario, "lane-data");
                auto* smallDataLane = wave::findLane(scenario, "lane-data-small");
                if (!beforeDataLane
                    || !beforeSmallDataLane
                    || !dataLane
                    || !smallDataLane
                    || valueAt(*dataLane, busRange->first + 1) != "0bxxxxxxxx"
                    || valueAt(*dataLane, busRange->second - 1) != "0bxxxxxxxx"
                    || valueAt(*smallDataLane, busRange->first + 1) != "0bxxxx"
                    || valueAt(*smallDataLane, busRange->second - 1) != "0bxxxx"
                    || valueAt(*dataLane, busRange->first - 1)
                        != valueAt(*beforeDataLane, busRange->first - 1)
                    || valueAt(*dataLane, busRange->second)
                        != valueAt(*beforeDataLane, busRange->second)
                    || valueAt(*smallDataLane, busRange->first - 1)
                        != valueAt(*beforeSmallDataLane, busRange->first - 1)
                    || valueAt(*smallDataLane, busRange->second)
                        != valueAt(*beforeSmallDataLane, busRange->second)
                    || !canvas->hasExplicitRangeSelection()) {
                    qCritical().noquote() << "Don't care did not fill both complete Bus ranges";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeBusRangeAssignment) {
                    qCritical().noquote() << "Bus preset range was not one atomic undo";
                    window.hide();
                    application.exit(4);
                    return;
                }

                rangeValueEdit->setFocus(Qt::OtherFocusReason);
                rangeValueEdit->setText(QStringLiteral("0xa5"));
                rangeValueEdit->setModified(true);
                QKeyEvent invalidBusEnter(
                    QEvent::KeyPress,
                    Qt::Key_Return,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(rangeValueEdit, &invalidBusEnter);
                QCoreApplication::processEvents();
                if (scenario != beforeBusRangeAssignment
                    || !rangeValueEdit->isModified()
                    || !canvas->hasExplicitRangeSelection()) {
                    qCritical().noquote() << "Invalid Bus range value caused a partial edit";
                    window.hide();
                    application.exit(4);
                    return;
                }
                measureAction->setChecked(true);
                QCoreApplication::processEvents();
                if (measureAction->isChecked()
                    || canvas->tool() != wave::WaveCanvas::Tool::WaveEdit
                    || scenario != beforeBusRangeAssignment
                    || !rangeValueEdit->isModified()
                    || !rangeToolbarAction->isVisible()) {
                    qCritical().noquote()
                        << "Measure mode discarded or bypassed an invalid Bus range draft";
                    window.hide();
                    application.exit(4);
                    return;
                }
                auto* mutableSmallDataLane = wave::findLane(
                    mutableScenarioFixture,
                    "lane-data-small");
                if (!mutableSmallDataLane) {
                    qCritical().noquote() << "Secondary Bus lane disappeared during refresh test";
                    window.hide();
                    application.exit(4);
                    return;
                }
                mutableSmallDataLane->kind = wave::LaneKind::Bit;
                canvas->refreshModel();
                QCoreApplication::processEvents();
                if (rangeValueEdit->isVisible()
                    || rangeValueEdit->isModified()
                    || !canvas->hasExplicitRangeSelection()) {
                    qCritical().noquote()
                        << "Bus draft survived a refreshed mixed-type selection invisibly";
                    window.hide();
                    application.exit(4);
                    return;
                }
                mutableSmallDataLane = wave::findLane(
                    mutableScenarioFixture,
                    "lane-data-small");
                mutableSmallDataLane->kind = wave::LaneKind::Bus;
                canvas->refreshModel();
                QCoreApplication::processEvents();
                if (!rangeValueEdit->isVisible()
                    || rangeValueEdit->isModified()
                    || !canvas->hasExplicitRangeSelection()) {
                    qCritical().noquote() << "Bus range did not recover cleanly after model refresh";
                    window.hide();
                    application.exit(4);
                    return;
                }

                rangeValueEdit->setText(QStringLiteral("0xa"));
                rangeValueEdit->setModified(true);
                QKeyEvent validBusEnter(
                    QEvent::KeyPress,
                    Qt::Key_Return,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(rangeValueEdit, &validBusEnter);
                QCoreApplication::processEvents();
                dataLane = wave::findLane(scenario, "lane-data");
                smallDataLane = wave::findLane(scenario, "lane-data-small");
                if (!dataLane
                    || !smallDataLane
                    || valueAt(*dataLane, busRange->first + 1) != "0xa"
                    || valueAt(*dataLane, busRange->second - 1) != "0xa"
                    || valueAt(*smallDataLane, busRange->first + 1) != "0xa"
                    || valueAt(*smallDataLane, busRange->second - 1) != "0xa"
                    || valueAt(*dataLane, busRange->first - 1)
                        != valueAt(*beforeDataLane, busRange->first - 1)
                    || valueAt(*dataLane, busRange->second)
                        != valueAt(*beforeDataLane, busRange->second)
                    || valueAt(*smallDataLane, busRange->first - 1)
                        != valueAt(*beforeSmallDataLane, busRange->first - 1)
                    || valueAt(*smallDataLane, busRange->second)
                        != valueAt(*beforeSmallDataLane, busRange->second)
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != busRange) {
                    qCritical().noquote() << "Custom value did not fill the complete Bus range";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto afterBusRangeAssignment = scenario;
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeBusRangeAssignment) {
                    qCritical().noquote() << "Custom Bus range was not one atomic undo";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
                    || scenario != afterBusRangeAssignment) {
                    qCritical().noquote() << "Custom Bus range atomic redo failed";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeBusRangeAssignment) {
                    qCritical().noquote() << "Custom Bus range final undo failed";
                    window.hide();
                    application.exit(4);
                    return;
                }
                window.resize(fullWindowSize);
                settleLayouts();
                if (window.size() != fullWindowSize
                    || !rangePalette->isVisibleTo(&window)
                    || !stableVerticalLayout()
                    || widgetGlobalRect(rangePalette).intersects(
                        widgetGlobalRect(canvas->viewport()))) {
                    qCritical().noquote()
                        << "Range toolbar did not remain detached after restoring the window";
                    window.hide();
                    application.exit(4);
                    return;
                }
                rangeValueEdit->setFocus(Qt::OtherFocusReason);
                QKeyEvent focusedRangeEscape(
                    QEvent::KeyPress,
                    Qt::Key_Escape,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(rangeValueEdit, &focusedRangeEscape);
                settleLayouts();
                if (canvas->hasExplicitRangeSelection()
                    || rangePalette->isVisibleTo(&window)
                    || rangeToolbarAction->isVisible()
                    || !stableVerticalLayout()
                    || canvas->selectedTimeRange()) {
                    qCritical().noquote()
                        << "Escape in the Bus range value field did not clear the selection";
                    window.hide();
                    application.exit(4);
                    return;
                }

                dragModified(
                    QPoint(xAtTick(20'000), requestY),
                    QPoint(xAtTick(40'000), dataY),
                    Qt::ShiftModifier);
                settleLayouts();
                const auto beforeMixedAssignment = scenario;
                const auto mixedRange = canvas->selectedTimeRange();
                const auto mixedLaneIds = canvas->selectedLaneIds();
                if (!canvas->hasExplicitRangeSelection()
                    || !rangePalette->isVisibleTo(&window)
                    || !rangeToolbarAction->isVisible()
                    || !stableVerticalLayout()
                    || !rangeContext->text().contains(QStringLiteral("Copy, cut, or clear"))
                    || !rangeCopyButton
                    || !rangeCopyButton->isVisibleTo(&window)
                    || !rangeCopyButton->isEnabled()
                    || !rangeCutButton->isVisibleTo(&window)
                    || !rangeCutButton->isEnabled()
                    || !rangeClearButton->isVisibleTo(&window)
                    || !rangeClearButton->isEnabled()
                    || rangeValueEdit->isVisibleTo(&window)
                    || rangeZeroButton->isVisibleTo(&window)
                    || rangeOneButton->isVisibleTo(&window)
                    || rangeXButton->isVisibleTo(&window)
                    || rangeZButton->isVisibleTo(&window)
                    || rangeDontCareButton->isVisibleTo(&window)) {
                    qCritical().noquote() << "Mixed range did not enter safe copy/cut/clear state";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto mixedCopyScreenshotPath = waveEditScreenshotPath;
                    const auto suffix = mixedCopyScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        mixedCopyScreenshotPath.insert(suffix, QStringLiteral("-mixed-copy"));
                    } else {
                        mixedCopyScreenshotPath.append(QStringLiteral("-mixed-copy.png"));
                    }
                    if (!window.grab().save(mixedCopyScreenshotPath)) {
                        qCritical().noquote() << "Cannot save mixed range Copy screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                if (!clickWidget(rangeCopyButton)) {
                    qCritical().noquote() << "Mixed range Copy button is not hit-testable";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto* copiedMime = QApplication::clipboard()->mimeData();
                const auto copiedDocument = copiedMime
                    ? QJsonDocument::fromJson(copiedMime->data(
                          QByteArrayLiteral("application/x-wave-workbench-range+json")))
                    : QJsonDocument{};
                const auto copiedRoot = copiedDocument.object();
                const auto copiedLaneObjects =
                    copiedRoot.value(QStringLiteral("lanes")).toArray();
                const auto copiedMetadataValid =
                    copiedRoot.value(QStringLiteral("schemaVersion")).toInt(-1) == 2
                    && std::all_of(
                        copiedLaneObjects.begin(),
                        copiedLaneObjects.end(),
                        [](const QJsonValue& laneValue) {
                            if (!laneValue.isObject()) return false;
                            const auto lane = laneValue.toObject();
                            bool validWidth = false;
                            const auto width = lane.value(QStringLiteral("width"))
                                                   .toString()
                                                   .toULongLong(&validWidth);
                            return lane.value(QStringLiteral("laneId")).isString()
                                && lane.value(QStringLiteral("name")).isString()
                                && lane.value(QStringLiteral("kind")).isString()
                                && lane.value(QStringLiteral("width")).isString()
                                && validWidth
                                && width > 0;
                        });
                bool validCopiedDuration = false;
                const auto copiedDuration = copiedRoot.value(QStringLiteral("durationTick"))
                                                .toString()
                                                .toLongLong(&validCopiedDuration);
                if (scenario != beforeMixedAssignment
                    || !canvas->hasExplicitRangeSelection()
                    || !mixedRange
                    || !copiedMime
                    || !copiedMime->hasFormat(
                        QByteArrayLiteral("application/x-wave-workbench-range+json"))
                    || !copiedDocument.isObject()
                    || copiedRoot.value(QStringLiteral("lanes")).toArray().size()
                        != mixedLaneIds.size()
                    || !copiedMetadataValid
                    || !validCopiedDuration
                    || copiedDuration != mixedRange->second - mixedRange->first
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Copied"))) {
                    qCritical().noquote()
                        << "Visible Copy did not preserve the complete mixed range";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_1);
                if (scenario != beforeMixedAssignment
                    || !canvas->hasExplicitRangeSelection()) {
                    qCritical().noquote() << "Mixed range assignment modified waveform data";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!clickWidget(rangeCutButton)) {
                    qCritical().noquote() << "Mixed range Cut button is not hit-testable";
                    window.hide();
                    application.exit(4);
                    return;
                }
                settleLayouts();
                const auto afterCut = scenario;
                const auto* cutMime = QApplication::clipboard()->mimeData();
                const auto cutDocument = cutMime
                    ? QJsonDocument::fromJson(cutMime->data(
                          QByteArrayLiteral("application/x-wave-workbench-range+json")))
                    : QJsonDocument{};
                if (afterCut == beforeMixedAssignment
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != mixedRange
                    || !rangePalette->isVisibleTo(&window)
                    || !rangeToolbarAction->isVisible()
                    || !cutMime
                    || cutDocument != copiedDocument
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Cut"))) {
                    qCritical().noquote()
                        << "Visible Cut did not copy and clear the mixed range in place";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeMixedAssignment
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != mixedRange
                    || !rangePalette->isVisibleTo(&window)) {
                    qCritical().noquote() << "Range Cut was not one atomic source undo";
                    window.hide();
                    application.exit(4);
                    return;
                }
                bool pasteMenuHandled = false;
                QTimer::singleShot(
                    0,
                    &application,
                    [&application, &pasteMenuHandled] {
                        auto* menu = qobject_cast<QMenu*>(
                            QApplication::activePopupWidget());
                        auto* pasteRange = menu
                            ? menu->findChild<QAction*>(
                                  QStringLiteral("PasteRangeHereAction"))
                            : nullptr;
                        if (!menu
                            || menu->objectName() != QStringLiteral("WaveformContextMenu")
                            || !pasteRange
                            || pasteRange->text()
                                != QStringLiteral("Paste copied range here")) {
                            qCritical().noquote()
                                << "Waveform context menu Paste here action is missing";
                            if (menu) menu->close();
                            application.exit(4);
                            return;
                        }
                        menu->setActiveAction(pasteRange);
                        pasteMenuHandled = true;
                        QKeyEvent enter(
                            QEvent::KeyPress,
                            Qt::Key_Return,
                            Qt::NoModifier);
                        QCoreApplication::sendEvent(menu, &enter);
                    });
                const QPoint pasteContextPoint(xAtTick(90'000), requestY);
                QContextMenuEvent pasteContext(
                    QContextMenuEvent::Mouse,
                    pasteContextPoint,
                    canvas->viewport()->mapToGlobal(pasteContextPoint));
                QCoreApplication::sendEvent(canvas->viewport(), &pasteContext);
                settleLayouts();
                const auto pastedRange = canvas->selectedTimeRange();
                if (!pasteMenuHandled
                    || scenario == beforeMixedAssignment
                    || !canvas->hasExplicitRangeSelection()
                    || !rangePalette->isVisibleTo(&window)
                    || !rangeToolbarAction->isVisible()
                    || !stableVerticalLayout()
                    || !rangeContext->text().contains(QStringLiteral("Copy, cut, or clear"))
                    || !pastedRange
                    || pastedRange->first <= mixedRange->second
                    || pastedRange->second - pastedRange->first != copiedDuration
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Pasted"))) {
                    qCritical().noquote()
                        << "Context-menu Paste here did not apply at the clicked destination";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto pastedRangeScreenshotPath = waveEditScreenshotPath;
                    const auto suffix = pastedRangeScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        pastedRangeScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-pasted-range"));
                    } else {
                        pastedRangeScreenshotPath.append(QStringLiteral("-pasted-range.png"));
                    }
                    if (!window.grab().save(pastedRangeScreenshotPath)) {
                        qCritical().noquote() << "Cannot save pasted range screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                const auto afterPaste = scenario;
                if (!clickWidget(rangeClearButton)) {
                    qCritical().noquote() << "Selected range Clear button is not hit-testable";
                    window.hide();
                    application.exit(4);
                    return;
                }
                settleLayouts();
                const auto afterClear = scenario;
                if (afterClear == afterPaste
                    || !canvas->hasExplicitRangeSelection()
                    || !rangePalette->isVisibleTo(&window)
                    || !rangeToolbarAction->isVisible()
                    || canvas->selectedTimeRange() != pastedRange
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Cleared"))) {
                    qCritical().noquote()
                        << "Visible Clear did not clear the pasted range in place";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != afterPaste
                    || !canvas->hasExplicitRangeSelection()
                    || !rangePalette->isVisibleTo(&window)
                    || canvas->selectedTimeRange() != pastedRange) {
                    qCritical().noquote() << "Range Clear was not one atomic undo";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_Delete);
                settleLayouts();
                if (scenario == afterPaste
                    || !canvas->hasExplicitRangeSelection()
                    || !rangePalette->isVisibleTo(&window)
                    || canvas->selectedTimeRange() != pastedRange
                    || !window.statusBar()->currentMessage().startsWith(
                        QStringLiteral("Cleared"))) {
                    qCritical().noquote()
                        << "Delete did not clear the selected range while retaining context";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != afterPaste
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != pastedRange) {
                    qCritical().noquote() << "Delete range clear was not one atomic undo";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeMixedAssignment
                    || !canvas->hasExplicitRangeSelection()
                    || !rangePalette->isVisibleTo(&window)
                    || !rangeToolbarAction->isVisible()
                    || canvas->selectedTimeRange() != pastedRange) {
                    qCritical().noquote() << "Context-menu Paste here was not one atomic undo";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_Escape);
                settleLayouts();
                if (canvas->hasExplicitRangeSelection()
                    || rangePalette->isVisibleTo(&window)
                    || rangeToolbarAction->isVisible()
                    || !stableVerticalLayout()
                    || canvas->selectedTimeRange()) {
                    qCritical().noquote() << "Escape did not clear the mixed range";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto beforeMultiTargetPaste = scenario;
                dragModified(
                    QPoint(xAtTick(120'000), requestY),
                    QPoint(xAtTick(130'000), acknowledgeY),
                    Qt::ShiftModifier);
                settleLayouts();
                const auto multiPasteSourceRange = canvas->selectedTimeRange();
                const auto multiPasteSourceIds = canvas->selectedLaneIds();
                const auto* multiSourceRequest = wave::findLane(scenario, "lane-request");
                const auto* multiSourceAcknowledge = wave::findLane(scenario, "lane-ack");
                if (!multiPasteSourceRange
                    || multiPasteSourceRange->second - multiPasteSourceRange->first
                        != 10'000
                    || multiPasteSourceIds
                        != QStringList{
                            QStringLiteral("lane-request"),
                            QStringLiteral("lane-ack")}
                    || !multiSourceRequest
                    || !multiSourceAcknowledge
                    || valueAt(*multiSourceRequest, 125'000) != "1"
                    || valueAt(*multiSourceAcknowledge, 125'000) != "1"
                    || !clickWidget(rangeCopyButton)) {
                    qCritical().noquote()
                        << "Multi-lane target Paste source could not be copied";
                    window.hide();
                    application.exit(4);
                    return;
                }

                sendKey(Qt::Key_Escape);
                settleLayouts();
                dragModified(
                    QPoint(xAtTick(20'000), resetY),
                    QPoint(xAtTick(30'000), resetY),
                    Qt::ShiftModifier);
                settleLayouts();
                if (!rangePasteButton
                    || !rangePasteButton->isVisibleTo(&window)
                    || !rangePasteButton->isEnabled()
                    || !clickWidget(rangePasteButton)) {
                    qCritical().noquote()
                        << "Visible range Paste button is unavailable for count validation";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (scenario != beforeMultiTargetPaste
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("2 copied signal(s) into 1 selected signal(s)"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("select the same number of targets"))) {
                    qCritical().noquote()
                        << "Multi-lane Paste count mismatch was not rejected visibly";
                    window.hide();
                    application.exit(4);
                    return;
                }

                sendKey(Qt::Key_Escape);
                settleLayouts();
                dragModified(
                    QPoint(xAtTick(20'000), resetY),
                    QPoint(xAtTick(30'000), requestY),
                    Qt::ShiftModifier);
                settleLayouts();
                const auto multiTargetRange = canvas->selectedTimeRange();
                const auto multiTargetIds = canvas->selectedLaneIds();
                if (!multiTargetRange
                    || multiTargetRange->first != 20'000
                    || multiTargetRange->second != 30'000
                    || multiTargetIds
                        != QStringList{
                            QStringLiteral("lane-reset"),
                            QStringLiteral("lane-request")}
                    || !rangePasteButton->isVisibleTo(&window)
                    || !rangePasteButton->isEnabled()
                    || !clickWidget(rangePasteButton)) {
                    qCritical().noquote()
                        << "Matching multi-lane target selection could not invoke Paste";
                    window.hide();
                    application.exit(4);
                    return;
                }
                settleLayouts();
                const auto afterMultiTargetPaste = scenario;
                const auto* multiTargetReset = wave::findLane(scenario, "lane-reset");
                const auto* multiTargetRequest = wave::findLane(scenario, "lane-request");
                const auto* unchangedMultiSourceAck = wave::findLane(scenario, "lane-ack");
                if (afterMultiTargetPaste == beforeMultiTargetPaste
                    || !multiTargetReset
                    || !multiTargetRequest
                    || !unchangedMultiSourceAck
                    || valueAt(*multiTargetReset, 25'000) != "1"
                    || valueAt(*multiTargetRequest, 25'000) != "1"
                    || unchangedMultiSourceAck->segments
                        != wave::findLane(beforeMultiTargetPaste, "lane-ack")->segments
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedTimeRange() != multiTargetRange
                    || canvas->selectedLaneIds() != multiTargetIds
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("2 copied signals → 2 selected signals"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Multi-lane Paste did not map sources to selected targets in order";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto multiTargetScreenshotPath = waveEditScreenshotPath;
                    const auto suffix = multiTargetScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        multiTargetScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-multi-target-paste"));
                    } else {
                        multiTargetScreenshotPath.append(
                            QStringLiteral("-multi-target-paste.png"));
                    }
                    if (!window.grab().save(multiTargetScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save multi-target Paste screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeMultiTargetPaste
                    || !QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
                    || scenario != afterMultiTargetPaste
                    || !QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeMultiTargetPaste) {
                    qCritical().noquote()
                        << "Multi-lane target Paste Undo/Redo was not atomic";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_Escape);
                settleLayouts();

                const auto beforeTargetAwarePaste = scenario;
                dragModified(
                    QPoint(xAtTick(90'000), requestY),
                    QPoint(xAtTick(100'000), requestY),
                    Qt::ShiftModifier);
                settleLayouts();
                const auto copiedSingleRange = canvas->selectedTimeRange();
                const auto copiedSingleLaneIds = canvas->selectedLaneIds();
                const auto* sourceRequest = wave::findLane(scenario, "lane-request");
                const auto* targetAcknowledge = wave::findLane(scenario, "lane-ack");
                if (!copiedSingleRange
                    || copiedSingleRange->second <= copiedSingleRange->first
                    || copiedSingleLaneIds.size() != 1
                    || copiedSingleLaneIds.front() != QStringLiteral("lane-request")
                    || !sourceRequest
                    || !targetAcknowledge
                    || valueAt(*sourceRequest, 95'000)
                        == valueAt(*targetAcknowledge, 55'000)
                    || !clickWidget(rangeCopyButton)) {
                    qCritical().noquote()
                        << "Single-lane target-aware Paste fixture could not be copied";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto copiedRequestValue = valueAt(*sourceRequest, 95'000);
                const auto* snapshotMime = QApplication::clipboard()->mimeData();
                const auto snapshotDocument = snapshotMime
                    ? QJsonDocument::fromJson(snapshotMime->data(
                          QByteArrayLiteral("application/x-wave-workbench-range+json")))
                    : QJsonDocument{};
                auto legacyRoot = snapshotDocument.object();
                auto legacyLanes = legacyRoot.value(QStringLiteral("lanes")).toArray();
                for (auto index = 0; index < legacyLanes.size(); ++index) {
                    auto legacyLane = legacyLanes.at(index).toObject();
                    legacyLane.remove(QStringLiteral("name"));
                    legacyLane.remove(QStringLiteral("kind"));
                    legacyLane.remove(QStringLiteral("width"));
                    legacyLanes.replace(index, legacyLane);
                }
                legacyRoot.insert(QStringLiteral("schemaVersion"), 1);
                legacyRoot.insert(QStringLiteral("lanes"), legacyLanes);
                const auto legacyBytes =
                    QJsonDocument(legacyRoot).toJson(QJsonDocument::Compact);
                auto* legacyMime = new QMimeData;
                legacyMime->setData(
                    QByteArrayLiteral("application/x-wave-workbench-range+json"),
                    legacyBytes);
                legacyMime->setText(QString::fromUtf8(legacyBytes));
                QApplication::clipboard()->setMimeData(legacyMime);
                if (!chooseWaveformAction(
                        QPoint(xAtTick(50'000), dataY),
                        QStringLiteral("Paste copied range here"))) {
                    qCritical().noquote()
                        << "Incompatible target Paste action could not be invoked";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto incompatiblePasteMessage =
                    window.statusBar()->currentMessage();
                if (scenario != beforeTargetAwarePaste
                    || !incompatiblePasteMessage.contains(
                        QStringLiteral("Cannot paste req"))
                    || !incompatiblePasteMessage.contains(
                        QStringLiteral("signal types must match"))) {
                    qCritical().noquote()
                        << "Incompatible target Paste did not fail visibly and atomically";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!chooseWaveformAction(
                        QPoint(xAtTick(50'000), acknowledgeY),
                        QStringLiteral("Paste copied range here"))) {
                    qCritical().noquote()
                        << "Compatible target-aware Paste action could not be invoked";
                    window.hide();
                    application.exit(4);
                    return;
                }
                settleLayouts();
                const auto afterTargetAwarePaste = scenario;
                const auto* pastedRequest = wave::findLane(scenario, "lane-request");
                const auto* pastedAcknowledge = wave::findLane(scenario, "lane-ack");
                const auto targetPasteRange = canvas->selectedTimeRange();
                const auto targetPasteLaneIds = canvas->selectedLaneIds();
                if (afterTargetAwarePaste == beforeTargetAwarePaste
                    || !pastedRequest
                    || !pastedAcknowledge
                    || pastedRequest->segments
                        != wave::findLane(beforeTargetAwarePaste, "lane-request")->segments
                    || valueAt(*pastedAcknowledge, 55'000) != copiedRequestValue
                    || !canvas->hasExplicitRangeSelection()
                    || !targetPasteRange
                    || targetPasteRange->first != 50'000
                    || targetPasteRange->second - targetPasteRange->first
                        != copiedSingleRange->second - copiedSingleRange->first
                    || targetPasteLaneIds.size() != 1
                    || targetPasteLaneIds.front() != QStringLiteral("lane-ack")
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("req → ack"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Single-lane Paste did not use the clicked compatible target";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto targetPasteScreenshotPath = waveEditScreenshotPath;
                    const auto suffix = targetPasteScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        targetPasteScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-target-aware-paste"));
                    } else {
                        targetPasteScreenshotPath.append(
                            QStringLiteral("-target-aware-paste.png"));
                    }
                    if (!window.grab().save(targetPasteScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save target-aware Paste screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeTargetAwarePaste) {
                    qCritical().noquote()
                        << "Target-aware Paste was not one atomic Undo";
                    window.hide();
                    application.exit(4);
                    return;
                }

                sendKey(Qt::Key_Escape);
                settleLayouts();
                dragModified(
                    QPoint(xAtTick(20'000), clockY),
                    QPoint(xAtTick(30'000), clockY),
                    Qt::ShiftModifier);
                settleLayouts();
                if (!canvas->hasExplicitRangeSelection()
                    || canvas->selectedLaneIds().size() != 1
                    || canvas->selectedLaneIds().front() != QStringLiteral("lane-clk")
                    || !clickWidget(rangeCopyButton)
                    || !chooseWaveformAction(
                        QPoint(xAtTick(40'000), clockY),
                        QStringLiteral("Paste copied range here"))) {
                    qCritical().noquote()
                        << "Normal Clock no-effect Paste workflow could not be invoked";
                    window.hide();
                    application.exit(4);
                    return;
                }
                settleLayouts();
                const auto noEffectPasteMessage = window.statusBar()->currentMessage();
                if (scenario != beforeTargetAwarePaste
                    || !canvas->hasExplicitRangeSelection()
                    || canvas->selectedLaneIds().size() != 1
                    || canvas->selectedLaneIds().front() != QStringLiteral("lane-clk")
                    || !noEffectPasteMessage.contains(
                        QStringLiteral("already matches copied range"))
                    || !noEffectPasteMessage.contains(
                        QStringLiteral("no values changed"))
                    || noEffectPasteMessage.contains(QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "No-effect Clock Paste changed the model or misreported its result";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto noEffectPasteScreenshotPath = waveEditScreenshotPath;
                    const auto suffix = noEffectPasteScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        noEffectPasteScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-paste-no-effect-feedback"));
                    } else {
                        noEffectPasteScreenshotPath.append(
                            QStringLiteral("-paste-no-effect-feedback.png"));
                    }
                    if (!window.grab().save(noEffectPasteScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save no-effect Paste screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                if (!QMetaObject::invokeMethod(&window, "redo", Qt::DirectConnection)
                    || scenario != afterTargetAwarePaste) {
                    qCritical().noquote()
                        << "No-effect Paste discarded the real target Paste Redo";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeTargetAwarePaste) {
                    qCritical().noquote()
                        << "Target-aware Paste baseline was not recoverable after Redo";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_Escape);
                settleLayouts();

                const auto beforeDeletedSourcePaste = scenario;
                dragModified(
                    QPoint(xAtTick(90'000), requestY),
                    QPoint(xAtTick(100'000), requestY),
                    Qt::ShiftModifier);
                settleLayouts();
                const auto* durableSource = wave::findLane(scenario, "lane-request");
                const auto* durableTarget = wave::findLane(scenario, "lane-ack");
                const auto durableSourceValue =
                    durableSource ? valueAt(*durableSource, 95'000) : std::string{};
                const auto durableTargetValue =
                    durableTarget ? valueAt(*durableTarget, 55'000) : std::string{};
                if (!durableSource
                    || !durableTarget
                    || durableSourceValue == durableTargetValue
                    || !clickWidget(rangeCopyButton)) {
                    qCritical().noquote()
                        << "Durable clipboard source range could not be copied";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto* durableMime = QApplication::clipboard()->mimeData();
                const auto durableDocument = durableMime
                    ? QJsonDocument::fromJson(durableMime->data(
                          QByteArrayLiteral("application/x-wave-workbench-range+json")))
                    : QJsonDocument{};
                const auto durableRoot = durableDocument.object();
                const auto durableLanes =
                    durableRoot.value(QStringLiteral("lanes")).toArray();
                const auto durableLane = durableLanes.size() == 1
                    ? durableLanes.first().toObject()
                    : QJsonObject{};
                bool validDurableDuration = false;
                const auto durableDuration =
                    durableRoot.value(QStringLiteral("durationTick"))
                        .toString()
                        .toLongLong(&validDurableDuration);
                if (durableRoot.value(QStringLiteral("schemaVersion")).toInt(-1) != 2
                    || durableLane.value(QStringLiteral("name")).toString()
                        != QStringLiteral("req")
                    || durableLane.value(QStringLiteral("kind")).toString()
                        != QStringLiteral("bit")
                    || durableLane.value(QStringLiteral("width")).toString()
                        != QStringLiteral("1")
                    || !validDurableDuration
                    || durableDuration != 10'000) {
                    qCritical().noquote()
                        << "Clipboard range is not a self-describing snapshot";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto endBeforeExtendedPaste = scenario.duration;
                const auto nearEndPasteStart = endBeforeExtendedPaste;
                const auto expectedExtendedEnd =
                    endBeforeExtendedPaste + durableDuration;
                const auto extendedProbeTick = endBeforeExtendedPaste + 2'000;
                const auto expectedExtendedEndLabel = QString::fromStdString(
                    wave::formatTick(expectedExtendedEnd, window.project().timeBase));
                if (!chooseWaveformAction(
                        QPoint(
                            std::max(191, xAtTick(nearEndPasteStart) - 1),
                            acknowledgeY),
                        QStringLiteral("Paste copied range here"))) {
                    qCritical().noquote()
                        << "Near-End Paste action could not be invoked";
                    window.hide();
                    application.exit(4);
                    return;
                }
                settleLayouts();
                const auto extendedPasteRange = canvas->selectedTimeRange();
                const auto* extendedPasteTarget =
                    wave::findLane(scenario, "lane-ack");
                if (!extendedPasteRange
                    || extendedPasteRange->first != nearEndPasteStart
                    || extendedPasteRange->second != expectedExtendedEnd
                    || scenario.duration != expectedExtendedEnd
                    || !extendedPasteTarget
                    || valueAt(*extendedPasteTarget, extendedProbeTick) != durableSourceValue
                    || canvas->horizontalScrollBar()->value() <= 0
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("End extended to %1").arg(expectedExtendedEndLabel))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    const auto failedExtendedRange = extendedPasteRange.value_or(
                        std::pair<wave::Tick, wave::Tick>{-1, -1});


                    qCritical().noquote()
                        << "Near-End Paste was truncated or not visibly extended"
                        << "range" << failedExtendedRange.first << failedExtendedRange.second
                        << "end" << scenario.duration
                        << "value"
                        << (extendedPasteTarget
                                ? QString::fromStdString(
                                      valueAt(*extendedPasteTarget, extendedProbeTick))
                                : QStringLiteral("<missing>"))
                        << "scroll" << canvas->horizontalScrollBar()->value()
                        << "status" << window.statusBar()->currentMessage();
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto extendedPasteScreenshotPath = waveEditScreenshotPath;
                    const auto suffix =
                        extendedPasteScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        extendedPasteScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-paste-extends-end"));
                    } else {
                        extendedPasteScreenshotPath.append(
                            QStringLiteral("-paste-extends-end.png"));
                    }
                    if (!window.grab().save(extendedPasteScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save extended Paste screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeDeletedSourcePaste) {
                    qCritical().noquote()
                        << "Extended Paste was not one atomic Undo";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_Escape);
                settleLayouts();

                bool sourceDeleteConfirmed = false;
                QTimer::singleShot(0, &application, [&sourceDeleteConfirmed] {
                    auto* confirmation = qobject_cast<QMessageBox*>(
                        QApplication::activeModalWidget());
                    auto* yes = confirmation
                        ? confirmation->button(QMessageBox::Yes)
                        : nullptr;
                    if (!confirmation || !yes) {
                        if (confirmation) confirmation->reject();
                        return;
                    }
                    sourceDeleteConfirmed = true;
                    yes->click();
                });
                if (!QMetaObject::invokeMethod(
                        canvas,
                        "removeLaneRequested",
                        Qt::DirectConnection,
                        Q_ARG(QString, QStringLiteral("lane-request")))) {
                    qCritical().noquote()
                        << "Copied source signal could not be removed";
                    window.hide();
                    application.exit(4);
                    return;
                }
                settleLayouts();
                const auto afterSourceRemoval = scenario;
                const auto ackYAfterSourceRemoval = laneCenterY("lane-ack");
                if (!sourceDeleteConfirmed
                    || wave::findLane(scenario, "lane-request")
                    || ackYAfterSourceRemoval < 0) {
                    qCritical().noquote()
                        << "Copied source signal was not removed cleanly";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!QMetaObject::invokeMethod(
                        canvas,
                        "pasteAtCursor",
                        Qt::DirectConnection)
                    || scenario != afterSourceRemoval
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("original signal no longer exists"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("select 1 target signal"))) {
                    qCritical().noquote()
                        << "Deleted-source Paste did not explain how to recover";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!chooseWaveformAction(
                        QPoint(xAtTick(50'000), ackYAfterSourceRemoval),
                        QStringLiteral("Paste copied range here"))) {
                    qCritical().noquote()
                        << "Deleted-source clipboard could not target a remaining signal";
                    window.hide();
                    application.exit(4);
                    return;
                }
                settleLayouts();
                const auto afterDeletedSourcePaste = scenario;
                const auto* durablePastedTarget =
                    wave::findLane(scenario, "lane-ack");
                const auto durablePasteRange = canvas->selectedTimeRange();
                if (afterDeletedSourcePaste == afterSourceRemoval
                    || wave::findLane(scenario, "lane-request")
                    || !durablePastedTarget
                    || valueAt(*durablePastedTarget, 55'000) != durableSourceValue
                    || !durablePasteRange
                    || *durablePasteRange
                        != std::pair<wave::Tick, wave::Tick>{50'000, 60'000}
                    || canvas->selectedLaneIds()
                        != QStringList{QStringLiteral("lane-ack")}
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("req → ack"))
                    || !window.statusBar()->currentMessage().contains(
                        QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Self-describing clipboard did not survive source deletion";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()) {
                    auto durablePasteScreenshotPath = waveEditScreenshotPath;
                    const auto suffix =
                        durablePasteScreenshotPath.lastIndexOf(QLatin1Char('.'));
                    if (suffix >= 0) {
                        durablePasteScreenshotPath.insert(
                            suffix,
                            QStringLiteral("-source-deleted-paste"));
                    } else {
                        durablePasteScreenshotPath.append(
                            QStringLiteral("-source-deleted-paste.png"));
                    }
                    if (!window.grab().save(durablePasteScreenshotPath)) {
                        qCritical().noquote()
                            << "Cannot save source-deleted Paste screenshot";
                        window.hide();
                        application.exit(3);
                        return;
                    }
                }
                if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != afterSourceRemoval
                    || wave::findLane(scenario, "lane-request")
                    || !QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                    || scenario != beforeDeletedSourcePaste) {
                    qCritical().noquote()
                        << "Paste and source deletion were not independently undoable";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_Escape);
                settleLayouts();

                canvas->viewport()->update();
                QCoreApplication::processEvents();
                const auto transition = std::find_if(
                    scenario.events.begin(),
                    scenario.events.end(),
                    [](const wave::Event& event) {
                        return event.laneId == "lane-request"
                            && event.tick == 80'000
                            && event.waveformLinked;
                    });
                if (transition == scenario.events.end()) {
                    qCritical().noquote() << "Wave Edit transition event is missing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto transitionId = transition->id;
                requestLane = wave::findLane(scenario, "lane-request");
                const auto linkedSegment = requestLane
                    ? std::find_if(
                          requestLane->segments.begin(),
                          requestLane->segments.end(),
                          [&transition](const wave::Segment& segment) {
                              return segment.id == transition->linkedSegmentId;
                          })
                    : std::vector<wave::Segment>::iterator{};
                if (!requestLane || linkedSegment == requestLane->segments.end()) {
                    qCritical().noquote() << "Wave Edit transition segment is missing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto expectedPreviewRange = std::pair<wave::Tick, wave::Tick>{
                    transition->tick,
                    linkedSegment->end,
                };
                const auto transitionTargetTick = tickAtPoint(
                    QPoint(xAtTick(90'000), requestY));
                sendMouse(
                    QEvent::MouseButtonPress,
                    QPoint(xAtTick(80'000), requestY),
                    Qt::LeftButton,
                    Qt::LeftButton);
                sendMouse(
                    QEvent::MouseMove,
                    QPoint(xAtTick(90'000), requestY),
                    Qt::NoButton,
                    Qt::LeftButton);
                QCoreApplication::processEvents();
                const auto* unchangedDuringPreview = wave::findEvent(
                    scenario,
                    transitionId);
                const auto previewRange = canvas->waveEditTransitionPreviewRange();
                if (!canvas->waveEditTransitionPreviewTick()
                    || *canvas->waveEditTransitionPreviewTick() != transitionTargetTick
                    || !previewRange
                    || *previewRange != expectedPreviewRange
                    || previewRange->first <= 0
                    || !unchangedDuringPreview
                    || unchangedDuringPreview->tick != 80'000) {
                    sendMouse(
                        QEvent::MouseButtonRelease,
                        QPoint(xAtTick(90'000), requestY),
                        Qt::LeftButton,
                        Qt::NoButton);
                    qCritical().noquote()
                        << "Bit transition drag did not provide a localized non-mutating preview";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveEditScreenshotPath.isEmpty()
                    && !window.grab().save(waveEditScreenshotPath)) {
                    sendMouse(
                        QEvent::MouseButtonRelease,
                        QPoint(xAtTick(90'000), requestY),
                        Qt::LeftButton,
                        Qt::NoButton);
                    qCritical().noquote() << "Cannot save Wave Edit screenshot";
                    window.hide();
                    application.exit(3);
                    return;
                }
                sendMouse(
                    QEvent::MouseButtonRelease,
                    QPoint(xAtTick(90'000), requestY),
                    Qt::LeftButton,
                    Qt::NoButton);
                QCoreApplication::processEvents();
                const auto* movedTransition = wave::findEvent(scenario, transitionId);
                requestLane = wave::findLane(scenario, "lane-request");
                if (!movedTransition
                    || movedTransition->tick != transitionTargetTick
                    || !requestLane
                    || valueAt(*requestLane, 85'000) != "0"
                    || valueAt(*requestLane, 95'000) != "1") {
                    qCritical().noquote() << "Bit transition drag did not commit the linked waveform";
                    window.hide();
                    application.exit(4);
                    return;
                }
                auto* undoAction = window.findChild<QAction*>(QStringLiteral("UndoAction"));
                auto* redoAction = window.findChild<QAction*>(QStringLiteral("RedoAction"));
                if (!undoAction || !redoAction) {
                    qCritical().noquote() << "Wave Edit Undo/Redo menu actions are missing";
                    window.hide();
                    application.exit(4);
                    return;
                }
                undoAction->trigger();
                QCoreApplication::processEvents();
                const auto* undoneTransition = wave::findEvent(scenario, transitionId);
                if (!undoneTransition || undoneTransition->tick != 80'000) {
                    qCritical().noquote() << "Bit transition drag undo failed";
                    window.hide();
                    application.exit(4);
                    return;
                }
                redoAction->trigger();
                QCoreApplication::processEvents();
                const auto* redoneTransition = wave::findEvent(scenario, transitionId);
                if (!redoneTransition || redoneTransition->tick != transitionTargetTick) {
                    qCritical().noquote() << "Bit transition drag redo failed";
                    window.hide();
                    application.exit(4);
                    return;
                }

                canvas->zoomIn();
                canvas->zoomIn();
                const auto scrollMaximum = canvas->horizontalScrollBar()->maximum();
                canvas->horizontalScrollBar()->setValue(scrollMaximum / 2);
                const auto scrollBeforePan = canvas->horizontalScrollBar()->value();
                const QPoint panStart(700, dataY);
                const QPoint panEnd(620, dataY);
                sendMouse(
                    QEvent::MouseButtonPress,
                    panStart,
                    Qt::MiddleButton,
                    Qt::MiddleButton);
                sendMouse(
                    QEvent::MouseMove,
                    panEnd,
                    Qt::NoButton,
                    Qt::MiddleButton);
                sendMouse(
                    QEvent::MouseButtonRelease,
                    panEnd,
                    Qt::MiddleButton,
                    Qt::NoButton);
                QCoreApplication::processEvents();
                if (scrollMaximum <= 0
                    || canvas->horizontalScrollBar()->value() <= scrollBeforePan) {
                    qCritical().noquote() << "Middle-button drag did not pan the timeline";
                    window.hide();
                    application.exit(4);
                    return;
                }
                canvas->fitScenario();

                if (canvas->tool() != wave::WaveCanvas::Tool::WaveEdit
                    || measureAction->isChecked()) {
                    qCritical().noquote() << "Direct editing did not remain the default tool";
                    window.hide();
                    application.exit(4);
                    return;
                }
                canvas->setFocus(Qt::OtherFocusReason);
                QKeyEvent escapeEvent(
                    QEvent::KeyPress,
                    Qt::Key_Escape,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(canvas, &escapeEvent);
                if (!canvas->selectedSegmentId().isEmpty()) {
                    qCritical().noquote() << "Escape did not clear Segment editing state";
                    window.hide();
                    application.exit(4);
                    return;
                }
                window.hide();
                application.exit(0);
            });
    } else if (!editMenuScreenshotPath.isEmpty()) {
        QTimer::singleShot(0, &window, [&window] {
            window.revealLocation(QStringLiteral("lane-request"), 80'000);
            window.openEditMenuPreview();
        });
        QTimer::singleShot(
            350,
            &application,
            [&application, &window, editMenuScreenshotPath] {
                auto* popup = QApplication::activePopupWidget();
                if (!popup || !popup->grab().save(editMenuScreenshotPath)) {
                    qCritical().noquote()
                        << "Cannot capture Edit menu:"
                        << editMenuScreenshotPath;
                    if (popup) popup->close();
                    window.hide();
                    application.exit(3);
                    return;
                }
                popup->close();
                window.hide();
                application.exit(0);
            });
    } else if (!laneDialogScreenshotPath.isEmpty()) {
        struct LaneDialogSmokeState {
            QDialog* dialog{};
            QLineEdit* name{};
            QComboBox* kind{};
            QLineEdit* width{};
            QCheckBox* signedValue{};
            QComboBox* radix{};
            QLineEdit* enumMap{};
            QComboBox* clock{};
            QComboBox* group{};
            QLineEdit* color{};
            QSpinBox* height{};
            QCheckBox* visible{};
            QLabel* error{};
            QLabel* saveState{};
            QAction* undoAction{};
            QAbstractButton* ok{};
            wave::Lane originalLane;
            QString originalName;
            QString originalWidth;
            QString originalEnumMap;
            QString originalColor;
            int originalKindIndex{-1};
            int originalRadixIndex{-1};
            int originalClockIndex{-1};
            int originalGroupIndex{-1};
            int originalHeight{};
            bool originalSigned{};
            bool originalVisible{};
            int stage{};
        };
        const auto state = std::make_shared<LaneDialogSmokeState>();
        QTimer::singleShot(0, &window, [&window] {
            window.openLanePropertiesPreview(QStringLiteral("lane-data"));
        });
        QTimer::singleShot(
            350,
            &application,
            [&application, &window, laneDialogScreenshotPath, state] {
                state->dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                state->name = state->dialog
                    ? state->dialog->findChild<QLineEdit*>(
                          QStringLiteral("LanePropertiesNameEdit"))
                    : nullptr;
                state->kind = state->dialog
                    ? state->dialog->findChild<QComboBox*>(
                          QStringLiteral("LanePropertiesKindCombo"))
                    : nullptr;
                state->width = state->dialog
                    ? state->dialog->findChild<QLineEdit*>(
                          QStringLiteral("LanePropertiesWidthEdit"))
                    : nullptr;
                state->signedValue = state->dialog
                    ? state->dialog->findChild<QCheckBox*>(
                          QStringLiteral("LanePropertiesSignedCheck"))
                    : nullptr;
                state->radix = state->dialog
                    ? state->dialog->findChild<QComboBox*>(
                          QStringLiteral("LanePropertiesRadixCombo"))
                    : nullptr;
                state->enumMap = state->dialog
                    ? state->dialog->findChild<QLineEdit*>(
                          QStringLiteral("LanePropertiesEnumMapEdit"))
                    : nullptr;
                state->clock = state->dialog
                    ? state->dialog->findChild<QComboBox*>(
                          QStringLiteral("LanePropertiesClockCombo"))
                    : nullptr;
                state->group = state->dialog
                    ? state->dialog->findChild<QComboBox*>(
                          QStringLiteral("LanePropertiesGroupCombo"))
                    : nullptr;
                state->color = state->dialog
                    ? state->dialog->findChild<QLineEdit*>(
                          QStringLiteral("LanePropertiesColorEdit"))
                    : nullptr;
                state->height = state->dialog
                    ? state->dialog->findChild<QSpinBox*>(
                          QStringLiteral("LanePropertiesHeightSpin"))
                    : nullptr;
                state->visible = state->dialog
                    ? state->dialog->findChild<QCheckBox*>(
                          QStringLiteral("LanePropertiesVisibleCheck"))
                    : nullptr;
                state->error = state->dialog
                    ? state->dialog->findChild<QLabel*>(
                          QStringLiteral("LanePropertiesError"))
                    : nullptr;
                state->saveState = window.findChild<QLabel*>(
                    QStringLiteral("SaveStateLabel"));
                state->undoAction = window.findChild<QAction*>(
                    QStringLiteral("UndoAction"));
                const auto* buttons = state->dialog
                    ? state->dialog->findChild<QDialogButtonBox*>()
                    : nullptr;
                state->ok = buttons ? buttons->button(QDialogButtonBox::Ok) : nullptr;
                const auto* original = !window.project().scenarios.empty()
                    ? wave::findLane(
                          window.project().scenarios.front(),
                          "lane-data")
                    : nullptr;
                const auto fail = [&application, &window](const QString& message) {
                    qCritical().noquote() << message;
                    if (auto* modal = QApplication::activeModalWidget()) modal->close();
                    window.hide();
                    application.exit(4);
                };
                if (!state->dialog
                    || state->dialog->objectName() != QStringLiteral("LanePropertiesDialog")
                    || !state->name
                    || !state->kind
                    || !state->width
                    || !state->signedValue
                    || !state->radix
                    || !state->enumMap
                    || !state->clock
                    || !state->group
                    || !state->color
                    || !state->height
                    || !state->visible
                    || !state->error
                    || !state->saveState
                    || !state->undoAction
                    || !state->ok
                    || !original
                    || state->saveState->text() != QStringLiteral("Saved")
                    || state->undoAction->isEnabled()) {
                    fail(QStringLiteral("Lane properties smoke did not start from a saved, editable dialog"));
                    return;
                }

                state->originalLane = *original;
                state->originalName = state->name->text();
                state->originalKindIndex = state->kind->currentIndex();
                state->originalWidth = state->width->text();
                state->originalSigned = state->signedValue->isChecked();
                state->originalRadixIndex = state->radix->currentIndex();
                state->originalEnumMap = state->enumMap->text();
                state->originalClockIndex = state->clock->currentIndex();
                state->originalGroupIndex = state->group->currentIndex();
                state->originalColor = state->color->text();
                state->originalHeight = state->height->value();
                state->originalVisible = state->visible->isChecked();

                const auto runner = std::make_shared<std::function<void()>>();
                *runner = [
                              &application,
                              &window,
                              laneDialogScreenshotPath,
                              state,
                              runner,
                              fail] {
                    const auto submitAndCheckNext = [&application, state, runner] {
                        QTimer::singleShot(0, &application, [runner] {
                            (*runner)();
                        });
                        state->ok->click();
                    };
                    const auto dialogStillOpen = [state] {
                        return QApplication::activeModalWidget() == state->dialog
                            && state->dialog->isVisible();
                    };
                    if (state->stage < 6 && !dialogStillOpen()) {
                        fail(QStringLiteral("Invalid lane properties closed the editor or opened a warning dialog"));
                        return;
                    }

                    if (state->stage == 0) {
                        if (!state->error->isVisible()
                            || !state->error->text().contains(
                                QStringLiteral("cannot be empty"),
                                Qt::CaseInsensitive)
                            || QApplication::focusWidget() != state->name
                            || state->kind->currentIndex() != state->originalKindIndex
                            || state->color->text() != state->originalColor) {
                            fail(QStringLiteral("Invalid lane name was not corrected inline with the draft retained"));
                            return;
                        }
                        state->name->setText(QStringLiteral("REQ"));
                        state->stage = 1;
                        submitAndCheckNext();
                        return;
                    }
                    if (state->stage == 1) {
                        if (!state->error->isVisible()
                            || !state->error->text().contains(
                                QStringLiteral("already uses this name"),
                                Qt::CaseInsensitive)
                            || QApplication::focusWidget() != state->name
                            || state->name->text() != QStringLiteral("REQ")
                            || state->kind->currentIndex() != state->originalKindIndex
                            || state->color->text() != state->originalColor) {
                            fail(QStringLiteral("Duplicate lane name was not rejected inline with the draft retained"));
                            return;
                        }
                        state->name->setText(state->originalName);
                        state->kind->setCurrentIndex(state->kind->findData(
                            static_cast<int>(wave::LaneKind::Bus)));
                        state->width->setText(QStringLiteral("0"));
                        state->stage = 2;
                        submitAndCheckNext();
                        return;
                    }
                    if (state->stage == 2) {
                        if (!state->error->isVisible()
                            || !state->error->text().contains(
                                QStringLiteral("Width must"),
                                Qt::CaseInsensitive)
                            || QApplication::focusWidget() != state->width
                            || state->name->text() != state->originalName) {
                            fail(QStringLiteral("Invalid lane width was not corrected inline with the draft retained"));
                            return;
                        }
                        state->kind->setCurrentIndex(state->kind->findData(
                            static_cast<int>(wave::LaneKind::Enum)));
                        state->width->setText(QStringLiteral("8"));
                        state->enumMap->setText(QStringLiteral("BROKEN"));
                        state->stage = 3;
                        submitAndCheckNext();
                        return;
                    }
                    if (state->stage == 3) {
                        if (!state->error->isVisible()
                            || !state->error->text().contains(
                                QStringLiteral("NAME=VALUE"),
                                Qt::CaseInsensitive)
                            || QApplication::focusWidget() != state->enumMap
                            || state->width->text() != QStringLiteral("8")) {
                            fail(QStringLiteral("Invalid Enum map was not corrected inline with the draft retained"));
                            return;
                        }
                        state->name->setText(state->originalName);
                        state->kind->setCurrentIndex(state->originalKindIndex);
                        state->width->setText(state->originalWidth);
                        state->signedValue->setChecked(state->originalSigned);
                        state->radix->setCurrentIndex(state->originalRadixIndex);
                        state->enumMap->setText(state->originalEnumMap);
                        state->clock->setCurrentIndex(state->originalClockIndex);
                        state->group->setCurrentIndex(state->originalGroupIndex);
                        state->color->setText(QStringLiteral("not-a-color"));
                        state->height->setValue(state->originalHeight);
                        state->visible->setChecked(state->originalVisible);
                        state->stage = 4;
                        submitAndCheckNext();
                        return;
                    }
                    if (state->stage == 4) {
                        if (!state->error->isVisible()
                            || !state->error->text().contains(
                                QStringLiteral("valid Qt color"),
                                Qt::CaseInsensitive)
                            || QApplication::focusWidget() != state->color
                            || state->name->text() != state->originalName
                            || state->kind->currentIndex() != state->originalKindIndex) {
                            fail(QStringLiteral("Invalid lane color was not corrected inline with the draft retained"));
                            return;
                        }
                        state->color->setText(state->originalColor);
                        state->width->setText(QStringLiteral("4"));
                        state->stage = 5;
                        submitAndCheckNext();
                        return;
                    }
                    if (state->stage == 5) {
                        if (!state->error->isVisible()
                            || !state->error->text().contains(
                                QStringLiteral("cannot be applied"),
                                Qt::CaseInsensitive)
                            || !state->error->text().contains(
                                QStringLiteral("exceeds the lane width"),
                                Qt::CaseInsensitive)
                            || QApplication::focusWidget() != state->width
                            || state->name->text() != state->originalName
                            || state->kind->currentIndex() != state->originalKindIndex
                            || state->color->text() != state->originalColor
                            || !state->dialog->grab().save(laneDialogScreenshotPath)) {
                            fail(QStringLiteral("Incompatible lane width was not rejected inline with the draft retained"));
                            return;
                        }
                        state->width->setText(state->originalWidth);
                        state->stage = 6;
                        submitAndCheckNext();
                        return;
                    }
                    const auto* lane = !window.project().scenarios.empty()
                        ? wave::findLane(
                              window.project().scenarios.front(),
                              "lane-data")
                        : nullptr;
                    if (QApplication::activeModalWidget()
                        || !lane
                        || *lane != state->originalLane
                        || state->saveState->text() != QStringLiteral("Saved")
                        || state->undoAction->isEnabled()
                        || !window.statusBar()->currentMessage().contains(
                            QStringLiteral("No properties changed for data[7:0]"))) {
                        fail(QStringLiteral("Unchanged lane properties created an edit or lacked clear feedback"));
                        return;
                    }
                    window.hide();
                    application.exit(0);
                };

                state->name->setText(QStringLiteral(" "));
                state->stage = 0;
                QTimer::singleShot(0, &application, [runner] {
                    (*runner)();
                });
                state->ok->click();
            });
    } else if (waveformOnlySmoke) {
        QTimer::singleShot(
            0,
            &application,
            [&application, &window, waveformOnlyScreenshotPath] {
                auto* canvas = window.findChild<wave::WaveCanvas*>();
                const auto docks = window.findChildren<QDockWidget*>();
                auto* modeToolbar = window.findChild<QToolBar*>(
                    QStringLiteral("ModeToolbar"));
                auto* waveformToolbar = window.findChild<QToolBar*>(
                    QStringLiteral("WaveformToolbar"));
                auto* busRadix = window.findChild<QComboBox*>(
                    QStringLiteral("BusEditRadixCombo"));
                auto* busRecent = window.findChild<QComboBox*>(
                    QStringLiteral("BusEditRecentValuesCombo"));
                auto* asyncTimingAction = window.findChild<QAction*>(
                    QStringLiteral("AsyncTimingAction"));
                if (!canvas
                    || window.centralWidget() != canvas
                    || !canvas->isVisible()
                    || !docks.isEmpty()
                    || modeToolbar
                    || !waveformToolbar
                    || !waveformToolbar->isVisible()
                    || !busRadix
                    || !busRecent
                    || busRadix->isVisible()
                    || busRecent->isVisible()
                    || !asyncTimingAction
                    || asyncTimingAction->isChecked()
                    || asyncTimingAction->text() != QStringLiteral("Sync")) {
                    qCritical().noquote()
                        << "Waveform-only layout contains legacy panels or snapping UI";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!waveformOnlyScreenshotPath.isEmpty()
                    && !window.grab().save(waveformOnlyScreenshotPath)) {
                    qCritical().noquote()
                        << "Cannot save waveform-only screenshot:"
                        << waveformOnlyScreenshotPath;
                    window.hide();
                    application.exit(3);
                    return;
                }
                window.hide();
                application.exit(0);
            });
    } else if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(350, &application, [&application, &window, screenshotPath] {
            if (!window.grab().save(screenshotPath)) {
                qCritical().noquote() << "Cannot save screenshot:" << screenshotPath;
                application.exit(3);
                return;
            }
            application.quit();
        });
    } else if (smokeTest) {
        QTimer::singleShot(250, &application, [&application, &window] {
            window.hide();
            application.exit(0);
        });
    }
    return application.exec();
}
