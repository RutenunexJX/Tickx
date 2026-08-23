#include "suite_integration.h"

#include "wave/model.h"
#include "wave/project_io.h"

#include <suiteapp/protocol.h>

#include <QTemporaryDir>
#include <QUrl>
#include <QUrlQuery>
#include <QtTest>

class WaveSuiteAppIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void publishesNativeSurfaceContract()
    {
        const QJsonObject descriptor =
            wave::WaveSuiteIntegration::appDescriptor(
                QStringLiteral("1.2.3"), QStringLiteral("test.wave"));
        QString reason;
        QVERIFY2(SuiteApp::validateAppDescriptor(descriptor, &reason),
                 qPrintable(reason));
        QVERIFY(SuiteApp::descriptorOwnsAction(
            descriptor, QStringLiteral("wave.project.open")));
        QVERIFY(SuiteApp::descriptorOwnsSurface(
            descriptor, QStringLiteral("wave.waveform")));
        const QJsonObject surface = descriptor
            .value(QStringLiteral("surfaces")).toArray().first().toObject();
        QCOMPARE(surface.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("native"));
        QCOMPARE(surface.value(QStringLiteral("native")).toObject()
                     .value(QStringLiteral("abi")).toInt(), 1);
    }

    void resolvesProjectThroughProjectIo()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        wave::Project project = wave::makeDemonstrationProject();
        const QString path = directory.filePath(QStringLiteral("demo.wave.json"));
        QString error;
        QVERIFY2(wave::saveProjectFileAtomic(project, path, &error),
                 qPrintable(error));

        QUrl uri;
        uri.setScheme(QStringLiteral("wave"));
        uri.setHost(QStringLiteral("project"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("file"), path);
        uri.setQuery(query);

        wave::WaveSuiteIntegration integration(nullptr);
        const QJsonObject response = integration.processRequestForTesting(
            SuiteApp::makeRequest(
                QStringLiteral("resource.resolve"),
                {{QStringLiteral("uri"), uri.toString()}}));
        QVERIFY(response.value(QStringLiteral("ok")).toBool());
        QCOMPARE(response.value(QStringLiteral("result")).toObject()
                     .value(QStringLiteral("kind")).toString(),
                 QStringLiteral("wave-project"));
    }
};

QTEST_APPLESS_MAIN(WaveSuiteAppIntegrationTest)

#include "suiteapp_integration_test.moc"
