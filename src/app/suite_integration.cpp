#include "suite_integration.h"

#include "main_window.h"

#include "wave/model.h"
#include "wave/project_io.h"
#include "wave/widgets.h"

#include <suiteapp/protocol.h>
#include <suiteapp/provider.h>
#include <suiteapp/runtime.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>

namespace wave {

namespace {

constexpr auto kOpenAction = "wave.project.open";
constexpr auto kWaveformSurface = "wave.waveform";

struct ProjectTarget {
    QString uri;
    QString path;

    bool isValid() const { return !path.trimmed().isEmpty(); }
};

ProjectTarget projectTarget(const QJsonObject& params)
{
    ProjectTarget target;
    target.uri = params.value(QStringLiteral("resourceUri")).toString();
    if (target.uri.isEmpty())
        target.uri = params.value(QStringLiteral("uri")).toString();
    if (!target.uri.isEmpty()) {
        const QUrl url(target.uri, QUrl::StrictMode);
        if (url.isValid()
            && url.scheme().compare(QStringLiteral("wave"),
                                    Qt::CaseInsensitive) == 0
            && url.host().compare(QStringLiteral("project"),
                                  Qt::CaseInsensitive) == 0) {
            target.path = QUrlQuery(url).queryItemValue(
                QStringLiteral("file"), QUrl::FullyDecoded);
        }
    }
    const QJsonObject arguments =
        params.value(QStringLiteral("arguments")).toObject();
    if (!arguments.value(QStringLiteral("projectPath")).toString().isEmpty())
        target.path = arguments.value(QStringLiteral("projectPath")).toString();
    if (!target.path.isEmpty())
        target.path = QFileInfo(target.path).absoluteFilePath();
    return target;
}

QJsonObject projectModel(const QString& uri,
                         const QString& path,
                         const Project& project)
{
    QJsonArray scenarios;
    for (const Scenario& scenario : project.scenarios) {
        scenarios.append(QJsonObject{
            {QStringLiteral("id"), QString::fromStdString(scenario.id)},
            {QStringLiteral("name"), QString::fromStdString(scenario.name)},
            {QStringLiteral("duration"),
             static_cast<double>(scenario.duration)},
            {QStringLiteral("laneCount"),
             static_cast<int>(scenario.lanes.size())},
        });
    }
    return {
        {QStringLiteral("appId"), QStringLiteral("wave")},
        {QStringLiteral("uri"), uri},
        {QStringLiteral("kind"), QStringLiteral("wave-project")},
        {QStringLiteral("projectPath"), path},
        {QStringLiteral("projectId"), QString::fromStdString(project.id)},
        {QStringLiteral("title"), QString::fromStdString(project.name)},
        {QStringLiteral("schemaVersion"), project.schemaVersion},
        {QStringLiteral("scenarios"), scenarios},
        {QStringLiteral("traceCount"),
         static_cast<int>(project.importedTraces.size())},
    };
}

} // namespace

WaveSuiteIntegration::WaveSuiteIntegration(MainWindow* window,
                                           QObject* parent)
    : QObject(parent)
    , window_(window)
{
}

WaveSuiteIntegration::~WaveSuiteIntegration() = default;

bool WaveSuiteIntegration::start(QString* failureReason)
{
    if (provider_ && provider_->isListening())
        return true;
    SuiteApp::RuntimeStartOptions runtimeOptions;
    const SuiteApp::RuntimeStatus runtime =
        SuiteApp::ensureRuntime(runtimeOptions);
    if (!runtime.available) {
        if (failureReason)
            *failureReason = runtime.errorMessage;
        return false;
    }
    provider_ = std::make_unique<SuiteApp::Provider>(
        appDescriptor(QCoreApplication::applicationVersion()),
        [this](const QJsonObject& request) {
            return processRequest(request);
        },
        this);
    return provider_->start(runtimeOptions.endpoint, failureReason);
}

bool WaveSuiteIntegration::isRegistered() const
{
    return provider_ && provider_->isListening();
}

QJsonObject WaveSuiteIntegration::appDescriptor(
    const QString& version,
    const QString& endpoint)
{
    return {
        {QStringLiteral("appId"), QStringLiteral("wave")},
        {QStringLiteral("displayName"), QStringLiteral("Tickx")},
        {QStringLiteral("version"), version.isEmpty()
             ? QStringLiteral("0.0.0") : version},
        {QStringLiteral("processId"),
         static_cast<double>(QCoreApplication::applicationPid())},
        {QStringLiteral("endpoint"), endpoint.isEmpty()
             ? SuiteApp::endpointForApp(QStringLiteral("wave"))
             : endpoint},
        {QStringLiteral("protocols"),
         QJsonArray{QString::fromLatin1(SuiteApp::kProtocol)}},
        {QStringLiteral("resourceSchemes"),
         QJsonArray{QStringLiteral("wave")}},
        {QStringLiteral("actions"),
         QJsonArray{QJsonObject{
             {QStringLiteral("id"), QString::fromLatin1(kOpenAction)},
             {QStringLiteral("resourceSchemes"),
              QJsonArray{QStringLiteral("wave")}},
             {QStringLiteral("sideEffect"), QStringLiteral("ui")}}}},
        {QStringLiteral("surfaces"),
         QJsonArray{QJsonObject{
             {QStringLiteral("id"), QString::fromLatin1(kWaveformSurface)},
             {QStringLiteral("mode"), QStringLiteral("native")},
             {QStringLiteral("fallback"), QStringLiteral("external")},
             {QStringLiteral("native"),
              QJsonObject{
                  {QStringLiteral("library"), QStringLiteral("wavewidgets")},
                  {QStringLiteral("abi"), kWaveWidgetsAbiVersion},
                  {QStringLiteral("factory"),
                   QStringLiteral("wavewidgets_create_simulation_workspace_v1")},
              }}
         }}},
    };
}

QJsonObject WaveSuiteIntegration::processRequestForTesting(
    const QJsonObject& request)
{
    return processRequest(request);
}

QJsonObject WaveSuiteIntegration::processRequest(
    const QJsonObject& request)
{
    const QString method = request.value(QStringLiteral("method")).toString();
    const QJsonObject params =
        request.value(QStringLiteral("params")).toObject();
    const ProjectTarget target = projectTarget(params);
    if (!target.isValid()) {
        return SuiteApp::errorResponse(
            request, QStringLiteral("invalid_resource"),
            QStringLiteral("A wave://project resource is required"));
    }
    const ProjectLoadResult loaded = loadProjectFile(target.path);
    if (!loaded.ok()) {
        return SuiteApp::errorResponse(
            request, QStringLiteral("project_open_failed"), loaded.error,
            {{QStringLiteral("projectPath"), target.path}});
    }
    const QJsonObject model =
        projectModel(target.uri, target.path, *loaded.project);
    if (method == QStringLiteral("resource.resolve"))
        return SuiteApp::successResponse(request, model);

    if (method == QStringLiteral("action.invoke")) {
        if (params.value(QStringLiteral("actionId")).toString()
            != QString::fromLatin1(kOpenAction)) {
            return SuiteApp::errorResponse(
                request, QStringLiteral("action_not_supported"),
                QStringLiteral("Unknown Tickx action"));
        }
        const bool opened = window_ && window_->openSuiteProject(target.path);
        return opened
            ? SuiteApp::successResponse(
                  request, {{QStringLiteral("opened"), true}})
            : SuiteApp::errorResponse(
                  request, QStringLiteral("project_open_cancelled"),
                  QStringLiteral("Tickx did not replace the current project"));
    }

    if (params.value(QStringLiteral("surfaceId")).toString()
        != QString::fromLatin1(kWaveformSurface)) {
        return SuiteApp::errorResponse(
            request, QStringLiteral("surface_not_supported"),
            QStringLiteral("Unknown Tickx surface"));
    }
    if (method == QStringLiteral("surface.describe")) {
        return SuiteApp::successResponse(
            request,
            {{QStringLiteral("surfaceId"),
              QString::fromLatin1(kWaveformSurface)},
             {QStringLiteral("mode"), QStringLiteral("native")},
             {QStringLiteral("fallback"), QStringLiteral("external")},
             {QStringLiteral("native"),
              appDescriptor(QStringLiteral("0.0.0"))
                  .value(QStringLiteral("surfaces")).toArray().first()
                  .toObject().value(QStringLiteral("native"))},
             {QStringLiteral("model"), model}});
    }
    if (method == QStringLiteral("surface.open")) {
        const bool opened = window_ && window_->openSuiteProject(target.path);
        return opened
            ? SuiteApp::successResponse(
                  request, {{QStringLiteral("opened"), true}})
            : SuiteApp::errorResponse(
                  request, QStringLiteral("project_open_cancelled"),
                  QStringLiteral("Tickx did not open the project"));
    }
    return SuiteApp::errorResponse(
        request, QStringLiteral("method_not_supported"),
        QStringLiteral("Tickx does not implement this provider method"));
}

} // namespace wave
