#include "session_cli.h"
#include "wave/session_contract.h"

#ifdef WAVEWORKBENCH_HAS_SUITEAPP
#include <suiteapp/client.h>
#include <suiteapp/protocol.h>
#endif

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTextStream>
#include <QThread>
#include <QUuid>

#include <cstdio>

QJsonObject sessionCliCapabilities()
{
    auto value = wave::describeSessionAutomation();
#ifdef WAVEWORKBENCH_HAS_SUITEAPP
    value.insert("available", true);
#else
    value.insert("available", false);
#endif
    return value;
}

int runSessionCli(QStringList arguments)
{
    QString command;
    bool pretty = false;
    auto emitResult = [&](QJsonObject object, int code) {
        object.insert("schema", "wave-workbench.session/v1");
        object.insert("command", "session." + command);
        QTextStream(stdout) << QJsonDocument(object).toJson(
            pretty ? QJsonDocument::Indented : QJsonDocument::Compact) << '\n';
        return code;
    };
    auto fail = [&](const QString& code, const QString& message, int exitCode) {
        return emitResult({{"ok", false},
            {"error", QJsonObject{{"code", code}, {"message", message}}}}, exitCode);
    };
    if (arguments.isEmpty() || arguments.front() == "--help" || arguments.front() == "-h") {
        QTextStream(stdout)
            << "Usage:\n"
               "  wave-cli session list [--runtime=<endpoint>] [--pretty]\n"
               "  wave-cli session <status|open|refresh|view|apply|save|undo|redo>\n"
               "    --session=<id> [--runtime=<endpoint>] [--args=<json-file|->]\n"
               "    [--project=<path>] [--ops=<json-file|->] [--scenario=<id>]\n"
               "    [--cursor=<time>] [--start=<time> --end=<time>]\n"
               "    [--expect-model-sha256=<sha>] [--expect-sha256=<sha|missing>]\n"
               "    [--dry-run] [--wait-ms=<0..30000>] [--timeout-ms=<100..30000>]\n"
               "  wave-cli session schema [--pretty]\n"
               "Use schema for argument fields, revision requirements and exit codes.\n";
        return arguments.isEmpty() ? 2 : 0;
    }
    command = arguments.takeFirst();
    QString runtime, sessionId, argsPath, opsPath;
    QJsonObject params;
    [[maybe_unused]] int waitMs = 0, timeoutMs = 3000;
    QSet<QString> options;
    const QMap<QString, QString> fields{
        {"project", "projectPath"}, {"scenario", "scenarioId"}, {"cursor", "cursor"},
        {"start", "start"}, {"end", "end"}, {"expect-model-sha256", "expectedModelSha256"},
        {"expect-sha256", "expectedDiskSha256"}};
    for (const auto& argument : arguments) {
        if (!argument.startsWith("--")) return fail("usage", "Expected a named option.", 2);
        const auto separator = argument.indexOf('=');
        const auto key = argument.mid(2, separator < 0 ? -1 : separator - 2);
        const auto value = separator < 0 ? QString{} : argument.mid(separator + 1);
        if (options.contains(key)) return fail("usage", "Duplicate option: " + key, 2);
        options.insert(key);
        if (key == "pretty" && separator < 0) { pretty = true; continue; }
        if (key == "dry-run" && separator < 0) { params.insert("dryRun", true); continue; }
        if (separator < 0 || value.isEmpty()) return fail("usage", "Use --name=value with a nonempty value.", 2);
        if (key == "runtime") runtime = value;
        else if (key == "session") sessionId = value;
        else if (key == "args") argsPath = value;
        else if (key == "ops") opsPath = value;
        else if (fields.contains(key)) params.insert(fields.value(key), value);
        else if (key == "wait-ms" || key == "timeout-ms") {
            bool valid = false;
            const int milliseconds = value.toInt(&valid);
            if (!valid || milliseconds < (key == "wait-ms" ? 0 : 100) || milliseconds > 30000)
                return fail("usage", "Invalid time limit.", 2);
            if (key == "wait-ms") waitMs = milliseconds;
            else timeoutMs = milliseconds;
        } else return fail("usage", "Unknown option: " + key, 2);
    }
    if (command == "schema") {
        if (options.size() > (pretty ? 1 : 0)) return fail("usage", "schema accepts only --pretty.", 2);
        return emitResult({{"ok", true}, {"result", sessionCliCapabilities()}}, 0);
    }
    const auto definitions = wave::describeSessionAutomation().value("arguments").toObject();
    if (command != "list" && !definitions.contains(command))
        return fail("usage", "Unknown session command.", 2);
    if (command == "list") {
        for (const auto& key : options)
            if (key != "runtime" && key != "pretty" && key != "timeout-ms")
                return fail("usage", "list accepts only --runtime, --timeout-ms and --pretty.", 2);
    } else if (sessionId.isEmpty() || QUuid(sessionId).isNull()) {
        return fail("usage", "An explicit --session=<id> from session list is required.", 2);
    }
    if (options.contains("wait-ms") && command != "refresh" && command != "open")
        return fail("usage", "--wait-ms is supported only by open and refresh.", 2);
    if (!opsPath.isEmpty() && command != "apply")
        return fail("usage", "--ops is supported only by apply.", 2);
    if (argsPath == "-" && opsPath == "-")
        return fail("usage", "Only one input may read stdin.", 2);
    auto readJson = [&](const QString& path, QJsonObject& object, QString& error) {
        QFile file;
        if (path == "-") {
            if (!file.open(stdin, QIODevice::ReadOnly)) { error = file.errorString(); return false; }
        } else {
            file.setFileName(path);
            if (!file.open(QIODevice::ReadOnly)) { error = file.errorString(); return false; }
        }
        const auto bytes = file.read(524289);
        if (bytes.size() > 524288) { error = "Input exceeds 512 KiB."; return false; }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            error = "Expected a JSON object: " + parseError.errorString(); return false;
        }
        object = document.object();
        return true;
    };
    QString error;
    if (!argsPath.isEmpty()) {
        QJsonObject object;
        if (!readJson(argsPath, object, error)) return fail("input", error, 3);
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (params.contains(it.key())) return fail("usage", "Duplicate argument: " + it.key(), 2);
            params.insert(it.key(), it.value());
        }
    }
    if (!opsPath.isEmpty()) {
        if (params.contains("batch")) return fail("usage", "Duplicate batch.", 2);
        QJsonObject batch;
        if (!readJson(opsPath, batch, error)) return fail("input", error, 3);
        params.insert("batch", batch);
    }
    if (params.contains("projectPath") && params.value("projectPath").isString())
        params.insert("projectPath", QFileInfo(params.value("projectPath").toString()).absoluteFilePath());
    if (command != "list") {
        const auto allowed = definitions.value(command).toArray();
        for (auto it = params.begin(); it != params.end(); ++it)
            if (!allowed.contains(it.key()))
                return fail("usage", "Unexpected argument for " + command + ": " + it.key(), 2);
    }
#ifndef WAVEWORKBENCH_HAS_SUITEAPP
    return fail("session-unavailable", "This build has no SuiteApp SDK; offline commands remain available.", 3);
#else
    if (runtime.isEmpty()) runtime = SuiteApp::defaultRuntimeEndpoint();
    const SuiteApp::Client client(runtime, timeoutMs);
    if (command == "list") {
        const auto listed = client.listProviders();
        if (!listed.hasResponse())
            return fail(listed.errorCode, listed.errorMessage, 3);
        if (!listed.response.value("ok").toBool())
            return emitResult({{"ok", false}, {"error", listed.response.value("error")}}, 3);
        QJsonArray sessions;
        const auto entries = listed.response.value("result").toObject().value("providers").toArray();
        for (const auto& entry : entries) {
            const auto descriptor = entry.toObject();
            const auto id = descriptor.value("sessionId").toString();
            if (descriptor.value("applicationId").toString() != "wave" || id.isEmpty()) continue;
            const auto providerId = descriptor.value("appId").toString();
            const auto response = client.invokeAction("wave.session.status", {{"sessionId", id}}, {}, providerId);
            QJsonObject item{{"sessionId", id}, {"providerId", providerId}};
            const bool reachable = response.hasResponse() && response.response.value("ok").toBool();
            item.insert("reachable", reachable);
            if (reachable) item.insert("state", response.response.value("result").toObject().value("state"));
            else item.insert("error", response.response.value("error").isObject()
                ? response.response.value("error")
                : QJsonObject{{"code", response.errorCode}, {"message", response.errorMessage}});
            sessions.append(item);
        }
        return emitResult({{"ok", true}, {"result", QJsonObject{{"sessions", sessions}}}}, 0);
    }
    sessionId = QUuid(sessionId).toString(QUuid::WithoutBraces);
    params.insert("sessionId", sessionId);
    if (QJsonDocument(params).toJson(QJsonDocument::Compact).size() > 524288)
        return fail("input-too-large", "Combined arguments exceed 512 KiB.", 3);
    const auto providerId = "wave.session." + sessionId;
    QElapsedTimer timer;
    timer.start();
    QString operation = command;
    QJsonObject currentParams = params;
    for (;;) {
        const auto response = client.invokeAction("wave.session." + operation, currentParams, {}, providerId);
        if (!response.hasResponse()) return fail(response.errorCode,
            response.errorMessage + " Inspect status before retrying a mutation.", 3);
        if (!response.response.value("ok").toBool())
            return emitResult({{"ok", false}, {"error", response.response.value("error")}}, 4);
        const auto result = response.response.value("result").toObject();
        const auto state = result.value("state").toObject();
        const bool adoption = command == "open" || command == "refresh";
        const bool current = state.value("adoption").toString() == "current";
        if (!adoption || current)
            return emitResult({{"ok", true}, {"result", result}}, 0);
        if (timer.elapsed() >= waitMs)
            return emitResult({{"ok", true}, {"result", result},
                {"adoptionPending", true}}, 6);
        QThread::msleep(static_cast<unsigned long>(std::clamp<qint64>(waitMs - timer.elapsed(), 1, 100)));
        operation = "refresh";
        currentParams = {{"sessionId", sessionId}};
        if (params.contains("expectedDiskSha256"))
            currentParams.insert("expectedDiskSha256", params.value("expectedDiskSha256"));
    }
#endif
}
