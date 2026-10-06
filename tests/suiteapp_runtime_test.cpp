#include "suite_integration.h"
#include "main_window.h"
#include "ui_controls.h"
#include "wave/model.h"
#include "wave/project_io.h"

#include <suiteapp/client.h>
#include <suiteapp/runtime.h>

#include <QAction>
#include <QApplication>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QtConcurrent>
#include <QtTest>

namespace {

QString uniqueEndpoint()
{
    return QStringLiteral("suiteapp.test.tickx.%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString projectUri(const QString& path)
{
    QUrl uri;
    uri.setScheme(QStringLiteral("wave"));
    uri.setHost(QStringLiteral("project"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("file"), path);
    uri.setQuery(query);
    return uri.toString(QUrl::FullyEncoded);
}

// The client waits on another thread while the real GUI provider handles IPC.
template<typename Request>
SuiteApp::TransportResult route(Request request)
{
    QFutureWatcher<SuiteApp::TransportResult> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcherBase::finished,
                     &loop, &QEventLoop::quit);
    watcher.setFuture(QtConcurrent::run(std::move(request)));
    if (!watcher.isFinished())
        loop.exec();
    const auto result = watcher.result();
    qInfo().noquote() << "IPC response:"
                      << QJsonDocument(result.response).toJson(QJsonDocument::Compact)
                      << result.errorCode << result.errorMessage;
    return result;
}

bool succeeded(const SuiteApp::TransportResult& result)
{
    return result.hasResponse()
        && result.response.value(QStringLiteral("ok")).toBool();
}

QJsonObject resultObject(const SuiteApp::TransportResult& result)
{
    return result.response.value(QStringLiteral("result")).toObject();
}

QString errorCode(const SuiteApp::TransportResult& result)
{
    return result.response.value(QStringLiteral("error")).toObject()
        .value(QStringLiteral("code")).toString();
}

} // namespace

class WaveSuiteAppRuntimeTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(settings_.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("TickxTests"));
        QCoreApplication::setApplicationName(QStringLiteral("SuiteAppIPC"));
        QCoreApplication::setApplicationVersion(
            QStringLiteral(WAVE_TEST_APPLICATION_VERSION));
        QApplication::setQuitOnLastWindowClosed(false);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_.path());
        qputenv("WAVEWORKBENCH_RECOVERY_DIR", settings_.path().toUtf8());
        qputenv("WAVEWORKBENCH_REDUCED_MOTION", "1");
        wave::ui::initialize();
        wave::ui::initializeApplicationTheme();
    }

    void unavailableRuntimePreservesStandaloneOpening()
    {
        QTemporaryDir data;
        QVERIFY(data.isValid());
        auto project = wave::makeDemonstrationProject();
        project.id = "standalone-fallback";
        const QString path = data.filePath(QStringLiteral("fallback.wave.json"));
        QString reason;
        QVERIFY(wave::saveProjectFileAtomic(project, path, &reason));
        wave::MainWindow window(wave::makeDemonstrationProject());
        wave::WaveSuiteIntegration integration(&window);
        SuiteApp::RuntimeStartOptions options;
        options.endpoint = uniqueEndpoint();
        options.startIfMissing = false;
        options.probeTimeoutMs = 20;
        QVERIFY(!integration.start(options, &reason));
        QVERIFY(!reason.isEmpty());
        QVERIFY(!integration.isRegistered());
        QVERIFY(window.openSuiteProject(path));
        QCOMPARE(QString::fromStdString(window.project().id),
                 QStringLiteral("standalone-fallback"));
    }

    void realRuntimeRoutesAndUnregisters()
    {
        QTemporaryDir data;
        QVERIFY(data.isValid());
        auto first = wave::makeDemonstrationProject();
        first.id = "ipc-first";
        auto second = first;
        second.id = "ipc-second";
        second.name = "IPC second project";
        const QString firstPath = data.filePath(QStringLiteral("first 项目.wave.json"));
        const QString secondPath = data.filePath(QStringLiteral("second.wave.json"));
        QString reason;
        QVERIFY(wave::saveProjectFileAtomic(first, firstPath, &reason));
        QVERIFY(wave::saveProjectFileAtomic(second, secondPath, &reason));

        const QString endpoint = uniqueEndpoint();
        QProcess runtime;
        runtime.setProcessChannelMode(QProcess::MergedChannels);
        runtime.start(QStringLiteral(WAVE_SUITE_RUNTIME_PATH),
                      {QStringLiteral("--endpoint"), endpoint});
        QVERIFY2(runtime.waitForStarted(5000), qPrintable(runtime.errorString()));
        qInfo().noquote() << "Runtime endpoint:" << endpoint << "PID:" << runtime.processId();
        const SuiteApp::Client client(endpoint, 4000);
        QTRY_VERIFY_WITH_TIMEOUT(succeeded(client.listProviders()), 5000);

        wave::MainWindow window(first, firstPath);
        auto integration = std::make_unique<wave::WaveSuiteIntegration>(&window);
        SuiteApp::RuntimeStartOptions options;
        options.endpoint = endpoint;
        options.startIfMissing = false;
        QVERIFY2(integration->start(options, &reason), qPrintable(reason));
        QVERIFY(integration->start(options, &reason));
        QVERIFY(integration->isRegistered());
        const auto registered = resultObject(client.listProviders())
            .value(QStringLiteral("providers")).toArray();
        QCOMPARE(registered.size(), 1);
        const auto descriptor = registered.first().toObject();
        QCOMPARE(descriptor.value(QStringLiteral("appId")).toString(), QStringLiteral("wave"));
        QCOMPARE(descriptor.value(QStringLiteral("displayName")).toString(), QStringLiteral("Tickx"));
        QCOMPARE(descriptor.value(QStringLiteral("version")).toString(),
                 QStringLiteral(WAVE_TEST_APPLICATION_VERSION));
        qInfo().noquote() << "Registered descriptor:"
                          << QJsonDocument(descriptor).toJson(QJsonDocument::Compact);

        const auto resolved = route([&] {
            return client.resolveResource(projectUri(firstPath));
        });
        QVERIFY(succeeded(resolved));
        QCOMPARE(resultObject(resolved).value(QStringLiteral("projectId")).toString(),
                 QStringLiteral("ipc-first"));
        const auto described = route([&] {
            return client.describeSurface(QStringLiteral("wave.waveform"), projectUri(firstPath));
        });
        QVERIFY(succeeded(described));
        QCOMPARE(resultObject(described).value(QStringLiteral("native")).toObject()
                     .value(QStringLiteral("factory")).toString(),
                 QStringLiteral("wavewidgets_create_simulation_workspace_v1"));
        const auto opened = route([&] {
            return client.invokeAction(QStringLiteral("wave.project.open"), {}, projectUri(secondPath));
        });
        QVERIFY(succeeded(opened));
        QCOMPARE(QString::fromStdString(window.project().id), QStringLiteral("ipc-second"));
        QVERIFY(window.isVisible());

        const auto beforeEdit = window.project();
        auto* duration = window.findChild<QLineEdit*>("TimelineDurationEdit");
        QVERIFY(duration);
        duration->setText(QStringLiteral("1234 ns"));
        QTest::keyClick(duration, Qt::Key_Return);
        QVERIFY(!(window.project() == beforeEdit));
        const auto dirtyProject = window.project();
        bool cancelled = false;
        QTimer cancelDialog;
        connect(&cancelDialog, &QTimer::timeout, &window, [&] {
            if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                if (auto* cancel = dialog->button(QMessageBox::Cancel)) {
                    cancelled = true;
                    cancel->click();
                }
            }
        });
        cancelDialog.start(10);
        const auto cancelledOpen = route([&] {
            return client.invokeAction(QStringLiteral("wave.project.open"), {}, projectUri(firstPath));
        });
        cancelDialog.stop();
        QVERIFY(cancelled);
        QCOMPARE(errorCode(cancelledOpen), QStringLiteral("project_open_cancelled"));
        QVERIFY(window.project() == dirtyProject);
        const auto sameProject = route([&] {
            return client.invokeAction(QStringLiteral("wave.project.open"), {}, projectUri(secondPath));
        });
        QVERIFY(succeeded(sameProject));
        QVERIFY(window.project() == dirtyProject);
        auto* undo = window.findChild<QAction*>("UndoAction");
        QVERIFY(undo);
        undo->trigger();
        QVERIFY(window.project() == beforeEdit);
        const auto surfaceOpened = route([&] {
            return client.openSurface(QStringLiteral("wave.waveform"), projectUri(firstPath));
        });
        QVERIFY(succeeded(surfaceOpened));
        QCOMPARE(QString::fromStdString(window.project().id), QStringLiteral("ipc-first"));

        const auto invalid = route([&] {
            return client.resolveResource(QStringLiteral("wave://invalid"));
        });
        QCOMPARE(errorCode(invalid), QStringLiteral("invalid_resource"));
        const auto missing = route([&] {
            return client.resolveResource(projectUri(data.filePath(QStringLiteral("missing.wave.json"))));
        });
        QCOMPARE(errorCode(missing), QStringLiteral("project_open_failed"));
        const auto unknown = route([&] {
            return client.invokeAction(QStringLiteral("wave.unknown"), {},
                                       projectUri(firstPath), QStringLiteral("wave"));
        });
        QCOMPARE(errorCode(unknown), QStringLiteral("action_not_supported"));
        const auto unknownSurface = route([&] {
            return client.describeSurface(QStringLiteral("wave.unknown"),
                                          projectUri(firstPath), QStringLiteral("wave"));
        });
        QCOMPARE(errorCode(unknownSurface), QStringLiteral("surface_not_supported"));
        integration.reset();
        const auto afterStop = client.listProviders();
        QVERIFY(succeeded(afterStop));
        QVERIFY(resultObject(afterStop).value(QStringLiteral("providers")).toArray().isEmpty());
        QCOMPARE(errorCode(client.resolveResource(projectUri(firstPath))),
                 QStringLiteral("provider_not_found"));
        QVERIFY2(runtime.waitForFinished(6000), "Runtime did not exit after provider unregister");
        QCOMPARE(runtime.exitCode(), 0);
        qInfo() << "Provider unregistered; isolated Runtime exited normally";
    }

    void startsRuntimeAndCleansUpAfterProviderDestruction()
    {
        SuiteApp::RuntimeStartOptions options;
        options.endpoint = uniqueEndpoint();
        options.executablePath = QStringLiteral(WAVE_SUITE_RUNTIME_PATH);
        options.startupTimeoutMs = 5000;
        const SuiteApp::Client client(options.endpoint, 300);
        QVERIFY(!client.listProviders().hasResponse());
        {
            wave::WaveSuiteIntegration integration(nullptr);
            QString reason;
            QVERIFY2(integration.start(options, &reason), qPrintable(reason));
            QVERIFY(integration.isRegistered());
            QVERIFY(succeeded(client.listProviders()));
        }
        QTRY_VERIFY_WITH_TIMEOUT(!client.listProviders().hasResponse(), 7000);
        qInfo().noquote() << "Auto-start and idle exit verified:" << options.endpoint;
    }

private:
    QTemporaryDir settings_;
};

QTEST_MAIN(WaveSuiteAppRuntimeTest)

#include "suiteapp_runtime_test.moc"