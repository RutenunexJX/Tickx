#include "main_window.h"
#include "trace_canvas.h"
#include "wave/commands.h"
#include "wave/project_io.h"
#include "wave/trace.h"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <limits>
#include <sstream>

namespace {
wave::Project fixture()
{
    wave::Project project;
    project.id = "trace-session";
    project.name = "Untitled";
    project.timeBase = {1'000};
    wave::Scenario scenario;
    scenario.id = "scenario";
    scenario.name = "Scenario";
    scenario.duration = 100;
    project.scenarios.push_back(scenario);
    return project;
}

const QByteArray vcd =
    "$timescale 1 ns $end\n$var wire 1 ! a $end\n$enddefinitions $end\n"
    "#0 0! #20 1! #60 0! #100\n";

bool write(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

QString resolve(const QString& projectFile, const std::string& stored)
{
    return QDir::cleanPath(QFileInfo(projectFile).absoluteDir()
        .absoluteFilePath(QString::fromStdString(stored)));
}

QJsonObject command(wave::MainWindow& window, const QString& name, QJsonObject args = {})
{
    args["expectedModelSha256"] = window.automationSessionState().value("modelSha256");
    return window.executeSessionCommand(name, args);
}

QJsonObject save(wave::MainWindow& window, const QString& path)
{
    return command(window, "save", {{"projectPath", path}, {"expectedDiskSha256", "missing"}});
}

QJsonObject renameScenario(wave::MainWindow& window)
{
    return command(window, "apply", {{"batch", QJsonObject{
        {"schema", "wave-workbench.operations/v1"},
        {"operations", QJsonArray{QJsonObject{{"op", "rename-scenario"}, {"name", "Edited"}}}}
    }}});
}

qint64 probe(wave::TraceCanvas& canvas, int x)
{
    QSignalSpy cursor(&canvas, &wave::TraceCanvas::cursorChanged);
    const QPointF point(x, 56);
    QMouseEvent event(QEvent::MouseMove, point, canvas.viewport()->mapToGlobal(point.toPoint()),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(), &event);
    return cursor.empty() ? std::numeric_limits<qint64>::min() : cursor.back()[0].toLongLong();
}

void capture(QWidget& widget, const QString& name)
{
    const auto directory = qEnvironmentVariable("WAVE_TRACE_SESSION_EVIDENCE_DIR");
    if (directory.isEmpty()) return;
    QDir().mkpath(directory);
    widget.grab().save(QDir(directory).filePath(name + ".png"));
}
}

class TraceSessionRegressionTest final : public QObject {
    Q_OBJECT
private slots:
    void longVcdTimes_data()
    {
        QTest::addColumn<QString>("scale");
        QTest::addColumn<QString>("raw");
        QTest::addColumn<qint64>("ps");
        QTest::addColumn<qint64>("offset");
        QTest::addColumn<qint64>("expected");
        QTest::addColumn<QString>("error");
        QTest::newRow("six-hours-seconds") << "1 s" << "21600" << qint64(1000) << qint64(0)
            << qint64(21'600'000'000'000) << "";
        QTest::newRow("six-hours-ns") << "1 ns" << "21600000000000" << qint64(1000) << qint64(-7)
            << qint64(21'599'999'999'993) << "";
        QTest::newRow("ten-ms") << "10 ms" << "2160000" << qint64(1) << qint64(0)
            << qint64(21'600'000'000'000'000) << "";
        QTest::newRow("hundred-us") << "100 us" << "216000000" << qint64(100) << qint64(0)
            << qint64(216'000'000'000'000) << "";
        QTest::newRow("uint64-raw") << "1 ps" << "18446744073709551614" << qint64(2) << qint64(0)
            << std::numeric_limits<qint64>::max() << "";
        QTest::newRow("int64-max") << "1 ns" << "9223372036854775807" << qint64(1000) << qint64(0)
            << std::numeric_limits<qint64>::max() << "";
        QTest::newRow("int64-min-offset") << "1 ps" << "0" << qint64(1)
            << std::numeric_limits<qint64>::min() << std::numeric_limits<qint64>::min() << "";
        QTest::newRow("huge-project-tick") << "1 ps" << "9223372036854775807"
            << std::numeric_limits<qint64>::max() << qint64(0) << qint64(1) << "";
        QTest::newRow("huge-project-zero") << "1 fs" << "0"
            << std::numeric_limits<qint64>::max() << qint64(0) << qint64(0) << "";
        QTest::newRow("fractional-fs") << "100 fs" << "30" << qint64(3) << qint64(0) << qint64(1) << "";
        QTest::newRow("inexact-fs") << "100 fs" << "31" << qint64(3) << qint64(0) << qint64(0) << "exactly";
        QTest::newRow("inexact-long") << "1 ps" << "18446744073709551615"
            << qint64(2) << qint64(0) << qint64(0) << "exactly";
        QTest::newRow("inexact-huge-project") << "1 ps" << "1"
            << std::numeric_limits<qint64>::max() << qint64(0) << qint64(0) << "exactly";
        QTest::newRow("final-tick-overflow") << "1 ps" << "9223372036854775808"
            << qint64(1) << qint64(0) << qint64(0) << "tick range";
        QTest::newRow("multiply-overflow") << "100 s" << "18446744073709551615"
            << qint64(1) << qint64(0) << qint64(0) << "tick range";
        QTest::newRow("offset-overflow") << "1 ps" << "9223372036854775807"
            << qint64(1) << qint64(1) << qint64(0) << "Aligned";
    }

    void longVcdTimes()
    {
        QFETCH(QString, scale); QFETCH(QString, raw); QFETCH(qint64, ps);
        QFETCH(qint64, offset); QFETCH(qint64, expected); QFETCH(QString, error);
        std::istringstream input(QString("$timescale %1 $end\n$var wire 1 ! a $end\n"
            "$enddefinitions $end\n#%2 1!\n").arg(scale, raw).toStdString());
        wave::TraceParseOptions options;
        options.projectTimeBase = {ps};
        options.offset = offset;
        const auto parsed = wave::parseVcd(input, options);
        if (!error.isEmpty()) {
            QVERIFY(!parsed.ok());
            QVERIFY2(QString::fromStdString(parsed.errorSummary()).contains(error),
                     parsed.errorSummary().c_str());
            return;
        }
        QVERIFY2(parsed.ok(), parsed.errorSummary().c_str());
        QCOMPARE(parsed.index->startTick, expected);
        QCOMPARE(parsed.index->endTick, expected);
        QCOMPARE(parsed.index->traceSignals.front().transitions.front().tick, expected);
    }

    void saveAsPreservesReferences_data()
    {
        QTest::addColumn<bool>("crossDirectory");
        QTest::addColumn<bool>("untitled");
        QTest::newRow("same-directory") << false << false;
        QTest::newRow("cross-directory") << true << false;
        QTest::newRow("first-save") << false << true;
    }

    void saveAsPreservesReferences()
    {
        QFETCH(bool, crossDirectory); QFETCH(bool, untitled);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto oldPath = directory.filePath("old/original.wave.json");
        const auto newPath = directory.filePath(crossDirectory ? "new/saved.wave.json" : "old/saved.wave.json");
        const auto tracePath = directory.filePath("old/waves/input.vcd");
        const auto otherPath = directory.filePath("shared/other.vcd");
        QVERIFY(write(tracePath, vcd)); QVERIFY(write(otherPath, vcd));
        QDir().mkpath(QFileInfo(newPath).absolutePath());
        auto project = fixture();
        project.importedTraces = {
            {"relative", untitled ? tracePath.toStdString() : "waves/input.vcd", "vcd", 0, {}, {}},
            {"parent", untitled ? otherPath.toStdString() : "../shared/other.vcd", "vcd", 0, {}, {}},
            {"absolute", otherPath.toStdString(), "vcd", 0, {}, {}},
            {"missing", untitled ? directory.filePath("old/waves/missing.vcd").toStdString()
                                : "waves/missing.vcd", "vcd", 0, {}, {}}
        };
        if (!untitled) QVERIFY(wave::saveProjectFileAtomic(project, oldPath));
        wave::MainWindow window(project, untitled ? QString{} : oldPath);
        window.show();
        const auto result = save(window, newPath);
        QVERIFY2(result.value("ok").toBool(), QJsonDocument(result).toJson().constData());
        const auto reopened = wave::loadProjectFile(newPath);
        QVERIFY2(reopened.ok(), qPrintable(reopened.error));
        QCOMPARE(reopened.project->importedTraces, window.project().importedTraces);
        QCOMPARE(resolve(newPath, reopened.project->importedTraces[0].path), tracePath);
        QCOMPARE(resolve(newPath, reopened.project->importedTraces[1].path), otherPath);
        QCOMPARE(resolve(newPath, reopened.project->importedTraces[2].path), otherPath);
        QCOMPARE(resolve(newPath, reopened.project->importedTraces[3].path),
                 directory.filePath("old/waves/missing.vcd"));
        QVERIFY(!QFileInfo(QString::fromStdString(reopened.project->importedTraces[0].path)).isAbsolute());
        QVERIFY(!result.value("state").toObject().value("dirty").toBool());
        wave::MainWindow opened(*reopened.project, newPath, nullptr, std::nullopt, true);
        opened.show();
        QTRY_VERIFY_WITH_TIMEOUT(!opened.findChildren<wave::TraceCanvas*>().isEmpty(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(probe(*opened.findChildren<wave::TraceCanvas*>().front(), 230) == 0, 5000);
        capture(opened, crossDirectory ? "save-as-cross-directory" : "save-as-first-or-same");
    }

    void saveFailureAndHistory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto oldPath = directory.filePath("old/project.wave.json");
        const auto newPath = directory.filePath("new/project.wave.json");
        const auto tracePath = directory.filePath("old/input.vcd");
        QVERIFY(write(tracePath, vcd));
        QDir().mkpath(QFileInfo(newPath).absolutePath());
        auto project = fixture();
        project.importedTraces = {{"trace", "input.vcd", "vcd", 0, {}, {}}};
        QVERIFY(wave::saveProjectFileAtomic(project, oldPath));
        wave::MainWindow window(project, oldPath);
        auto changed = renameScenario(window);
        QVERIFY2(changed.value("ok").toBool(), QJsonDocument(changed).toJson().constData());
        const auto before = window.project();
        const auto beforeState = window.automationSessionState();
        QVERIFY(beforeState.value("dirty").toBool());
        const auto blocked = directory.filePath("not-a-directory");
        QVERIFY(write(blocked, "block"));
        const auto failed = save(window, blocked + "/failed.wave.json");
        QVERIFY(!failed.value("ok").toBool());
        QVERIFY(window.project() == before);
        const auto afterState = window.automationSessionState();
        QCOMPARE(afterState.value("projectPath"), beforeState.value("projectPath"));
        QCOMPARE(afterState.value("modelSha256"), beforeState.value("modelSha256"));
        QCOMPARE(afterState.value("loadedSha256"), beforeState.value("loadedSha256"));
        QCOMPARE(afterState.value("dirty"), beforeState.value("dirty"));
        QVERIFY(!QFileInfo::exists(blocked + "/failed.wave.json"));
        const auto saved = save(window, newPath);
        QVERIFY2(saved.value("ok").toBool(), QJsonDocument(saved).toJson().constData());
        QCOMPARE(resolve(newPath, window.project().importedTraces[0].path), tracePath);
        const auto undone = command(window, "undo");
        QVERIFY2(undone.value("ok").toBool(), QJsonDocument(undone).toJson().constData());
        QCOMPARE(window.project().scenarios[0].name, std::string("Scenario"));
        QCOMPARE(resolve(newPath, window.project().importedTraces[0].path), tracePath);
        const auto redone = command(window, "redo");
        QVERIFY(redone.value("ok").toBool());
        QCOMPARE(resolve(newPath, window.project().importedTraces[0].path), tracePath);
        QVERIFY(!redone.value("state").toObject().value("dirty").toBool());
    }


    void firstSaveRelativeAndFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        struct RestoreDirectory {
            QString path = QDir::currentPath();
            ~RestoreDirectory() { QDir::setCurrent(path); }
        } restore;
        QVERIFY(QDir::setCurrent(directory.path()));
        const auto source = directory.filePath("waves/input.vcd");
        QVERIFY(write(source, vcd));
        const auto destination = directory.filePath("projects/first.wave.json");
        QDir().mkpath(QFileInfo(destination).absolutePath());
        auto project = fixture();
        project.importedTraces = {{"trace", "waves/input.vcd", "vcd", 0, {}, {}}};
        wave::MainWindow window(project);
        const auto state = window.automationSessionState();
        const auto blocked = directory.filePath("blocked");
        QVERIFY(write(blocked, "block"));
        QVERIFY(!save(window, blocked + "/first.wave.json").value("ok").toBool());
        QVERIFY(window.project() == project);
        QCOMPARE(window.automationSessionState().value("projectPath"), state.value("projectPath"));
        QCOMPARE(window.automationSessionState().value("dirty"), state.value("dirty"));
        const auto result = save(window, destination);
        QVERIFY2(result.value("ok").toBool(), QJsonDocument(result).toJson().constData());
        QCOMPARE(resolve(destination, window.project().importedTraces[0].path), source);
        QVERIFY(!QFileInfo(QString::fromStdString(window.project().importedTraces[0].path)).isAbsolute());
    }

    void repairHistoryPaths_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("mapping-snapshot") << 0;
        QTest::newRow("identity-snapshot") << 1;
        QTest::newRow("source-snapshot") << 2;
    }

    void repairHistoryPaths()
    {
        QFETCH(int, kind);
        auto project = fixture();
        project.importedTraces = {{"trace", "old.vcd", "vcd", 0, {{"lane", "a"}}, {}}};
        wave::CommandStack history;
        const auto original = project.importedTraces[0];
        if (kind == 0) QVERIFY(history.execute(
            std::make_unique<wave::RemoveTraceMappingAtIndexCommand>(project, 0, original, "lane")));
        if (kind == 1) QVERIFY(history.execute(
            std::make_unique<wave::ChangeTraceIdentityAtIndexCommand>(project, 0, original, "renamed")));
        if (kind == 2) QVERIFY(history.execute(
            std::make_unique<wave::ChangeTraceSourceAtIndexCommand>(project, 0, original, "new.vcd", "vcd")));
        history.visitTracePaths([](std::string& path) { path = "../original/" + path; });
        QVERIFY(history.undo());
        QCOMPARE(project.importedTraces[0].path, std::string("../original/old.vcd"));
        QVERIFY(history.redo());
        QCOMPARE(project.importedTraces[0].path, std::string(kind == 2
            ? "../original/new.vcd" : "../original/old.vcd"));
    }

    void offsetPreservesViewport()
    {
        std::istringstream input(vcd.toStdString());
        wave::TraceParseOptions options; options.projectTimeBase = {1000};
        auto parsed = wave::parseVcd(input, options);
        QVERIFY(parsed.ok());
        auto project = fixture();
        wave::ImportedTrace reference;
        wave::TraceCanvas canvas;
        canvas.resize(1000, 260);
        canvas.setTrace(&project, &project.scenarios[0], &*parsed.index, &reference);
        canvas.show(); canvas.fitTrace(); QApplication::processEvents();
        canvas.zoomIn(); canvas.zoomIn(); canvas.zoomIn();
        canvas.horizontalScrollBar()->setValue(canvas.horizontalScrollBar()->maximum() / 3);
        const auto original = probe(canvas, 570);
        const auto originalScroll = canvas.horizontalScrollBar()->value();
        wave::Tick currentOffset = 0;
        for (const wave::Tick offset : {250, -300, 40, 0}) {
            QVERIFY(parsed.index->shift(offset - currentOffset));
            currentOffset = offset;
            canvas.refreshTrace();
            QCOMPARE(probe(canvas, 570), original + offset);
            QCOMPARE(canvas.horizontalScrollBar()->value(), originalScroll);
        }
        QVERIFY(parsed.index->shift(-300)); canvas.refreshTrace();
        canvas.fitTrace();
        QCOMPARE(probe(canvas, 230), qint64(-300));
        QVERIFY(probe(canvas, canvas.viewport()->width() - 30) > -210);
        canvas.zoomIn(); canvas.zoomIn(); canvas.zoomIn();
        canvas.revealTick(-280);
        const auto left = probe(canvas, 230);
        const auto right = probe(canvas, canvas.viewport()->width() - 1);
        QVERIFY(left <= -280 && right >= -280);
        canvas.horizontalScrollBar()->setValue(canvas.horizontalScrollBar()->maximum());
        QVERIFY(probe(canvas, canvas.viewport()->width() - 1) >= -201);
        const auto before = parsed.index->traceSignals;
        const auto beforeFailureTick = probe(canvas, canvas.viewport()->width() - 1);
        QVERIFY(!parsed.index->shift(std::numeric_limits<wave::Tick>::min()));
        canvas.refreshTrace();
        QCOMPARE(parsed.index->traceSignals, before);
        QCOMPARE(probe(canvas, canvas.viewport()->width() - 1), beforeFailureTick);
        capture(canvas, "offset-negative-zoom-scroll");
    }

    void windowAlignmentCancelAndFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("project.wave.json");
        QVERIFY(write(directory.filePath("input.vcd"), vcd));
        auto project = fixture();
        project.importedTraces = {{"trace", "input.vcd", "vcd", 0, {}, {}}};
        QVERIFY(wave::saveProjectFileAtomic(project, path));
        wave::MainWindow window(project, path, nullptr, std::nullopt, true);
        window.show();
        auto canvases = window.findChildren<wave::TraceCanvas*>();
        QCOMPARE(canvases.size(), 1); // Current result window; the legacy import dock is not created.
        QTRY_COMPARE_WITH_TIMEOUT(probe(*canvases[0], 230), qint64(0), 5000);
        const auto align = [&](const QString& offset, bool accept) {
            bool found = false;
            QTimer timer;
            timer.setInterval(20);
            connect(&timer, &QTimer::timeout, &window, [&] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if (!dialog) return;
                if (qobject_cast<QMessageBox*>(dialog)) { dialog->accept(); return; }
                const auto fields = dialog->findChildren<QLineEdit*>();
                if (fields.isEmpty()) return;
                found = true;
                fields.front()->setText(offset);
                if (accept) dialog->accept(); else dialog->reject();
            });
            timer.start();
            const bool invoked = QMetaObject::invokeMethod(&window, "alignImportedTrace", Qt::DirectConnection);
            return invoked && found;
        };
        const auto original = window.project();
        QVERIFY(align("250 ns", false));
        QVERIFY(window.project() == original);
        QVERIFY(!window.automationSessionState().value("dirty").toBool());
        for (const qint64 offset : {250, -300, 0}) {
            QVERIFY(align(QString::number(offset) + " ns", true));
            QCOMPARE(window.project().importedTraces[0].offset, offset);
            for (auto* canvas : canvases) {
                canvas->fitTrace();
                QCOMPARE(probe(*canvas, 230), offset);
                canvas->zoomIn(); canvas->zoomIn();
                canvas->revealTick(offset + 80);
                QVERIFY(probe(*canvas, canvas->viewport()->width() - 1) >= offset + 80);
            }
        }
        const auto beforeFailure = window.project();
        const auto revision = window.automationSessionState().value("modelSha256");
        QVERIFY(align("9223372036854775807 ns", true));
        QVERIFY(window.project() == beforeFailure);
        QCOMPARE(window.automationSessionState().value("modelSha256"), revision);
        QVERIFY(align("invalid", true));
        QVERIFY(window.project() == beforeFailure);
        capture(window, "alignment-window");
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir profile;
    if (!profile.isValid()) return 2;
    qputenv("WAVEWORKBENCH_SETTINGS_DIR", profile.path().toUtf8());
    qputenv("WAVEWORKBENCH_RECOVERY_DIR", (profile.path() + "/recovery").toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, profile.path());
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    TraceSessionRegressionTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "trace_session_regression_test.moc"
