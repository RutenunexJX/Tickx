#include "suite_integration.h"

#include "main_window.h"

#include "wave/model.h"
#include "wave/project_io.h"
#include "wave/widgets.h"
#include "wave/session_contract.h"
#include <suiteapp/client.h>

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
    return start(SuiteApp::RuntimeStartOptions{}, failureReason);
}

bool WaveSuiteIntegration::start(
    const SuiteApp::RuntimeStartOptions& runtimeOptions,
    QString* failureReason)
{
    if (isRegistered()) return true;
    const SuiteApp::RuntimeStatus runtime =
        SuiteApp::ensureRuntime(runtimeOptions);
    if (!runtime.available) {
        if (failureReason)
            *failureReason = runtime.errorMessage;
        return false;
    }
    if (window_) {
        auto descriptor = appDescriptor(QCoreApplication::applicationVersion());
        const auto sessionId = window_->automationSessionId();
        const auto providerId = QStringLiteral("wave.session.") + sessionId;
        descriptor.insert("appId", providerId);
        descriptor.insert("applicationId", "wave");
        descriptor.insert("sessionId", sessionId);
        descriptor.insert("endpoint", SuiteApp::endpointForApp(providerId));
        descriptor.insert("resourceSchemes", QJsonArray{});
        descriptor.insert("surfaces", QJsonArray{});
        QJsonArray actions;
        const auto commands = describeSessionAutomation().value("arguments").toObject();
        for (auto it = commands.begin(); it != commands.end(); ++it)
            actions.append(QJsonObject{{"id", "wave.session." + it.key()},
                {"resourceSchemes", QJsonArray{}},
                {"sideEffect", it.key() == "status" ? "none" : "ui"}});
        descriptor.insert("actions", actions);
        sessionProvider_ = std::make_unique<SuiteApp::Provider>(descriptor,
            [this](const QJsonObject& request) { return processRequest(request); }, this);
        if (!sessionProvider_->start(runtimeOptions.endpoint, failureReason)) return false;
        // The SDK registry keys legacy providers by appId. Keep the existing
        // compatibility owner; unique session providers never overwrite it.
        const auto listed = SuiteApp::Client(runtimeOptions.endpoint).listProviders();
        const auto entries = listed.response.value("result").toObject().value("providers").toArray();
        for (const auto& entry : entries)
            if (entry.toObject().value("appId").toString() == "wave") return true;
    }
    provider_ = std::make_unique<SuiteApp::Provider>(
        appDescriptor(QCoreApplication::applicationVersion()),
        [this](const QJsonObject& request) {
            return processRequest(request);
        },
        this);
    const bool legacyStarted = provider_->start(runtimeOptions.endpoint, failureReason);
    return legacyStarted || (sessionProvider_ && sessionProvider_->isListening());
}

bool WaveSuiteIntegration::isRegistered() const
{
    return (sessionProvider_ && sessionProvider_->isListening()) || (provider_ && provider_->isListening());
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
    const auto action = params.value("actionId").toString();
    if (method == "action.invoke" && action.startsWith("wave.session.")) {
        if (!window_) return SuiteApp::errorResponse(request, "session-unavailable", "No window.");
        auto arguments = params.value("arguments").toObject();
        if (arguments.take("sessionId").toString() != window_->automationSessionId())
            return SuiteApp::errorResponse(request, "session-mismatch", "Explicit sessionId does not match this window.");
        auto result = window_->executeSessionCommand(action.mid(13), arguments);
        const bool ok = result.take("ok").toBool();
        if (ok) return SuiteApp::successResponse(request, result);
        const auto error = result.take("error").toObject();
        return SuiteApp::errorResponse(request, error.value("code").toString(),
            error.value("message").toString(), result);
    }
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
