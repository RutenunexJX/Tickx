#include "wave/fst_trace.h"

#include <QElapsedTimer>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>

#include <algorithm>
#include <limits>
#include <set>
#include <sstream>

namespace wave {
namespace {

constexpr auto RequestSchema = "wave-wellen-reader.request/v1";
constexpr auto ResponseSchema = "wave-wellen-reader.response/v1";

struct HelperResponse {
    QJsonObject object;
    std::vector<TraceDiagnostic> diagnostics;
    bool cancelled{false};
    bool valid{false};
};

void addDiagnostic(
    std::vector<TraceDiagnostic>& diagnostics,
    const TraceDiagnosticSeverity severity,
    std::string message)
{
    diagnostics.push_back({severity, 0, std::move(message)});
}

bool hasErrors(const std::vector<TraceDiagnostic>& diagnostics)
{
    return std::any_of(
        diagnostics.begin(),
        diagnostics.end(),
        [](const TraceDiagnostic& diagnostic) {
            return diagnostic.severity == TraceDiagnosticSeverity::Error;
        });
}

std::string sourceFingerprint(const QString& path)
{
    const QFileInfo info(path);
    if (!info.isFile()) return {};
    const auto canonical = info.canonicalFilePath().isEmpty()
        ? info.absoluteFilePath()
        : info.canonicalFilePath();
    return QStringLiteral("%1\n%2\n%3\n%4")
        .arg(canonical)
        .arg(info.size())
        .arg(info.lastModified().toMSecsSinceEpoch())
        .arg(info.fileTime(QFileDevice::FileMetadataChangeTime).toMSecsSinceEpoch())
        .toUtf8()
        .toStdString();
}

std::string diagnosticSummary(const std::vector<TraceDiagnostic>& diagnostics)
{
    std::ostringstream output;
    bool first = true;
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.severity != TraceDiagnosticSeverity::Error) continue;
        if (!first) output << '\n';
        output << diagnostic.message;
        first = false;
    }
    return output.str();
}

TraceDiagnosticSeverity diagnosticSeverity(const QString& text)
{
    if (text.compare(QStringLiteral("warning"), Qt::CaseInsensitive) == 0) {
        return TraceDiagnosticSeverity::Warning;
    }
    if (text.compare(QStringLiteral("information"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("info"), Qt::CaseInsensitive) == 0) {
        return TraceDiagnosticSeverity::Information;
    }
    return TraceDiagnosticSeverity::Error;
}

void appendResponseDiagnostics(
    const QJsonObject& object,
    std::vector<TraceDiagnostic>& diagnostics)
{
    const auto values = object.value(QStringLiteral("diagnostics"));
    if (!values.isArray()) return;
    for (const auto& value : values.toArray()) {
        if (!value.isObject()) continue;
        const auto item = value.toObject();
        addDiagnostic(
            diagnostics,
            diagnosticSeverity(item.value(QStringLiteral("severity")).toString()),
            item.value(QStringLiteral("message")).toString().toStdString());
    }
}

HelperResponse invokeReader(
    const QString& executable,
    const QJsonObject& request,
    const TraceParseOptions& options,
    const FstTraceReaderLimits& limits)
{
    HelperResponse response;
    if (executable.isEmpty() || !QFileInfo(executable).isFile()) {
        addDiagnostic(
            response.diagnostics,
            TraceDiagnosticSeverity::Error,
            "Wellen reader executable is missing: " + executable.toStdString());
        return response;
    }
    if (options.isCancelled && options.isCancelled()) {
        response.cancelled = true;
        return response;
    }
    if (limits.maxResponseBytes <= 0 || limits.timeoutMs <= 0) {
        addDiagnostic(
            response.diagnostics,
            TraceDiagnosticSeverity::Error,
            "Invalid Wellen reader resource limits.");
        return response;
    }

    QProcess process;
    process.setProgram(executable);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(QIODevice::ReadWrite);
    if (!process.waitForStarted(10'000)) {
        addDiagnostic(
            response.diagnostics,
            TraceDiagnosticSeverity::Error,
            "Failed to start Wellen reader: " + process.errorString().toStdString());
        return response;
    }

    const auto payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
    if (process.write(payload) != payload.size()) {
        process.kill();
        process.waitForFinished();
        addDiagnostic(
            response.diagnostics,
            TraceDiagnosticSeverity::Error,
            "Failed to send the complete request to the Wellen reader.");
        return response;
    }
    process.closeWriteChannel();

    QByteArray standardOutput;
    QByteArray standardError;
    QElapsedTimer timer;
    timer.start();
    while (process.state() != QProcess::NotRunning) {
        process.waitForFinished(25);
        standardOutput += process.readAllStandardOutput();
        standardError += process.readAllStandardError();
        if (standardOutput.size() > limits.maxResponseBytes) {
            process.kill();
            process.waitForFinished();
            addDiagnostic(
                response.diagnostics,
                TraceDiagnosticSeverity::Error,
                "Wellen reader response exceeded the configured size limit.");
            return response;
        }
        if (options.isCancelled && options.isCancelled()) {
            process.kill();
            process.waitForFinished();
            response.cancelled = true;
            return response;
        }
        if (timer.elapsed() > limits.timeoutMs) {
            process.kill();
            process.waitForFinished();
            addDiagnostic(
                response.diagnostics,
                TraceDiagnosticSeverity::Error,
                "Wellen reader timed out.");
            return response;
        }
    }
    standardOutput += process.readAllStandardOutput();
    standardError += process.readAllStandardError();
    if (standardOutput.size() > limits.maxResponseBytes) {
        addDiagnostic(
            response.diagnostics,
            TraceDiagnosticSeverity::Error,
            "Wellen reader response exceeded the configured size limit.");
        return response;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(standardOutput, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        const auto detail = standardError.trimmed().isEmpty()
            ? parseError.errorString()
            : QString::fromUtf8(standardError.trimmed());
        addDiagnostic(
            response.diagnostics,
            TraceDiagnosticSeverity::Error,
            "Wellen reader returned invalid JSON: " + detail.toStdString());
        return response;
    }
    response.object = document.object();
    appendResponseDiagnostics(response.object, response.diagnostics);
    if (response.object.value(QStringLiteral("schema")).toString()
            != QString::fromLatin1(ResponseSchema)) {
        addDiagnostic(
            response.diagnostics,
            TraceDiagnosticSeverity::Error,
            "Wellen reader returned an unsupported response schema.");
        return response;
    }
    if (response.object.value(QStringLiteral("format")).toString()
            != QStringLiteral("fst")) {
        addDiagnostic(
            response.diagnostics,
            TraceDiagnosticSeverity::Error,
            "Wellen reader response is not an FST trace.");
        return response;
    }
    if (!response.object.value(QStringLiteral("ok")).toBool(false)
        || process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        if (!hasErrors(response.diagnostics)) {
            addDiagnostic(
                response.diagnostics,
                TraceDiagnosticSeverity::Error,
                "Wellen reader failed without a diagnostic.");
        }
        return response;
    }
    response.valid = true;
    return response;
}

QJsonObject makeRequest(
    const QString& operation,
    const QString& path,
    const TraceParseOptions& options,
    std::span<const std::string> signalIds)
{
    QJsonArray ids;
    for (const auto& id : signalIds) ids.append(QString::fromStdString(id));
    QJsonObject request{
        {QStringLiteral("schema"), QString::fromLatin1(RequestSchema)},
        {QStringLiteral("operation"), operation},
        {QStringLiteral("path"), QFileInfo(path).absoluteFilePath()},
        {QStringLiteral("projectTickPs"), options.projectTimeBase.picosecondsPerTick},
        {QStringLiteral("offset"), options.offset},
        {QStringLiteral("signalIds"), ids},
    };
    return request;
}

bool parseStringArray(
    const QJsonValue& value,
    std::vector<std::string>& output)
{
    if (!value.isArray()) return false;
    for (const auto& item : value.toArray()) {
        if (!item.isString()) return false;
        output.push_back(item.toString().toStdString());
    }
    return true;
}

bool parseSignal(
    const QJsonValue& value,
    const bool expectLoaded,
    TraceSignal& signal,
    std::string& error)
{
    if (!value.isObject()) {
        error = "Wellen reader signal entry is not an object.";
        return false;
    }
    const auto object = value.toObject();
    signal.id = object.value(QStringLiteral("id")).toString().toStdString();
    signal.identifierCode = object.value(QStringLiteral("identifierCode")).toString().toStdString();
    signal.scope = object.value(QStringLiteral("scope")).toString().toStdString();
    signal.reference = object.value(QStringLiteral("reference")).toString().toStdString();
    signal.fullName = object.value(QStringLiteral("fullName")).toString().toStdString();
    const auto width = object.value(QStringLiteral("width")).toInteger(0);
    signal.transitionsLoaded = object.value(QStringLiteral("transitionsLoaded")).toBool(false);
    if (signal.id.empty() || signal.reference.empty() || signal.fullName.empty()
        || width <= 0 || width > std::numeric_limits<std::uint32_t>::max()
        || signal.transitionsLoaded != expectLoaded
        || !parseStringArray(object.value(QStringLiteral("scopePath")), signal.scopePath)) {
        error = "Wellen reader returned invalid signal metadata.";
        return false;
    }
    signal.width = static_cast<std::uint32_t>(width);
    const auto transitions = object.value(QStringLiteral("transitions"));
    if (!transitions.isUndefined() && !transitions.isArray()) {
        error = "Wellen reader signal transitions are not an array.";
        return false;
    }
    Tick previous = std::numeric_limits<Tick>::min();
    if (transitions.isArray()) {
        for (const auto& item : transitions.toArray()) {
            if (!item.isObject()) {
                error = "Wellen reader transition entry is not an object.";
                return false;
            }
            const auto transition = item.toObject();
            const auto tick = transition.value(QStringLiteral("tick")).toInteger();
            const auto valueText = transition.value(QStringLiteral("value")).toString();
            if (valueText.isEmpty() || (!signal.transitions.empty() && tick <= previous)) {
                error = "Wellen reader returned invalid transition ordering or value.";
                return false;
            }
            signal.transitions.push_back({tick, valueText.toStdString()});
            previous = tick;
        }
    }
    if (!expectLoaded && !signal.transitions.empty()) {
        error = "FST metadata response unexpectedly contains transitions.";
        return false;
    }
    return true;
}

} // namespace

bool TraceSignalLoadResult::ok() const noexcept
{
    return !cancelled && !hasErrors(diagnostics);
}

std::string TraceSignalLoadResult::errorSummary() const
{
    return diagnosticSummary(diagnostics);
}

TraceParseResult readFstMetadataFile(
    const QString& path,
    const TraceParseOptions& options,
    const QString& readerExecutable,
    const FstTraceReaderLimits& limits)
{
    TraceParseResult result;
    if (!options.projectTimeBase.isValid()) {
        addDiagnostic(
            result.diagnostics,
            TraceDiagnosticSeverity::Error,
            "Project timebase is invalid.");
        return result;
    }
    const auto fingerprintBefore = sourceFingerprint(path);
    if (fingerprintBefore.empty()) {
        addDiagnostic(
            result.diagnostics,
            TraceDiagnosticSeverity::Error,
            "FST source file is missing or inaccessible.");
        return result;
    }
    const auto response = invokeReader(
        readerExecutable,
        makeRequest(QStringLiteral("metadata"), path, options, {}),
        options,
        limits);
    result.diagnostics = response.diagnostics;
    result.cancelled = response.cancelled;
    if (!response.valid) return result;
    const auto fingerprintAfter = sourceFingerprint(path);
    if (fingerprintAfter.empty() || fingerprintAfter != fingerprintBefore) {
        addDiagnostic(
            result.diagnostics,
            TraceDiagnosticSeverity::Error,
            "FST source file changed while metadata was being read.");
        return result;
    }

    const auto signalCount = response.object.value(QStringLiteral("signalCount")).toInteger(-1);
    const auto transitionCount = response.object.value(QStringLiteral("transitionCount")).toInteger(-1);
    const auto values = response.object.value(QStringLiteral("signals"));
    if (signalCount < 0
        || static_cast<quint64>(signalCount) > limits.maxMetadataSignals
        || transitionCount != 0 || !values.isArray()
        || values.toArray().size() != signalCount) {
        addDiagnostic(
            result.diagnostics,
            TraceDiagnosticSeverity::Error,
            "Wellen reader metadata violates the configured scale or metadata-only contract.");
        return result;
    }

    TraceIndex index;
    index.identity = options.identity;
    index.format = TraceFormat::Fst;
    index.projectTimeBase = options.projectTimeBase;
    index.startTick = response.object.value(QStringLiteral("startTick")).toInteger(options.offset);
    index.endTick = response.object.value(QStringLiteral("endTick")).toInteger(index.startTick);
    index.sourceFingerprint = fingerprintAfter;
    std::set<std::string> ids;
    for (const auto& value : values.toArray()) {
        TraceSignal signal;
        std::string error;
        if (!parseSignal(value, false, signal, error) || !ids.insert(signal.id).second) {
            addDiagnostic(
                result.diagnostics,
                TraceDiagnosticSeverity::Error,
                error.empty() ? "Wellen reader returned duplicate signal IDs." : error);
            return result;
        }
        index.traceSignals.push_back(std::move(signal));
    }
    if (index.traceSignals.empty()) {
        addDiagnostic(
            result.diagnostics,
            TraceDiagnosticSeverity::Error,
            "FST contains no supported digital signals.");
        return result;
    }
    result.index = std::move(index);
    return result;
}

TraceSignalLoadResult loadFstSignalsFile(
    const QString& path,
    const TraceParseOptions& options,
    const QString& readerExecutable,
    const std::span<const std::string> signalIds,
    const FstTraceReaderLimits& limits)
{
    TraceSignalLoadResult result;
    result.identity = options.identity;
    if (signalIds.empty() || signalIds.size() > limits.maxSignalsPerRequest) {
        addDiagnostic(
            result.diagnostics,
            TraceDiagnosticSeverity::Error,
            "FST signal load request is empty or exceeds the configured batch limit.");
        return result;
    }
    const auto fingerprintBefore = sourceFingerprint(path);
    if (fingerprintBefore.empty()) {
        addDiagnostic(
            result.diagnostics,
            TraceDiagnosticSeverity::Error,
            "FST source file is missing or inaccessible.");
        return result;
    }
    std::set<std::string> requested;
    for (const auto& id : signalIds) {
        if (id.empty() || !requested.insert(id).second) {
            addDiagnostic(
                result.diagnostics,
                TraceDiagnosticSeverity::Error,
                "FST signal load request contains an empty or duplicate signal ID.");
            return result;
        }
    }
    const auto response = invokeReader(
        readerExecutable,
        makeRequest(QStringLiteral("load"), path, options, signalIds),
        options,
        limits);
    result.diagnostics = response.diagnostics;
    result.cancelled = response.cancelled;
    if (!response.valid) return result;
    const auto fingerprintAfter = sourceFingerprint(path);
    if (fingerprintAfter.empty() || fingerprintAfter != fingerprintBefore) {
        addDiagnostic(
            result.diagnostics,
            TraceDiagnosticSeverity::Error,
            "FST source file changed while signals were being read.");
        return result;
    }
    result.sourceFingerprint = fingerprintAfter;

    const auto values = response.object.value(QStringLiteral("signals"));
    if (!values.isArray() || values.toArray().size() != static_cast<qsizetype>(signalIds.size())) {
        addDiagnostic(
            result.diagnostics,
            TraceDiagnosticSeverity::Error,
            "Wellen reader returned a different signal batch than requested.");
        return result;
    }
    std::uint64_t transitionCount = 0;
    for (const auto& value : values.toArray()) {
        TraceSignal signal;
        std::string error;
        if (!parseSignal(value, true, signal, error)
            || !requested.erase(signal.id)) {
            addDiagnostic(
                result.diagnostics,
                TraceDiagnosticSeverity::Error,
                error.empty() ? "Wellen reader returned an unexpected signal ID." : error);
            return result;
        }
        transitionCount += signal.transitions.size();
        result.loadedSignals.push_back(std::move(signal));
    }
    const auto reported = response.object.value(QStringLiteral("transitionCount")).toInteger(-1);
    if (!requested.empty() || reported < 0
        || static_cast<std::uint64_t>(reported) != transitionCount) {
        addDiagnostic(
            result.diagnostics,
            TraceDiagnosticSeverity::Error,
            "Wellen reader transition count does not match the loaded batch.");
    }
    return result;
}

bool mergeLoadedTraceSignals(
    TraceIndex& index,
    const TraceSignalLoadResult& loaded,
    std::string* error)
{
    const auto fail = [error](std::string message) {
        if (error) *error = std::move(message);
        return false;
    };
    if (index.format != TraceFormat::Fst || !loaded.ok()) {
        return fail("Only successful FST batches can be merged.");
    }
    if (index.identity != loaded.identity) {
        return fail("FST signal batch identity does not match the active trace.");
    }
    if (index.sourceFingerprint.empty()
        || index.sourceFingerprint != loaded.sourceFingerprint) {
        return fail("FST source file changed between metadata and signal loading.");
    }
    std::set<std::string> ids;
    for (const auto& incoming : loaded.loadedSignals) {
        if (!incoming.transitionsLoaded || !ids.insert(incoming.id).second) {
            return fail("FST signal batch contains invalid or duplicate signals.");
        }
        auto* target = index.findSignal(incoming.id);
        if (!target) return fail("FST signal batch references unknown metadata.");
        if (target->fullName != incoming.fullName
            || target->scopePath != incoming.scopePath
            || target->reference != incoming.reference
            || target->width != incoming.width) {
            return fail("FST signal metadata changed between metadata and signal loading.");
        }
    }
    for (const auto& incoming : loaded.loadedSignals) {
        auto* target = index.findSignal(incoming.id);
        target->transitions = incoming.transitions;
        target->transitionsLoaded = true;
    }
    index.transitionCount = 0;
    for (const auto& signal : index.traceSignals) {
        index.transitionCount += signal.transitions.size();
    }
    return true;
}

} // namespace wave
