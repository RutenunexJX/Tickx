#include "main_window.h"
#include "suite_integration.h"
#include "ui_controls.h"
#include "wave_canvas.h"
#include "wave/project_io.h"

#include <suiteapp/client.h>
#include <suiteapp/runtime.h>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest>

namespace {
class OwnedProcess : public QProcess {
public:
    ~OwnedProcess() override {
        if (state() != NotRunning) {
            terminate();
            if (!waitForFinished(3000)) { kill(); waitForFinished(3000); }
        }
    }
};
struct Reply {
    int code = -1;
    QByteArray output;
    QJsonObject json;
    QJsonObject state() const { return json.value("result").toObject().value("state").toObject(); }
};
QByteArray bytes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
QString sha(const QString& path)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes(path), QCryptographicHash::Sha256).toHex());
}
QJsonObject batch(QJsonArray operations)
{
    return {{"schema", "wave-workbench.operations/v1"}, {"operations", operations}};
}
}

class SessionCliTest : public QObject {
    Q_OBJECT
    QTemporaryDir data_;
    QString endpoint_ = "suiteapp.test.session." + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonArray transcript_;
    int nextFile_ = 0;
    Reply cli(const QStringList& args, const QByteArray& input = {})
    {
        OwnedProcess process;
        process.start(QStringLiteral(WAVE_CLI_PATH), args);
        if (!process.waitForStarted(5000)) return {};
        process.write(input);
        process.closeWriteChannel();
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        connect(&process, &QProcess::finished, &loop, &QEventLoop::quit);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(20000);
        if (process.state() != QProcess::NotRunning) loop.exec();
        if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(2000); }
        Reply reply;
        reply.code = process.exitCode();
        reply.output = process.readAllStandardOutput();
        const auto diagnostics = process.readAllStandardError();
        reply.json = QJsonDocument::fromJson(reply.output).object();
        transcript_.append(QJsonObject{{"arguments", QJsonArray::fromStringList(args)},
            {"exitCode", reply.code}, {"json", reply.json},
            {"stderr", QString::fromUtf8(diagnostics)}});
        if (reply.code != 0)
            qInfo().noquote() << "CLI" << args.join(' ') << reply.code
                << reply.output.left(450) << diagnostics.left(450);
        return reply;
    }
    Reply session(const QString& id, const QString& command, const QJsonObject& arguments = {},
                  const QStringList& flags = {})
    {
        QStringList args{"session", command, "--runtime=" + endpoint_, "--session=" + id};
        args += flags;
        if (!arguments.isEmpty()) args << "--args=-";
        return cli(args, arguments.isEmpty() ? QByteArray{} : QJsonDocument(arguments).toJson());
    }
    QString file(const QJsonObject& value)
    {
        const auto path = data_.filePath(QStringLiteral("参数 %1.json").arg(++nextFile_));
        QFile output(path);
        if (!output.open(QIODevice::WriteOnly)) return {};
        output.write(QJsonDocument(value).toJson());
        return path;
    }
    void capture(wave::MainWindow& window, const QString& name)
    {
        const auto directory = qEnvironmentVariable("WAVE_SESSION_EVIDENCE_DIR");
        if (directory.isEmpty()) return;
        QDir().mkpath(directory);
        window.grab().save(QDir(directory).filePath(name + ".png"));
    }
private slots:
    void initTestCase()
    {
        QVERIFY(data_.isValid());
        QCoreApplication::setOrganizationName("TickxSessionTests");
        QCoreApplication::setApplicationName("PublicCli");
        QCoreApplication::setApplicationVersion(WAVE_TEST_APPLICATION_VERSION);
        QApplication::setQuitOnLastWindowClosed(false);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, data_.path());
        qputenv("WAVEWORKBENCH_RECOVERY_DIR", data_.path().toUtf8());
        qputenv("WAVEWORKBENCH_REDUCED_MOTION", "1");
        wave::ui::initialize();
        wave::ui::initializeApplicationTheme();
    }
    void publicCliControlsRealWindows()
    {
        const auto path = data_.filePath(QStringLiteral("同一路径 project.wave.json"));
        const auto created = cli({"new", path, "--project-id=session-project", "--scenario-id=main",
            "--duration=100 ns", "--operations=" + QStringLiteral(WAVE_SESSION_EXAMPLE)});
        QCOMPARE(created.code, 0);
        const auto loaded = wave::loadProjectFile(path);
        QVERIFY2(loaded.ok(), qPrintable(loaded.error));
        QCOMPARE(loaded.project->scenarios.size(), std::size_t(2));
        QCOMPARE(cli({"validate", path}).code, 0);
        QCOMPARE(cli({"session", "schema"}).code, 0);
        QCOMPARE(cli({"session", "view", "--cursor=3 ns"}).code, 2);
        QCOMPARE(cli({"session", "view", "--session=" + QUuid::createUuid().toString(QUuid::WithoutBraces),
            "--runtime=" + endpoint_}).code, 3);

        OwnedProcess runtime;
        runtime.start(QStringLiteral(WAVE_SUITE_RUNTIME_PATH), {"--endpoint", endpoint_});
        QVERIFY(runtime.waitForStarted(5000));
        SuiteApp::Client client(endpoint_, 300);
        QTRY_VERIFY_WITH_TIMEOUT(client.listProviders().hasResponse(), 5000);
        wave::MainWindow first(*loaded.project, path);
        wave::MainWindow second(wave::makeDemonstrationProject());
        first.resize(1280, 820);
        second.resize(1100, 700);
        first.show();
        second.show();
        auto* canvas = first.findChild<wave::WaveCanvas*>();
        QVERIFY(canvas);
        first.activateWindow();
        canvas->setFocus();
        QTest::qWait(120);
        wave::WaveSuiteIntegration firstProvider(&first), secondProvider(&second);
        SuiteApp::RuntimeStartOptions options;
        options.endpoint = endpoint_;
        options.startIfMissing = false;
        QString reason;
        QVERIFY2(firstProvider.start(options, &reason), qPrintable(reason));
        QVERIFY2(secondProvider.start(options, &reason), qPrintable(reason));
        const auto id = first.automationSessionId();
        const auto otherId = second.automationSessionId();
        const auto listed = cli({"session", "list", "--runtime=" + endpoint_});
        QCOMPARE(listed.code, 0);
        QCOMPARE(listed.json.value("result").toObject().value("sessions").toArray().size(), 2);
        const auto initial = session(id, "status").state();
        const auto otherInitial = session(otherId, "status").state();
        QCOMPARE(initial.value("loadedSha256").toString(), sha(path));
        QCOMPARE(initial.value("adoption").toString(), QString("current"));
        capture(first, "before-view");

        QJsonObject view{{"scenarioId", "main"}, {"cursor", "25 ns"}, {"start", "10 ns"},
            {"end", "50 ns"}, {"laneIds", QJsonArray{"group-core"}},
            {"range", QJsonObject{{"start", "20 ns"}, {"end", "30 ns"}}}};
        const auto shown = session(id, "view", view);
        QCOMPARE(shown.code, 0);
        QCOMPARE(canvas->cursorTick(), wave::Tick(25000));
        QVERIFY(qAbs(canvas->visibleTimeRange().first - 10000) < 100);
        QVERIFY(qAbs(canvas->visibleTimeRange().second - 50000) < 100);
        QVERIFY(canvas->hasExplicitRangeSelection());
        QCOMPARE(canvas->selectedTimeRange()->first, wave::Tick(20000));
        QVERIFY(canvas->selectedLaneIds().contains("lane-request"));
        QVERIFY(first.isVisible());
        capture(first, "after-view");
        QCOMPARE(session(otherId, "status").state().value("modelSha256"), otherInitial.value("modelSha256"));
        const auto beforeInvalid = session(id, "status").state().value("view");
        QCOMPARE(session(id, "view", {{"cursor", "70 ns"}, {"laneIds", QJsonArray{"missing"}}}).code, 4);
        QCOMPARE(session(id, "status").state().value("view"), beforeInvalid);
        QCOMPARE(session(id, "view", {{"cursor", "70 ns"}, {"dryRun", true}}).code, 0);
        QCOMPARE(canvas->cursorTick(), wave::Tick(25000));
        QCOMPARE(session(id, "view", {{"cursor", 4}}).code, 4);
        QCOMPARE(session(id, "view", {{"start", "40 ns"}, {"end", "10 ns"}}).code, 4);
        QCOMPARE(session(id, "view", {{"markerIds", QJsonArray{"marker-active"}},
            {"relationIds", QJsonArray{"relation-response"}}}).code, 0);
        QVERIFY(canvas->selectedMarkerIds().contains("marker-active"));
        QVERIFY(canvas->selectedRelationIds().contains("relation-response"));
        QCOMPARE(session(id, "view", {{"groups", QJsonArray{QJsonObject{{"id", "group-core"}, {"collapsed", true}}}}}).code, 0);
        QVERIFY(canvas->isGroupCollapsed("group-core"));
        QCOMPARE(session(id, "view", {{"groups", QJsonArray{QJsonObject{{"id", "group-core"}, {"collapsed", false}}}},
            {"relationsVisible", false}, {"range", QJsonValue::Null}}).code, 0);
        QVERIFY(!canvas->relationsVisible());
        QCOMPARE(session(id, "view", {{"pan", "10 ns"}}).code, 0);
        QVERIFY(canvas->visibleTimeRange().first >= 19000);
        const auto span = canvas->visibleTimeSpan();
        QCOMPARE(session(id, "view", {{"zoom", "in"}}).code, 0);
        QVERIFY(canvas->visibleTimeSpan() < span);
        QCOMPARE(session(id, "view", {{"zoom", "out"}}).code, 0);
        QCOMPARE(session(id, "view", {{"scenarioId", "alternate"}, {"zoom", "fit"}}).code, 0);
        QCOMPARE(session(id, "status").state().value("view").toObject().value("scenarioId").toString(), QString("alternate"));
        QCOMPARE(session(id, "view", {{"scenarioId", "main"}}).code, 0);

        const auto rename = batch(QJsonArray{QJsonObject{{"op", "rename-lane"},
            {"laneId", "lane-request"}, {"name", "request_online"}}});
        auto state = session(id, "status").state();
        const auto beforeModel = state.value("modelSha256").toString();
        QCOMPARE(session(id, "apply", {{"batch", rename}, {"dryRun", true}, {"scenarioId", "main"}}).code, 0);
        QCOMPARE(session(id, "status").state().value("modelSha256").toString(), beforeModel);
        auto invalidBatch = rename;
        auto operations = rename.value("operations").toArray();
        operations.append(QJsonObject{{"op", "rename-lane"}, {"laneId", "unknown"}, {"name", "bad"}});
        invalidBatch.insert("operations", operations);
        QCOMPARE(session(id, "apply", {{"batch", invalidBatch}, {"expectedModelSha256", beforeModel},
            {"scenarioId", "main"}}).code, 4);
        QCOMPARE(session(id, "status").state().value("modelSha256").toString(), beforeModel);
        QCOMPARE(session(id, "apply", {{"batch", rename}}).code, 4);
        QCOMPARE(session(id, "apply", {{"batch", rename}, {"expectedModelSha256", QString(64, '0')}}).code, 4);
        QCOMPARE(session(id, "view", view).code, 0);
        const auto applied = session(id, "apply", {{"batch", rename}, {"scenarioId", "main"},
            {"expectedModelSha256", beforeModel}});
        QCOMPARE(applied.code, 0);
        QVERIFY(applied.state().value("dirty").toBool());
        QVERIFY(!session(id, "status").state().value("activeEditor").toBool());
        QCOMPARE(session(id, "apply", {{"batch", rename}, {"expectedModelSha256", beforeModel}}).code, 4);
        QCOMPARE(wave::findLane(first.project().scenarios[0], "lane-request")->name, std::string("request_online"));

        state = session(id, "status").state();
        const auto savedBefore = bytes(path);
        QCOMPARE(session(id, "save", {{"expectedModelSha256", state.value("modelSha256")},
            {"expectedDiskSha256", state.value("loadedSha256")}, {"dryRun", true}}).code, 0);
        QCOMPARE(bytes(path), savedBefore);
        QCOMPARE(session(id, "save", {{"expectedModelSha256", state.value("modelSha256")},
            {"expectedDiskSha256", QString(64, '0')}}).code, 4);
        QCOMPARE(bytes(path), savedBefore);
        QCOMPARE(session(id, "save", {{"projectPath", data_.filePath("missing/failed.wave.json")},
            {"expectedModelSha256", state.value("modelSha256")}, {"expectedDiskSha256", "missing"}}).code, 4);
        QCOMPARE(session(id, "status").state().value("modelSha256"), state.value("modelSha256"));
        QCOMPARE(session(id, "undo", {{"expectedModelSha256", state.value("modelSha256")}}).code, 0);
        QCOMPARE(session(id, "status").state().value("modelSha256").toString(), beforeModel);
        state = session(id, "status").state();
        QCOMPARE(session(id, "redo", {{"expectedModelSha256", state.value("modelSha256")}}).code, 0);
        state = session(id, "status").state();
        QCOMPARE(session(id, "save", {{"expectedModelSha256", state.value("modelSha256")},
            {"expectedDiskSha256", state.value("loadedSha256")}}).code, 0);
        QVERIFY(!session(id, "status").state().value("dirty").toBool());
        QCOMPARE(wave::findLane(wave::loadProjectFile(path).project->scenarios[0], "lane-request")->name,
            std::string("request_online"));

        QCOMPARE(session(id, "view", {{"range", QJsonValue::Null}}).code, 0);
        state = session(id, "status").state();
        const auto visibility = session(id, "apply", {
            {"scenarioId", "main"}, {"expectedModelSha256", state.value("modelSha256")},
            {"batch", batch(QJsonArray{QJsonObject{{"op", "update-group"}, {"groupId", "group-core"}, {"visible", false}}})}});
        QCOMPARE(visibility.code, 0);
        QVERIFY(!canvas->isLaneDisplayed("group-core"));
        QCOMPARE(session(id, "undo", {{"expectedModelSha256", visibility.state().value("modelSha256")}}).code, 0);
        QVERIFY(canvas->isLaneDisplayed("group-core"));
        QCOMPARE(cli({"session", "view", "--session=" + id, "--runtime=" + endpoint_, "--args=-"},
            QByteArray(524289, ' ')).code, 3);

        auto* duration = first.findChild<QLineEdit*>("TimelineDurationEdit");
        QVERIFY(duration);
        duration->setText("120 ns");
        QTest::keyClick(duration, Qt::Key_Return);
        duration->clearFocus();
        canvas->setFocus();
        duration->setModified(false);
        state = session(id, "status").state();
        QVERIFY(state.value("dirty").toBool());
        // A native edit precedes another snapshot batch; undo must restore both histories.
        QCOMPARE(session(id, "apply", {{"batch", batch(QJsonArray{QJsonObject{
            {"op", "rename-lane"}, {"laneId", "lane-data"}, {"name", "data_online"}}})},
            {"scenarioId", "main"}, {"expectedModelSha256", state.value("modelSha256")}}).code, 0);
        state = session(id, "status").state();
        QCOMPARE(session(id, "undo", {{"expectedModelSha256", state.value("modelSha256")}}).code, 0);
        state = session(id, "status").state();
        QCOMPARE(session(id, "undo", {{"expectedModelSha256", state.value("modelSha256")}}).code, 0);
        QCOMPARE(first.project().scenarios[0].duration, wave::Tick(100000));

        const auto externalOps = file(batch(QJsonArray{QJsonObject{
            {"op", "rename-lane"}, {"laneId", "lane-request"}, {"name", "request_disk"}}}));
        QCOMPARE(cli({"apply", path, externalOps, "--in-place", "--scenario=main", "--expect-sha256=" + sha(path)}).code, 0);
        const auto adopted = session(id, "refresh", {{"expectedDiskSha256", sha(path)}}, {"--wait-ms=3000"});
        QCOMPARE(adopted.code, 0);
        QCOMPARE(adopted.state().value("loadedSha256").toString(), sha(path));
        QCOMPARE(wave::findLane(first.project().scenarios[0], "lane-request")->name, std::string("request_disk"));
        QCOMPARE(session(id, "open", {{"projectPath", path}, {"expectedDiskSha256", sha(path)}}).code, 0);

        state = session(id, "status").state();
        const auto dirtyEdit = session(id, "apply", {{"batch", rename}, {"scenarioId", "main"},
            {"expectedModelSha256", state.value("modelSha256")}});
        QCOMPARE(dirtyEdit.code, 0);
        const auto dirtyHash = dirtyEdit.state().value("modelSha256");
        const auto externalAgain = file(batch(QJsonArray{QJsonObject{
            {"op", "rename-lane"}, {"laneId", "lane-request"}, {"name", "request_external_2"}}}));
        QCOMPARE(cli({"apply", path, externalAgain, "--in-place", "--scenario=main"}).code, 0);
        const auto conflict = session(id, "refresh", {{"expectedDiskSha256", sha(path)}}, {"--wait-ms=120"});
        QCOMPARE(conflict.code, 6);
        QCOMPARE(conflict.state().value("adoption").toString(), QString("conflict"));
        QCOMPARE(conflict.state().value("modelSha256"), dirtyHash);
        QCOMPARE(session(id, "save", {{"expectedModelSha256", dirtyHash}, {"expectedDiskSha256", sha(path)}}).code, 4);
        QCOMPARE(session(id, "undo", {{"expectedModelSha256", dirtyHash}}).code, 0);
        QCOMPARE(session(id, "refresh", {{"expectedDiskSha256", sha(path)}}, {"--wait-ms=3000"}).code, 0);

        duration->setText("uncommitted");
        duration->setModified(true);
        duration->setFocus();
        const auto beforeActive = session(id, "status").state().value("loadedSha256");
        QCOMPARE(cli({"apply", path, externalOps, "--in-place", "--scenario=main"}).code, 0);
        const auto pending = session(id, "refresh", {{"expectedDiskSha256", sha(path)}});
        QCOMPARE(pending.code, 6);
        QVERIFY(pending.state().value("activeEditor").toBool());
        QCOMPARE(pending.state().value("loadedSha256"), beforeActive);
        QCOMPARE(session(id, "view", {{"cursor", "1 ns"}}).code, 4);
        duration->setText("100 ns");
        duration->setModified(false);
        duration->clearFocus();
        canvas->setFocus();
        QCOMPARE(session(id, "refresh", {{"expectedDiskSha256", sha(path)}}, {"--wait-ms=3000"}).code, 0);

        const auto validBytes = bytes(path);
        const auto validModel = session(id, "status").state().value("modelSha256");
        {
            QFile bad(path); QVERIFY(bad.open(QIODevice::WriteOnly)); bad.write("{broken");
        }
        const auto invalidDisk = session(id, "refresh");
        QCOMPARE(invalidDisk.code, 6);
        QCOMPARE(invalidDisk.state().value("adoption").toString(), QString("invalid"));
        QCOMPARE(invalidDisk.state().value("modelSha256"), validModel);
        { QFile restore(path); QVERIFY(restore.open(QIODevice::WriteOnly)); restore.write(validBytes); }
        QCOMPARE(session(id, "refresh", {}, {"--wait-ms=3000"}).code, 0);
        QCOMPARE(session(id, "refresh", {{"expectedDiskSha256", QString(64, '0')}}).code, 4);
        QCOMPARE(session(QUuid::createUuid().toString(QUuid::WithoutBraces), "status").code, 4);

        const auto secondFile = data_.filePath(QStringLiteral("保存 reopen.wave.json"));
        state = session(id, "status").state();
        QCOMPARE(session(id, "save", {{"projectPath", secondFile}, {"expectedModelSha256", state.value("modelSha256")},
            {"expectedDiskSha256", "missing"}}).code, 0);
        QCOMPARE(session(id, "open", {{"projectPath", path}}).code, 0);
        QCOMPARE(session(id, "open", {{"projectPath", secondFile}}).code, 0);
        QCOMPARE(session(id, "status").state().value("loadedSha256").toString(), sha(secondFile));
        QCOMPARE(cli({"generate", secondFile, data_.filePath(QStringLiteral("导出 export")), "--scenario=main"}).code, 0);
        capture(first, "completed");
    }
    void cleanupTestCase()
    {
        const auto directory = qEnvironmentVariable("WAVE_SESSION_EVIDENCE_DIR");
        if (directory.isEmpty()) return;
        QDir().mkpath(directory);
        QFile output(QDir(directory).filePath("session-cli-transcript.json"));
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write(QJsonDocument(transcript_).toJson());
    }
};
QTEST_MAIN(SessionCliTest)
#include "session_cli_test.moc"
