#include "main_window.h"
#include "wave_canvas.h"

#include "wave/model.h"
#include "wave/project_io.h"

#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QLineEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollBar>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>

namespace {

bool waitUntil(const std::function<bool()>& predicate, const int timeoutMs = 6'000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QTest::qWait(20);
    }
    return predicate();
}

bool inspectDirectoryChange(
    wave::MainWindow& window,
    QFileSystemWatcher& watcher,
    const QString& projectPath)
{
    const auto generation = window.property(
        "wavewidgets.projectWatchGeneration").toULongLong();
    return QMetaObject::invokeMethod(
               &watcher,
               "directoryChanged",
               Qt::DirectConnection,
               Q_ARG(QString, QFileInfo(projectPath).absolutePath()))
        && waitUntil([&window, generation] {
               return window.property("wavewidgets.projectWatchGeneration")
                          .toULongLong()
                   > generation;
           });
}

wave::Project initialProject()
{
    auto project = wave::makeDemonstrationProject();
    project.id = "project-external-reload";
    project.name = "External reload baseline";

    wave::Scenario scenario;
    scenario.id = "scenario-cli";
    scenario.name = "CLI update target";
    scenario.duration = 500'000;
    for (int index = 0; index < 14; ++index) {
        wave::Lane lane;
        lane.id = "lane-cli-" + std::to_string(index);
        lane.name = "cli_signal_" + std::to_string(index);
        lane.kind = wave::LaneKind::Bit;
        lane.color = index % 2 == 0 ? "#64b5f6" : "#81c784";
        wave::setSegmentRange(
            lane,
            0,
            150'000 + index * 2'000,
            "0",
            "segment-cli-low-" + std::to_string(index));
        wave::setSegmentRange(
            lane,
            150'000 + index * 2'000,
            scenario.duration,
            "1",
            "segment-cli-high-" + std::to_string(index));
        scenario.lanes.push_back(std::move(lane));
    }
    wave::Event sourceEvent;
    sourceEvent.id = "event-cli-source";
    sourceEvent.laneId = "lane-cli-0";
    sourceEvent.tick = 100'000;
    sourceEvent.action = wave::EventAction::Drive;
    sourceEvent.value = "0";
    sourceEvent.description = "CLI source";
    scenario.events.push_back(sourceEvent);
    wave::Event targetEvent;
    targetEvent.id = "event-cli-target";
    targetEvent.laneId = "lane-cli-1";
    targetEvent.tick = 200'000;
    targetEvent.action = wave::EventAction::Expect;
    targetEvent.value = "1";
    targetEvent.description = "CLI target";
    scenario.events.push_back(targetEvent);
    wave::Relation relation;
    relation.id = "relation-cli";
    relation.sourceEventId = sourceEvent.id;
    relation.targetEventId = targetEvent.id;
    relation.minimumDelay = 0;
    relation.maximumDelay = 200'000;
    relation.description = "CLI external relation";
    scenario.relations.push_back(std::move(relation));
    wave::Marker marker;
    marker.id = "marker-cli";
    marker.name = "CLI checkpoint";
    marker.start = 175'000;
    marker.end = 175'000;
    scenario.markers.push_back(std::move(marker));
    project.scenarios.push_back(std::move(scenario));
    return project;
}

wave::Scenario* cliScenario(wave::Project& project)
{
    const auto found = std::find_if(
        project.scenarios.begin(),
        project.scenarios.end(),
        [](const wave::Scenario& scenario) {
            return scenario.id == "scenario-cli";
        });
    return found == project.scenarios.end() ? nullptr : &*found;
}

wave::Project externalVersion(
    const wave::Project& base,
    const std::string& name,
    const int generation,
    const bool addLane)
{
    auto project = base;
    project.name = name;
    auto* scenario = cliScenario(project);
    if (!scenario || scenario->lanes.empty()) return project;

    auto& changed = scenario->lanes.at(
        static_cast<std::size_t>(generation) % scenario->lanes.size());
    changed.color = generation % 2 == 0 ? "#ff8a65" : "#ba68c8";
    changed.name = "cli_changed_" + std::to_string(generation);
    if (addLane) {
        wave::Lane lane;
        lane.id = "lane-external-" + std::to_string(generation);
        lane.name = "external_signal_" + std::to_string(generation);
        lane.kind = wave::LaneKind::Bit;
        lane.color = "#ffd54f";
        wave::setSegmentRange(
            lane,
            0,
            scenario->duration,
            generation % 2 == 0 ? "0" : "1",
            "segment-external-" + std::to_string(generation));
        scenario->lanes.push_back(std::move(lane));
    }
    return project;
}

bool saveProject(
    const wave::Project& project,
    const QString& path,
    const char* context)
{
    QString error;
    if (wave::saveProjectFileAtomic(project, path, &error)) return true;
    std::cerr << context << ": " << error.toStdString() << '\n';
    return false;
}

bool replaceWithInvalidProject(const QString& path)
{
    QSaveFile file(path);
    const QByteArray invalid = QByteArrayLiteral("{ invalid wave project");
    return file.open(QIODevice::WriteOnly)
        && file.write(invalid) == invalid.size()
        && file.commit();
}

int fail(const int code, const char* message)
{
    std::cerr << "external reload regression failed: " << message << '\n';
    return code;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return fail(2, "temporary directory unavailable");

    const QString projectPath = directory.filePath(
        QStringLiteral("external-reload.wave.json"));
    const auto baseline = initialProject();
    if (!saveProject(baseline, projectPath, "initial save")) return 3;
    auto loaded = wave::loadProjectFile(projectPath);
    if (!loaded.ok()) return fail(4, "initial project did not reload");

    wave::MainWindow window(std::move(*loaded.project), projectPath);
    window.resize(900, 600);
    window.show();
    window.activateWindow();
    QTest::qWait(100);

    auto* canvas = window.findChild<wave::WaveCanvas*>(
        QStringLiteral("StimulusCanvas"));
    auto* selector = window.findChild<QComboBox*>(
        QStringLiteral("WaveformSelector"));
    auto* watcher = window.findChild<QFileSystemWatcher*>(
        QStringLiteral("ProjectFileWatcher"));
    auto* conflictBar = window.findChild<QFrame*>(
        QStringLiteral("ExternalProjectConflictBar"));
    auto* reloadButton = window.findChild<QPushButton*>(
        QStringLiteral("ExternalProjectReloadButton"));
    auto* keepButton = window.findChild<QPushButton*>(
        QStringLiteral("ExternalProjectKeepButton"));
    if (!canvas || !selector || !watcher || !conflictBar || !reloadButton
        || !keepButton) {
        return fail(5, "external reload UI was not constructed");
    }
    if (!watcher->files().contains(QFileInfo(projectPath).absoluteFilePath())
        || !watcher->directories().contains(QFileInfo(projectPath).absolutePath())) {
        return fail(6, "project file and parent directory are not both watched");
    }

    selector->setCurrentIndex(1);
    QCoreApplication::processEvents();
    if (selector->currentIndex() != 1) {
        return fail(7, "secondary scenario was not selected");
    }
    const QString selectedLane = QStringLiteral("lane-cli-5");
    canvas->selectLaneHeaders({selectedLane}, selectedLane);
    canvas->goToTick(175'000);
    if (!canvas->restoreVisibleTimeSpan(90'000, 175'000)) {
        return fail(8, "test zoom could not be established");
    }
    canvas->horizontalScrollBar()->setValue(
        std::min(120, canvas->horizontalScrollBar()->maximum()));
    canvas->verticalScrollBar()->setValue(
        std::min(160, canvas->verticalScrollBar()->maximum()));
    const QStringList selectedMarkers{QStringLiteral("marker-cli")};
    const QStringList selectedRelations{QStringLiteral("relation-cli")};
    canvas->restoreStableObjectSelections(
        selectedMarkers,
        selectedRelations);
    QCoreApplication::processEvents();

    const auto cursorBefore = canvas->cursorTick();
    const auto spanBefore = canvas->visibleTimeSpan();
    const int horizontalBefore = canvas->horizontalScrollBar()->value();
    const int verticalBefore = canvas->verticalScrollBar()->value();
    if (canvas->selectedLaneId() != selectedLane
        || cursorBefore != 175'000
        || spanBefore <= 0
        || horizontalBefore <= 0
        || verticalBefore <= 0) {
        return fail(9, "selection, cursor, zoom, or scroll fixture is incomplete");
    }

    const auto initialParseCount = window.property(
        "wavewidgets.externalProjectParseCount").toULongLong();
    const auto cleanUpdate = externalVersion(
        window.project(), "External clean update", 5, true);
    if (!saveProject(cleanUpdate, projectPath, "clean external update")) {
        return 10;
    }
    if (!waitUntil([&window] {
            return window.property("wavewidgets.externalReloadCount")
                       .toULongLong()
                == 1;
        })) {
        return fail(11, "clean atomic replacement did not auto-reload");
    }
    QCoreApplication::processEvents();
    if (window.project().name != "External clean update"
        || selector->currentIndex() != 1
        || canvas->selectedLaneId() != selectedLane
        || canvas->cursorTick() != cursorBefore
        || canvas->visibleTimeSpan() != spanBefore
        || canvas->horizontalScrollBar()->value() != horizontalBefore
        || canvas->verticalScrollBar()->value() != verticalBefore
        || canvas->selectedMarkerIds() != selectedMarkers
        || canvas->selectedRelationIds() != selectedRelations) {
        return fail(12, "clean reload did not preserve GUI context");
    }
    const QString cleanSummary = window.property(
        "wavewidgets.lastExternalUpdateSummary").toString();
    const QStringList highlighted = canvas->property(
        "wavewidgets.externalUpdateLaneIds").toStringList();
    if (!cleanSummary.contains(QStringLiteral("External/CLI update"))
        || !cleanSummary.contains(QStringLiteral("Lane +1 ~1 -0"))
        || !highlighted.contains(selectedLane)
        || !highlighted.contains(QStringLiteral("lane-external-5"))) {
        return fail(13, "external update summary or lane highlight is incomplete");
    }
    const auto cleanParseCount = window.property(
        "wavewidgets.externalProjectParseCount").toULongLong();
    if (cleanParseCount != initialParseCount + 1) {
        return fail(40, "a new external version was not parsed exactly once");
    }

    const qulonglong cleanReloadCount = window.property(
        "wavewidgets.externalReloadCount").toULongLong();
    if (!inspectDirectoryChange(window, *watcher, projectPath)
        || window.property("wavewidgets.externalProjectParseCount")
               .toULongLong()
            != cleanParseCount
        || window.property("wavewidgets.externalReloadCount").toULongLong()
            != cleanReloadCount) {
        return fail(41, "an unchanged parent-directory notification reparsed the project");
    }
    if (!saveProject(window.project(), projectPath, "same-content save")) {
        return 14;
    }
    if (!inspectDirectoryChange(window, *watcher, projectPath)
        || window.property("wavewidgets.externalProjectParseCount")
               .toULongLong()
            != cleanParseCount
        || window.property("wavewidgets.externalReloadCount").toULongLong()
            != cleanReloadCount
        || window.property("wavewidgets.externalConflictState").toString()
            != QStringLiteral("none")) {
        return fail(15, "same-content watcher event reparsed or reloaded the project");
    }
    if (!QMetaObject::invokeMethod(
            &window, "saveProject", Qt::DirectConnection)) {
        return fail(16, "self-save action was not invokable");
    }
    if (!inspectDirectoryChange(window, *watcher, projectPath)
        || window.property("wavewidgets.externalProjectParseCount")
               .toULongLong()
            != cleanParseCount
        || window.property("wavewidgets.externalReloadCount").toULongLong()
            != cleanReloadCount) {
        return fail(17, "self-save reparsed or reloaded its own project");
    }

    if (!QMetaObject::invokeMethod(
            &window, "markEdited", Qt::DirectConnection)) {
        return fail(18, "dirty-state action was not invokable");
    }
    const auto dirtyUpdate = externalVersion(
        window.project(), "External while dirty", 6, false);
    if (!saveProject(dirtyUpdate, projectPath, "dirty external update")) {
        return 19;
    }
    if (!waitUntil([&window] {
            return window.property("wavewidgets.externalConflictState")
                       .toString()
                == QStringLiteral("dirty");
        })) {
        return fail(20, "dirty GUI state was silently overwritten");
    }
    if (window.project().name == "External while dirty"
        || !conflictBar->isVisibleTo(&window)) {
        return fail(21, "dirty conflict did not retain the local project");
    }
    reloadButton->click();
    if (!waitUntil([&window] {
            return window.project().name == "External while dirty"
                && window.property("wavewidgets.externalReloadCount")
                       .toULongLong()
                    == 2;
        })) {
        return fail(22, "explicit dirty reload did not apply disk state");
    }

    auto* inlineEditor = new QLineEdit(&window);
    inlineEditor->setObjectName(QStringLiteral("ExternalReloadTestEditor"));
    inlineEditor->setGeometry(20, 40, 260, 28);
    inlineEditor->setText(QStringLiteral("uncommitted inline value"));
    inlineEditor->setModified(true);
    inlineEditor->show();
    inlineEditor->setFocus(Qt::OtherFocusReason);
    QCoreApplication::processEvents();
    const auto editorUpdate = externalVersion(
        window.project(), "External during editor", 7, false);
    if (!saveProject(editorUpdate, projectPath, "editor external update")) {
        return 23;
    }
    if (!waitUntil([&window] {
            return window.property("wavewidgets.externalConflictState")
                       .toString()
                == QStringLiteral("editing");
        })) {
        return fail(24, "active inline editor was silently overwritten");
    }
    if (window.project().name == "External during editor"
        || inlineEditor->text() != QStringLiteral("uncommitted inline value")
        || !inlineEditor->hasFocus()) {
        return fail(25, "inline editor content or focus was not retained");
    }
    inlineEditor->hide();
    inlineEditor->deleteLater();
    canvas->setFocus(Qt::OtherFocusReason);
    if (!waitUntil([&window] {
            return window.project().name == "External during editor"
                && window.property("wavewidgets.externalReloadCount")
                       .toULongLong()
                    == 3;
        })) {
        return fail(26, "deferred editor update did not apply after editing ended");
    }

    const auto parseCountBeforeInvalid = window.property(
        "wavewidgets.externalProjectParseCount").toULongLong();
    if (!replaceWithInvalidProject(projectPath)) {
        return fail(27, "invalid atomic replacement could not be written");
    }
    if (!waitUntil([&window] {
            return window.property("wavewidgets.externalConflictState")
                       .toString()
                == QStringLiteral("invalid");
        })) {
        return fail(28, "invalid external project was not quarantined");
    }
    if (window.project().name != "External during editor") {
        return fail(29, "invalid external project replaced the valid GUI state");
    }
    const auto invalidParseCount = window.property(
        "wavewidgets.externalProjectParseCount").toULongLong();
    if (invalidParseCount <= parseCountBeforeInvalid) {
        return fail(42, "a new invalid external version bypassed parsing");
    }
    keepButton->click();
    if (!inspectDirectoryChange(window, *watcher, projectPath)
        || !inspectDirectoryChange(window, *watcher, projectPath)
        || window.property("wavewidgets.externalProjectParseCount")
               .toULongLong()
            != invalidParseCount
        || window.project().name != "External during editor"
        || window.property("wavewidgets.externalReloadCount").toULongLong() != 3
        || window.property("wavewidgets.externalConflictState").toString()
            != QStringLiteral("none")
        || conflictBar->isVisibleTo(&window)) {
        return fail(43, "the ignored invalid version was reparsed or reopened a conflict");
    }
    const auto recoveredUpdate = externalVersion(
        window.project(), "Recovered valid update", 8, false);
    if (!saveProject(recoveredUpdate, projectPath, "valid recovery update")) {
        return 30;
    }
    if (!waitUntil([&window] {
            return window.project().name == "Recovered valid update"
                && window.property("wavewidgets.externalReloadCount")
                       .toULongLong()
                    == 4;
        })) {
        return fail(31, "valid replacement did not recover from invalid input");
    }
    if (window.property("wavewidgets.externalProjectParseCount").toULongLong()
        <= invalidParseCount) {
        return fail(44, "a new version after Keep was incorrectly skipped");
    }

    const auto rapidA = externalVersion(
        window.project(), "Rapid update A", 9, false);
    const auto rapidB = externalVersion(
        rapidA, "Rapid update B", 10, false);
    if (!saveProject(rapidA, projectPath, "rapid update A")
        || !saveProject(rapidB, projectPath, "rapid update B")) {
        return 32;
    }
    if (!waitUntil([&window] {
            return window.project().name == "Rapid update B";
        })) {
        return fail(33, "newest consecutive replacement did not win");
    }

    const auto abaA = externalVersion(
        window.project(), "ABA final A", 11, false);
    const auto abaB = externalVersion(
        abaA, "ABA transient B", 12, false);
    if (!saveProject(abaA, projectPath, "ABA first A")
        || !saveProject(abaB, projectPath, "ABA B")
        || !saveProject(abaA, projectPath, "ABA final A")) {
        return 34;
    }
    if (!waitUntil([&window] {
            return window.project().name == "ABA final A";
        })) {
        return fail(35, "ABA replacement applied a stale intermediate version");
    }
    const auto stableCount = window.property(
        "wavewidgets.externalReloadCount").toULongLong();
    QTest::qWait(700);
    if (window.project().name != "ABA final A"
        || window.property("wavewidgets.externalReloadCount").toULongLong()
            != stableCount
        || window.property("wavewidgets.externalConflictState").toString()
            != QStringLiteral("none")) {
        return fail(36, "stale watcher event changed the settled ABA result");
    }

    auto removedScenarioUpdate = window.project();
    removedScenarioUpdate.name = "Scenario removed externally";
    std::erase_if(
        removedScenarioUpdate.scenarios,
        [](const wave::Scenario& scenario) {
            return scenario.id == "scenario-cli";
        });
    if (!saveProject(
            removedScenarioUpdate,
            projectPath,
            "external Scenario removal")) {
        return 37;
    }
    if (!waitUntil([&window, stableCount] {
            return window.project().name == "Scenario removed externally"
                && window.property("wavewidgets.externalReloadCount")
                       .toULongLong()
                    > stableCount;
        })) {
        return fail(38, "external Scenario removal did not reload");
    }
    const auto removalSummary = window.property(
        "wavewidgets.lastExternalUpdateSummary").toString();
    if (window.project().scenarios.size() != 1
        || selector->count() != 1
        || selector->currentIndex() != 0
        || window.project().scenarios.front().id == "scenario-cli"
        || !removalSummary.contains(QStringLiteral("scenario ID scenario-cli was removed"))
        || !removalSummary.contains(QStringLiteral("nearest-index fallback"))
        || !canvas->selectedMarkerIds().isEmpty()
        || !canvas->selectedRelationIds().isEmpty()) {
        return fail(39, "external Scenario removal did not report deterministic fallback");
    }

    window.close();
    QCoreApplication::processEvents();
    return 0;
}
