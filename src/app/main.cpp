#include "main_window.h"
#include "wave_canvas.h"

#include "wave/model.h"
#include "wave/integration.h"
#include "wave/project_io.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFont>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>

#include <algorithm>
#include <iterator>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        application.setFont(QFont(QStringLiteral("Segoe UI"), 9));
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
    QString conditionCompareScreenshotPath;
    QString cursorModeScreenshotPath;
    bool cursorModeSmoke = false;
    QString uriText;
    QString autosaveSmokePath;
    bool laneRemovalSmoke = false;
    bool laneReorderSmoke = false;
    bool canvasAddLaneSmoke = false;
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
        } else if (argument.startsWith(QStringLiteral("--condition-compare-screenshot="))) {
            conditionCompareScreenshotPath = argument.mid(
                QStringLiteral("--condition-compare-screenshot=").size());
        } else if (argument == QStringLiteral("--lane-removal-smoke")) {
            laneRemovalSmoke = true;
        } else if (argument == QStringLiteral("--lane-reorder-smoke")) {
            laneReorderSmoke = true;
        } else if (argument == QStringLiteral("--canvas-add-lane-smoke")) {
            canvasAddLaneSmoke = true;
        } else if (argument.startsWith(QStringLiteral("--cursor-mode-smoke="))) {
            cursorModeSmoke = true;
            cursorModeScreenshotPath = argument.mid(
                QStringLiteral("--cursor-mode-smoke=").size());
        } else if (argument == QStringLiteral("--cursor-mode-smoke")) {
            cursorModeSmoke = true;
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

    auto project = wave::makeDemonstrationProject();
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
    if (!conditionCompareScreenshotPath.isEmpty()) {
        if (project.scenarios.empty() || project.scenarios.front().relations.empty()) {
            qCritical().noquote() << "Condition compare smoke requires a relation";
            return 2;
        }
        project.scenarios.front().relations.front().condition = "reset_n == 0";
        compareMode = true;
    }
    if (!autosaveSmokePath.isEmpty()) {
        project.importedTraces.clear();
    }

    wave::MainWindow window(
        std::move(project),
        autosaveSmokePath.isEmpty() ? projectPath : autosaveSmokePath);
    window.show();
    if (!conditionCompareScreenshotPath.isEmpty()) {
        if (auto* relationOnly = window.findChild<QCheckBox*>(
                QStringLiteral("CompareRelationOnly"))) {
            relationOnly->setChecked(true);
        }
    }
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
        QTimer::singleShot(0, &window, [&application, &window] {
            auto* canvas = window.findChild<wave::WaveCanvas*>();
            auto* addButton = window.findChild<QToolButton*>(
                QStringLiteral("CanvasAddSignalButton"));
            if (!canvas || !addButton) {
                qCritical().noquote() << "Canvas add-signal control is missing";
                window.hide();
                application.exit(4);
                return;
            }

            canvas->verticalScrollBar()->setValue(
                canvas->verticalScrollBar()->maximum());
            if (!addButton->isVisible()
                || !addButton->geometry().intersects(canvas->viewport()->rect())
                || !addButton->text().startsWith(QLatin1Char('+'))) {
                qCritical().noquote()
                    << "Canvas add-signal control is not visible at the end of the lane list";
                window.hide();
                application.exit(4);
                return;
            }

            const auto originalLaneCount =
                window.project().scenarios.front().lanes.size();
            QTimer::singleShot(0, &application, [&application] {
                auto* dialog = qobject_cast<QDialog*>(
                    QApplication::activeModalWidget());
                if (!dialog) {
                    qCritical().noquote() << "Canvas add-signal dialog did not open";
                    application.exit(4);
                    return;
                }
                const auto edits = dialog->findChildren<QLineEdit*>();
                const auto name = std::find_if(
                    edits.begin(),
                    edits.end(),
                    [](const QLineEdit* edit) {
                        return !edit->isReadOnly()
                            && edit->text() == QStringLiteral("signal");
                    });
                auto* buttons = dialog->findChild<QDialogButtonBox*>();
                if (name == edits.end() || !buttons || !buttons->button(QDialogButtonBox::Ok)) {
                    qCritical().noquote() << "Canvas add-signal dialog fields are incomplete";
                    dialog->reject();
                    application.exit(4);
                    return;
                }
                (*name)->setText(QStringLiteral("canvas_signal"));
                buttons->button(QDialogButtonBox::Ok)->click();
            });
            addButton->click();

            const auto& lanes = window.project().scenarios.front().lanes;
            if (lanes.size() != originalLaneCount + 1
                || lanes.back().name != "canvas_signal"
                || lanes.back().kind != wave::LaneKind::Bit) {
                qCritical().noquote() << "Canvas add-signal action created the wrong lane";
                window.hide();
                application.exit(4);
                return;
            }
            if (!QMetaObject::invokeMethod(&window, "undo", Qt::DirectConnection)
                || window.project().scenarios.front().lanes.size() != originalLaneCount) {
                qCritical().noquote() << "Canvas add-signal undo failed";
                window.hide();
                application.exit(4);
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
                auto* cursorAction = window.findChild<QAction*>(
                    QStringLiteral("CursorToolAction"));
                if (!canvas || !cursorAction) {
                    qCritical().noquote() << "Cursor mode controls are missing";
                    window.hide();
                    application.exit(4);
                    return;
                }

                canvas->fitScenario();
                cursorAction->trigger();
                if (!cursorAction->isChecked()
                    || canvas->tool() != wave::WaveCanvas::Tool::Marker) {
                    qCritical().noquote() << "Cursor mode did not activate";
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

                cursorAction->trigger();
                if (cursorAction->isChecked()
                    || canvas->tool() != wave::WaveCanvas::Tool::Selection
                    || canvas->movableCursorTick()
                    || canvas->temporaryCursorTick()
                    || !canvas->selectedMarkerId().isEmpty()) {
                    qCritical().noquote() << "Second Cursor click did not exit editing mode";
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
    } else if (!conditionCompareScreenshotPath.isEmpty()) {
        QTimer::singleShot(
            1'500,
            &application,
            [&application, &window, conditionCompareScreenshotPath] {
                const auto* table = window.findChild<QTableWidget*>(
                    QStringLiteral("CompareResultTable"));
                const auto* summary = window.findChild<QLabel*>(
                    QStringLiteral("CompareSummary"));
                if (!table
                    || !summary
                    || table->rowCount() != 1
                    || !table->item(0, 0)
                    || table->item(0, 0)->text() != QObject::tr("diagnostic")
                    || !summary->text().contains(QObject::tr("1 diagnostics"))) {
                    qCritical().noquote()
                        << "Condition compare diagnostic was not rendered";
                    window.hide();
                    application.exit(4);
                    return;
                }
                if (!window.grab().save(conditionCompareScreenshotPath)) {
                    qCritical().noquote()
                        << "Cannot save condition compare screenshot:"
                        << conditionCompareScreenshotPath;
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
