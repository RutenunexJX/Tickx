#include "main_window.h"
#include "wave_canvas.h"

#include "wave/model.h"
#include "wave/integration.h"
#include "wave/project_io.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
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
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>

#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <iterator>
#include <limits>
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
        || canvasAddLaneSmoke
        || !screenshotPath.isEmpty()
        || !laneDialogScreenshotPath.isEmpty()
        || !editMenuScreenshotPath.isEmpty()
        || !autosaveSmokePath.isEmpty();
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
            if (!QFileInfo::exists(snapshotPath) || !loaded.ok()) {
                qCritical().noquote()
                    << "Autosave recovery snapshot is missing or invalid:"
                    << snapshotPath
                    << loaded.error;
                window.hide();
                application.exit(4);
                return;
            }
            window.hide();
            application.exit(0);
        });
    } else if (newProjectSmoke) {
        QTimer::singleShot(0, &window, [&application, &window] {
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
                qCritical().noquote() << "New project direct action or blank-state controls are missing";
                window.hide();
                application.exit(4);
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
                qCritical().noquote() << "Default startup did not expose the direct 200 ns blank waveform";
                window.hide();
                application.exit(4);
                return;
            }

            durationEdit->setFocus(Qt::OtherFocusReason);
            durationEdit->setText(QStringLiteral("300 ns"));
            durationEdit->setModified(true);
            QKeyEvent cancelDuration(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QCoreApplication::sendEvent(durationEdit, &cancelDuration);
            QCoreApplication::processEvents();
            if (!verifyBlank()) {
                qCritical().noquote() << "Escape did not cancel the direct timeline edit";
                window.hide();
                application.exit(4);
                return;
            }

            addClock->click();
            QCoreApplication::processEvents();
            auto* setupPanel = canvas->findChild<QWidget*>(
                QStringLiteral("QuickLaneSetupPanel"));
            auto* quickName = canvas->findChild<QLineEdit*>(
                QStringLiteral("QuickLaneNameEdit"));
            if (!setupPanel || !setupPanel->isVisible() || !quickName) {
                qCritical().noquote() << "Blank-state quick creation did not start inline";
                window.hide();
                application.exit(4);
                return;
            }
            QKeyEvent cancelQuick(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QCoreApplication::sendEvent(quickName, &cancelQuick);
            QCoreApplication::processEvents();
            if (setupPanel->isVisible() || !verifyBlank()) {
                qCritical().noquote() << "Escape did not atomically cancel the quick signal";
                window.hide();
                application.exit(4);
                return;
            }

            action->trigger();
            QCoreApplication::processEvents();
            if (QApplication::activeModalWidget()
                || window.findChild<QDialog*>(QStringLiteral("NewProjectDialog"))
                || !verifyBlank()) {
                qCritical().noquote() << "New unexpectedly opened configuration or changed defaults";
                if (auto* dialog = QApplication::activeModalWidget()) dialog->close();
                window.hide();
                application.exit(4);
                return;
            }
            window.hide();
            application.exit(0);
        });
    } else if (userJourneySmoke) {
        QFile::remove(userJourneySavePath);
        QFile::remove(userJourneySavePath + QStringLiteral(".autosave"));
        QTimer::singleShot(
            0,
            &window,
            [&application, &window, userJourneySavePath] {
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
                auto* palette = canvas->findChild<QWidget*>(QStringLiteral("BusPresetPalette"));
                auto* busValue = canvas->findChild<QLineEdit*>(
                    QStringLiteral("BusPresetValueEdit"));
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
                QTimer::singleShot(
                    0,
                    &window,
                    [&application, &saveDialogHandled, userJourneySavePath] {
                        auto* dialog = qobject_cast<QFileDialog*>(
                            QApplication::activeModalWidget());
                        if (!dialog) {
                            qCritical().noquote() << "Save As dialog did not open in user journey";
                            application.exit(4);
                            return;
                        }
                        dialog->setDirectory(QFileInfo(userJourneySavePath).absolutePath());
                        dialog->selectFile(QFileInfo(userJourneySavePath).fileName());
                        saveDialogHandled = true;
                        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                    });
                sendKey(durationEdit, Qt::Key_S, Qt::ControlModifier);
                QCoreApplication::processEvents();
                const auto saved = wave::loadProjectFile(userJourneySavePath);
                const auto* savedRenamedBit = saved.ok()
                    ? wave::findLane(saved.project->scenarios.front(), bitLaneId)
                    : nullptr;
                const auto* savedBus = saved.ok()
                    ? wave::findLane(saved.project->scenarios.front(), busLaneId)
                    : nullptr;
                if (!saveDialogHandled
                    || !QFileInfo::exists(userJourneySavePath)
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
            QTimer::singleShot(0, &application, [&application] {
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
            if (wave::findLane(removedScenario, "lane-request")
                || !relationRemoved) {
                QStringList remainingLanes;
                for (const auto& lane : removedScenario.lanes) {
                    remainingLanes.append(QString::fromStdString(lane.id));
                }
                QStringList remainingRelations;
                for (const auto& relation : removedScenario.relations) {
                    remainingRelations.append(QString::fromStdString(relation.id));
                }
                qCritical().noquote()
                    << "Lane removal did not clean dependent data; lanes:"
                    << remainingLanes.join(QLatin1Char(','))
                    << "relations:"
                    << remainingRelations.join(QLatin1Char(','));
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
            if (!wave::findLane(restoredScenario, "lane-request")
                || !relationRestored) {
                qCritical().noquote() << "Lane removal undo did not restore dependent data";
                window.hide();
                application.exit(4);
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
                if (movedUp.size() != originalLanes.size()
                    || movedUp.at(2).id != "lane-request") {
                    qCritical().noquote() << "Lane reorder up produced the wrong order";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!invoke("undo")
                    || window.project().scenarios.front().lanes != originalLanes) {
                    qCritical().noquote() << "Lane reorder up undo failed";
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
                if (movedDown.size() != originalLanes.size()
                    || movedDown.at(4).id != "lane-request") {
                    qCritical().noquote() << "Lane reorder down produced the wrong order";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!invoke("undo")
                    || window.project().scenarios.front().lanes != originalLanes) {
                    qCritical().noquote() << "Lane reorder down undo failed";
                    window.hide();
                    application.exit(4);
                    return;
                }
                window.hide();
                application.exit(0);
            });
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
            auto* measureAction = window.findChild<QAction*>(
                QStringLiteral("MeasureToolAction"));
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
                        || text.compare(QStringLiteral("Fit selection"), Qt::CaseInsensitive) == 0;
                });
            if (!waveformToolbar
                || !undoAction
                || !redoAction
                || !cutRangeAction
                || !measureAction
                || window.findChild<QAction*>(QStringLiteral("WaveEditToolAction"))
                || checkableModeCount != 1
                || measureAction->isChecked()
                || measureAction->shortcut().matches(QKeySequence(Qt::CTRL | Qt::Key_M))
                    != QKeySequence::ExactMatch
                || canvas->tool() != wave::WaveCanvas::Tool::WaveEdit
                || toolbarActions.contains(undoAction)
                || toolbarActions.contains(redoAction)
                || hasRemovedTool
                || undoAction->shortcut().matches(QKeySequence(QKeySequence::Undo))
                    != QKeySequence::ExactMatch
                || redoAction->shortcut().matches(QKeySequence(QKeySequence::Redo))
                    != QKeySequence::ExactMatch
                || cutRangeAction->shortcut().matches(QKeySequence(QKeySequence::Cut))
                    != QKeySequence::ExactMatch) {
                fail(QStringLiteral("Toolbar convergence or Undo/Redo/Cut menu shortcuts are incorrect"));
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
            auto* presetPalette = canvas->findChild<QWidget*>(
                QStringLiteral("BusPresetPalette"));
            const std::array<QToolButton*, 4> presetButtons{
                canvas->findChild<QToolButton*>(QStringLiteral("BusPresetZeroButton")),
                canvas->findChild<QToolButton*>(QStringLiteral("BusPresetXButton")),
                canvas->findChild<QToolButton*>(QStringLiteral("BusPresetZButton")),
                canvas->findChild<QToolButton*>(QStringLiteral("BusPresetDontCareButton")),
            };
            auto* directValue = canvas->findChild<QLineEdit*>(
                QStringLiteral("BusPresetValueEdit"));
            auto* contextLabel = canvas->findChild<QLabel*>(
                QStringLiteral("BusPresetContextLabel"));
            if (!presetPalette
                || !presetPalette->isVisible()
                || !directValue
                || !directValue->isVisible()
                || !contextLabel
                || !contextLabel->text().contains(QString::fromStdString(quickBus.name))
                || canvas->findChild<QToolButton*>(QStringLiteral("BusPresetCustomButton"))
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
            directValue->setText(QStringLiteral("0x2a"));
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
            if (!dragEnter.isAccepted()
                || !drop.isAccepted()
                || !busAfterPreset
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
                fail(QStringLiteral("Bus quick-value drop did not create one persisted beat"));
                return;
            }
            const auto presetStart = presetSegment->start;
            undoAction->trigger();
            QCoreApplication::processEvents();
            busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!busAfterPreset || !busAfterPreset->segments.empty()) {
                fail(QStringLiteral("Bus quick-value drop undo failed"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();
            busAfterPreset = wave::findLane(
                window.project().scenarios.front(), quickBus.id);
            if (!busAfterPreset || busAfterPreset->segments.empty()) {
                fail(QStringLiteral("Bus quick-value drop redo failed"));
                return;
            }

            const auto edgeX = 190 + static_cast<int>(std::llround(
                static_cast<double>(presetStart)
                / static_cast<double>(window.project().scenarios.front().duration)
                * static_cast<double>(canvas->viewport()->width() - 190)))
                - canvas->horizontalScrollBar()->value();
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
                fail(QStringLiteral("Soft snapping did not capture a nearby signal edge"));
                return;
            }
            sendMouse(
                QEvent::MouseMove,
                QPoint(edgeX + 24, presetBusY),
                Qt::NoButton,
                Qt::NoButton);
            QCoreApplication::processEvents();
            if (canvas->cursorTick() == presetStart) {
                fail(QStringLiteral("Soft snapping remained active outside its pixel radius"));
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
                fail(QStringLiteral("Soft snapping did not capture a nearby ruler tick"));
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
            renameEdit->setText(QStringLiteral("cancelled_bit"));
            sendKey(renameEdit, Qt::Key_Escape);
            QCoreApplication::processEvents();
            auto* renamedBit = wave::findLane(
                window.project().scenarios.front(), quickBit.id);
            if (renameEdit->isVisible()
                || !renamedBit
                || renamedBit->name != quickBit.name) {
                fail(QStringLiteral("Escape did not cancel inline signal rename"));
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
                            dialogHandled = true;
                            QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
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
            if (!editedBus || editedBus->width != 16) {
                fail(QStringLiteral("Bus right-click parameters were not committed"));
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
            if (!renamedBit || renamedBit->height != 64) {
                fail(QStringLiteral("Bit right-click parameters were not committed"));
                return;
            }

            if (!editFromContext(quickClock.id, [](QDialog* dialog) {
                    auto* mode = dialog->findChild<QComboBox*>(
                        QStringLiteral("ClockRateMode"));
                    auto* value = dialog->findChild<QLineEdit*>(
                        QStringLiteral("ClockRateValue"));
                    if (dialog->objectName() != QStringLiteral("QuickClockParametersDialog")
                        || !mode
                        || !value) {
                        return false;
                    }
                    mode->setCurrentIndex(mode->findData(QStringLiteral("period")));
                    value->setText(QStringLiteral("12000 ticks"));
                    return true;
                })) {
                fail(QStringLiteral("Clock right-click frequency/period editor did not open"));
                return;
            }
            const auto* editedClock = wave::findClock(
                window.project(), quickClock.clockDomainId);
            if (!editedClock || editedClock->period != 12'000) {
                fail(QStringLiteral("Clock right-click period was not committed"));
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
            if (clockPosition == orderBeforeDrag.end()) {
                fail(QStringLiteral("Quick clock disappeared before lane drag"));
                return;
            }
            const auto expectedDrop = static_cast<std::size_t>(
                std::distance(orderBeforeDrag.begin(), clockPosition));
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
            if (orderAfterDrag.at(expectedDrop) != quickBus.id) {
                fail(QStringLiteral("Header drag committed the wrong lane order"));
                return;
            }
            undoAction->trigger();
            QCoreApplication::processEvents();
            if (laneOrder() != orderBeforeDrag) {
                fail(QStringLiteral("Header drag was not one undoable MoveLaneCommand"));
                return;
            }
            redoAction->trigger();
            QCoreApplication::processEvents();
            if (laneOrder() != orderAfterDrag) {
                fail(QStringLiteral("Header drag redo failed"));
                return;
            }

            auto* transactionPanel = canvas->findChild<QWidget*>(
                QStringLiteral("QuickLaneSetupPanel"));
            auto* transactionName = canvas->findChild<QLineEdit*>(
                QStringLiteral("QuickLaneNameEdit"));
            auto* transactionError = canvas->findChild<QLabel*>(
                QStringLiteral("QuickLaneSetupError"));
            auto* transactionBusPalette = canvas->findChild<QWidget*>(
                QStringLiteral("BusPresetPalette"));
            auto* transactionBusPreset = canvas->findChild<QToolButton*>(
                QStringLiteral("BusPresetXButton"));
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

            canvas->revealLocation(QString::fromStdString(quickBit.id), 0);
            QCoreApplication::processEvents();
            bitY = laneCenter(quickBit.id);
            clickHeader(QPoint(80, bitY));
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

                canvas->fitScenario();
                measureAction->trigger();
                if (!measureAction->isChecked()
                    || canvas->tool() != wave::WaveCanvas::Tool::Marker) {
                    qCritical().noquote() << "Measure mode did not activate";
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

                click(point1);
                if (!canvas->movableCursorTick()
                    || canvas->temporaryCursorTick()) {
                    qCritical().noquote() << "Left click did not create one movable cursor";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto beforeArrow = *canvas->movableCursorTick();
                sendKey(Qt::Key_Right);
                if (!canvas->movableCursorTick()
                    || *canvas->movableCursorTick() <= beforeArrow) {
                    qCritical().noquote() << "Right arrow did not move the cursor";
                    window.hide();
                    application.exit(4);
                    return;
                }

                const auto movableBeforeShift = *canvas->movableCursorTick();
                click(point4, Qt::ShiftModifier);
                if (!canvas->temporaryCursorTick()
                    || *canvas->movableCursorTick() != movableBeforeShift
                    || *canvas->temporaryCursorTick() <= *canvas->movableCursorTick()) {
                    qCritical().noquote() << "Shift click did not create a temporary cursor";
                    window.hide();
                    application.exit(4);
                    return;
                }

                drag(point4, point3);
                if (!canvas->movableCursorTick()
                    || !canvas->temporaryCursorTick()
                    || *canvas->movableCursorTick() >= *canvas->temporaryCursorTick()) {
                    qCritical().noquote() << "Direct drag did not create a signed cursor measurement";
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
                if (!locked || locked->start <= lockedBeforeDrag) {
                    qCritical().noquote() << "Dragging a selected locked cursor did not move it";
                    window.hide();
                    application.exit(4);
                    return;
                }
                const auto lockedBeforeArrow = locked->start;
                sendKey(Qt::Key_Left);
                locked = markerById();
                if (!locked || locked->start >= lockedBeforeArrow) {
                    qCritical().noquote() << "Left arrow did not move the selected locked cursor";
                    window.hide();
                    application.exit(4);
                    return;
                }
                sendKey(Qt::Key_Delete);
                if (window.project().scenarios.front().markers.size()
                    != originalMarkerCount) {
                    qCritical().noquote() << "Delete did not remove the selected locked cursor";
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

                const auto dataY = laneCenterY("lane-data");
                const auto resetY = laneCenterY("lane-reset");
                const auto requestY = laneCenterY("lane-request");
                const auto acknowledgeY = laneCenterY("lane-ack");
                if (dataY < 0 || resetY < 0 || requestY < 0 || acknowledgeY < 0) {
                    qCritical().noquote() << "Wave Edit smoke lanes are missing";
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

                bool valueDialogHandled = false;
                QTimer::singleShot(0, &window, [&valueDialogHandled] {
                    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    auto* edit = dialog ? dialog->findChild<QLineEdit*>() : nullptr;
                    if (!dialog || !edit) return;
                    edit->setText(QStringLiteral("0x2a"));
                    valueDialogHandled = true;
                    dialog->accept();
                });
                sendMouse(
                    QEvent::MouseButtonDblClick,
                    QPoint(xAtTick(payloadEditTick), dataY),
                    Qt::LeftButton,
                    Qt::LeftButton);
                QCoreApplication::processEvents();
                dataLane = wave::findLane(scenario, "lane-data");
                if (!valueDialogHandled || !dataLane
                    || valueAt(*dataLane, payloadEditTick) != "0x2a") {
                    qCritical().noquote() << "Double click did not edit the existing segment value";
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

                click(QPoint(xAtTick(65'000), requestY));
                requestLane = wave::findLane(scenario, "lane-request");
                const auto secondClickRange = canvas->selectedTimeRange();
                if (!requestLane
                    || valueAt(*requestLane, 55'000) != "0"
                    || valueAt(*requestLane, 65'000) != "0"
                    || valueAt(*requestLane, 75'000) != "0"
                    || !canvas->selectedSegmentId().isEmpty()
                    || !secondClickRange
                    || *secondClickRange
                        != std::pair<wave::Tick, wave::Tick>{60'000, 70'000}) {
                    qCritical().noquote()
                        << "Repeated click selected a merged bit segment instead of one beat";
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
                        QStringLiteral("Ctrl+Z"))) {
                    qCritical().noquote()
                        << "Bit context Clear did not clear exactly one selected beat";
                    window.hide();
                    application.exit(4);
                    return;
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
                    || scenario != beforeRangeBoundaryAdjustment) {
                    qCritical().noquote()
                        << "Re-selecting the original range after handle adjustment failed";
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
                const std::array<QWidget*, 9> visibleRangeControls{
                    rangeContext,
                    rangeCopyButton,
                    rangeCutButton,
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
        QTimer::singleShot(0, &window, [&window] {
            window.openLanePropertiesPreview(QStringLiteral("lane-request"));
        });
        QTimer::singleShot(
            350,
            &application,
            [&application, &window, laneDialogScreenshotPath] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if (!dialog || !dialog->grab().save(laneDialogScreenshotPath)) {
                    qCritical().noquote()
                        << "Cannot capture lane properties dialog:"
                        << laneDialogScreenshotPath;
                    if (dialog) dialog->reject();
                    window.hide();
                    application.exit(3);
                    return;
                }
                dialog->reject();
                window.hide();
                application.exit(0);
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
                auto* snapControl = waveformToolbar
                    ? waveformToolbar->findChild<QComboBox*>()
                    : nullptr;
                if (!canvas
                    || window.centralWidget() != canvas
                    || !canvas->isVisible()
                    || !docks.isEmpty()
                    || modeToolbar
                    || !waveformToolbar
                    || !waveformToolbar->isVisible()
                    || snapControl) {
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
