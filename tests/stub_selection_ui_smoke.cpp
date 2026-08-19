#include "main_window.h"

#include "wave/module_manifest.h"
#include "wave/simulation_session.h"
#include "wave/stimulus_scenario.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QTemporaryDir>
#include <QToolButton>

#include <algorithm>
#include <iostream>

namespace {

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

QJsonObject association(const QString& name, const int position)
{
    return {
        {QStringLiteral("name"), name},
        {QStringLiteral("position"), position},
    };
}

QJsonObject instance(
    const QString& name,
    const QString& targetKind,
    const QJsonArray& ports)
{
    return {
        {QStringLiteral("instanceName"), name},
        {QStringLiteral("constructKind"), targetKind},
        {QStringLiteral("sourceFile"),
         QStringLiteral("rtl/wave_fixed_counter.sv")},
        {QStringLiteral("sourceLine"), 12},
        {QStringLiteral("sourceColumn"), 5},
        {QStringLiteral("parameterAssociations"), QJsonArray{}},
        {QStringLiteral("portAssociations"), ports},
        {QStringLiteral("syntaxComplete"), true},
        {QStringLiteral("failureReason"), QString{}},
    };
}

QByteArray unresolvedManifest(const QString& fixtureRoot)
{
    QJsonObject root = QJsonDocument::fromJson(readFile(
        QDir(fixtureRoot).filePath(QStringLiteral("manifest.json"))))
                           .object();
    root.insert(QStringLiteral("schemaVersion"), 4);
    const QJsonObject target = root.value(QStringLiteral("target")).toObject();
    root.insert(
        QStringLiteral("observationScope"),
        QJsonObject{
            {QStringLiteral("mode"), QStringLiteral("module")},
            {QStringLiteral("label"), QStringLiteral("wave_fixed_counter")},
            {QStringLiteral("sourceFile"), target.value(QStringLiteral("sourceFile"))},
            {QStringLiteral("startLine"), target.value(QStringLiteral("sourceLine"))},
            {QStringLiteral("endLine"), target.value(QStringLiteral("sourceLine"))},
        });
    root.insert(QStringLiteral("observations"), QJsonArray{});
    QJsonArray ports = root.value(QStringLiteral("ports")).toArray();
    for (qsizetype index = 0; index < ports.size(); ++index) {
        QJsonObject port = ports.at(index).toObject();
        port.insert(QStringLiteral("structuredLeavesAvailable"), false);
        port.insert(QStringLiteral("editableLeaves"), QJsonArray{});
        port.insert(QStringLiteral("structuredFailureReason"), QString{});
        ports[index] = port;
    }
    root.insert(QStringLiteral("ports"), ports);
    root.insert(
        QStringLiteral("unresolvedDependencies"),
        QJsonArray{
            QJsonObject{
                {QStringLiteral("moduleName"),
                 QStringLiteral("missing_vendor_core")},
                {QStringLiteral("instances"),
                 QJsonArray{instance(
                     QStringLiteral("u_vendor"),
                     QStringLiteral("module"),
                     QJsonArray{
                         association(QStringLiteral("clk_i"), 0),
                         association(QStringLiteral("data_i"), 1),
                     })}},
                {QStringLiteral("stubSupported"), true},
                {QStringLiteral("stubUnsupportedReason"), QString{}},
            },
            QJsonObject{
                {QStringLiteral("moduleName"), QStringLiteral("vendor_bus_if")},
                {QStringLiteral("instances"),
                 QJsonArray{instance(
                     QStringLiteral("u_bus"),
                     QStringLiteral("interface"),
                     QJsonArray{})}},
                {QStringLiteral("stubSupported"), false},
                {QStringLiteral("stubUnsupportedReason"),
                 QStringLiteral("Interface instances are not supported.")},
            },
        });
    return QJsonDocument(root).toJson();
}

QAction* dependencyAction(QMenu* menu, const QString& module)
{
    const auto actions = menu ? menu->actions() : QList<QAction*>{};
    const auto found = std::find_if(
        actions.cbegin(), actions.cend(), [&module](const QAction* action) {
            return action->objectName()
                    == QStringLiteral("SimulationStubDependencyAction")
                && action->property("moduleName").toString() == module;
        });
    return found == actions.cend() ? nullptr : *found;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    if (application.arguments().size() != 2) return 2;

    const QString fixtureRoot = QDir(QStringLiteral(WAVE_SOURCE_DIR))
        .filePath(QStringLiteral("tests/fixtures/simulation/fixed-counter"));
    QTemporaryDir sessionRoot;
    if (!sessionRoot.isValid()) return 3;
    const QString manifestPath = QDir(sessionRoot.path()).filePath(
        QStringLiteral("manifest.json"));
    const QByteArray manifestDocument = unresolvedManifest(fixtureRoot);
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::WriteOnly)
        || manifestFile.write(manifestDocument) != manifestDocument.size()) {
        return 4;
    }
    manifestFile.close();

    const auto parsed = wave::parseZeroSlackModuleManifest(manifestDocument);
    if (!parsed.ok()) return 5;
    const auto imported = wave::importZeroSlackModuleManifest(*parsed.manifest);
    if (!imported.ok() || !imported.project) return 6;

    auto project = *imported.project;
    wave::SimulationRunRequest request;
    request.manifestPath = manifestPath;
    request.stimulusPath = QDir(sessionRoot.path()).filePath(
        QStringLiteral("stimulus.json"));
    request.workspaceRoot = fixtureRoot;
    request.artifactDirectory = QDir(sessionRoot.path()).filePath(
        QStringLiteral("artifacts"));
    request.resultProjectPath = QDir(sessionRoot.path()).filePath(
        QStringLiteral("result.wave.json"));
    request.toolchain.verilatorProgram = QStringLiteral("verilator");
    request.toolchain.cxxProgram = QStringLiteral("c++");
    wave::attachSimulationSession(project, request);

    wave::MainWindow window(std::move(project), {}, nullptr, std::nullopt, true);
    auto* button = window.findChild<QToolButton*>(
        QStringLiteral("SimulationStubDependenciesButton"));
    if (!button || !button->menu() || !button->isEnabled()
        || button->text() != QStringLiteral("Stubs (0/2)")) {
        std::cerr << "stub button mismatch: exists=" << (button != nullptr)
                  << " menu=" << (button && button->menu())
                  << " enabled=" << (button && button->isEnabled())
                  << " text="
                  << (button ? button->text().toStdString() : std::string{})
                  << '\n';
        return 7;
    }
    auto* supported = dependencyAction(
        button->menu(), QStringLiteral("missing_vendor_core"));
    auto* unsupported = dependencyAction(
        button->menu(), QStringLiteral("vendor_bus_if"));
    if (!supported || !supported->isCheckable() || supported->isChecked()
        || !supported->isEnabled() || !unsupported || unsupported->isEnabled()
        || !unsupported->toolTip().contains(QStringLiteral("not supported"))) {
        std::cerr << "stub actions mismatch: supported=" << (supported != nullptr)
                  << " supportedEnabled=" << (supported && supported->isEnabled())
                  << " unsupported=" << (unsupported != nullptr)
                  << " unsupportedEnabled=" << (unsupported && unsupported->isEnabled())
                  << " unsupportedTip="
                  << (unsupported ? unsupported->toolTip().toStdString()
                                  : std::string{})
                  << '\n';
        return 8;
    }

    supported->trigger();
    application.processEvents();
    const auto session = wave::simulationSessionFromProject(window.project());
    if (!session.ok()
        || session.request->stubbedModules
            != QStringList{QStringLiteral("missing_vendor_core")}
        || button->text() != QStringLiteral("Stubs (1/2)")) {
        std::cerr << "stub selection persistence mismatch: session="
                  << session.ok() << " selected="
                  << (session.ok()
                          ? session.request->stubbedModules.join(',').toStdString()
                          : std::string{})
                  << " text=" << button->text().toStdString() << '\n';
        return 9;
    }

    window.resize(1'200, 720);
    window.show();
    application.processEvents();
    const QString screenshotPath = application.arguments().at(1);
    QDir().mkpath(QFileInfo(screenshotPath).absolutePath());
    if (!window.grab().save(screenshotPath)) return 10;
    window.hide();
    return 0;
}
