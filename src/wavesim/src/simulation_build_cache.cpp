#include "wave/simulation_build_cache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

#include <algorithm>

namespace wave {
namespace {

QString normalizedPath(const QString& path)
{
    return QDir::fromNativeSeparators(QDir::cleanPath(path));
}

QJsonArray stringArray(const QStringList& values)
{
    QJsonArray result;
    for (const auto& value : values) result.append(value);
    return result;
}

bool exactKeys(
    const QJsonObject& object,
    const QSet<QString>& expected,
    QString& error,
    const QString& context)
{
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
        if (!expected.contains(iterator.key())) {
            error = QStringLiteral("Unknown %1 property: %2")
                        .arg(context, iterator.key());
            return false;
        }
    }
    for (const auto& key : expected) {
        if (!object.contains(key)) {
            error = QStringLiteral("Missing %1 property: %2").arg(context, key);
            return false;
        }
    }
    return true;
}

bool sha256Identity(const QString& value)
{
    if (!value.startsWith(QStringLiteral("sha256:")) || value.size() != 71)
        return false;
    return std::all_of(
        value.cbegin() + 7,
        value.cend(),
        [](const QChar character) {
            return (character >= QLatin1Char('0') && character <= QLatin1Char('9'))
                || (character >= QLatin1Char('a') && character <= QLatin1Char('f'));
        });
}

QJsonObject compileEnvironmentEvidence(const QProcessEnvironment& environment)
{
    static const QStringList keys{
        QStringLiteral("CC"),
        QStringLiteral("CXX"),
        QStringLiteral("CFLAGS"),
        QStringLiteral("CXXFLAGS"),
        QStringLiteral("CPPFLAGS"),
        QStringLiteral("LDFLAGS"),
        QStringLiteral("MAKE"),
        QStringLiteral("MAKEFLAGS"),
        QStringLiteral("PATH"),
        QStringLiteral("SYSTEMC"),
        QStringLiteral("SYSTEMC_HOME"),
        QStringLiteral("SYSTEMC_INCLUDE"),
        QStringLiteral("SYSTEMC_LIBDIR"),
        QStringLiteral("VERILATOR_ROOT"),
        // The deterministic test tool uses this as its produced model identity.
        QStringLiteral("WAVE_SIMULATOR_FIXTURE"),
    };
    QJsonObject result;
    for (const auto& key : keys) {
        if (environment.contains(key)) {
            result.insert(
                key,
                simulationSha256(environment.value(key).toUtf8()));
        }
    }
    return result;
}

QString fingerprintForEvidence(const QJsonObject& evidence)
{
    return simulationSha256(
        QJsonDocument(evidence).toJson(QJsonDocument::Compact));
}

} // namespace

QString simulationSha256(const QByteArray& document)
{
    return QStringLiteral("sha256:")
        + QString::fromLatin1(
            QCryptographicHash::hash(document, QCryptographicHash::Sha256).toHex());
}

SimulationBuildFingerprint computeSimulationBuildFingerprint(
    const SimulationBuildFingerprintInput& input)
{
    QJsonArray sources;
    for (const auto& source : input.sources) {
        sources.append(QJsonObject{
            {QStringLiteral("path"), normalizedPath(source.path)},
            {QStringLiteral("role"), source.role},
            {QStringLiteral("size"), QString::number(source.content.size())},
            {QStringLiteral("sha256"), simulationSha256(source.content)},
        });
    }
    const QJsonObject toolchain{
        {QStringLiteral("verilatorProgram"), normalizedPath(input.verilatorProgram)},
        {QStringLiteral("verilatorVersion"), input.verilatorVersion},
        {QStringLiteral("verilatorArguments"), stringArray(input.verilatorArguments)},
        {QStringLiteral("cxxProgram"), normalizedPath(input.cxxProgram)},
        {QStringLiteral("cxxVersion"), input.cxxVersion},
        {QStringLiteral("cxxFamily"),
         QString::fromLatin1(toString(input.cxxFamily).data())},
        {QStringLiteral("cxxArguments"), stringArray(input.cxxArguments)},
        {QStringLiteral("environment"), compileEnvironmentEvidence(input.environment)},
    };
    QJsonObject evidence{
        {QStringLiteral("contract"),
         QString::fromLatin1(SimulationRuntimeHarnessVersion)},
        {QStringLiteral("manifestSha256"), simulationSha256(input.manifestDocument)},
        {QStringLiteral("harnessSha256"), simulationSha256(input.harnessDocument)},
        {QStringLiteral("sources"), sources},
        {QStringLiteral("toolchain"), toolchain},
    };
    return {fingerprintForEvidence(evidence), std::move(evidence)};
}

QByteArray serializeSimulationBuildCacheRecord(
    const SimulationBuildCacheRecord& record)
{
    const QJsonObject root{
        {QStringLiteral("schema"), QString::fromLatin1(SimulationBuildCacheSchema)},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("fingerprint"), record.build.value},
        {QStringLiteral("evidence"), record.build.evidence},
        {QStringLiteral("executableSha256"), record.executableSha256},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

SimulationBuildCacheRecordParseResult parseSimulationBuildCacheRecord(
    const QByteArray& document)
{
    SimulationBuildCacheRecordParseResult result;
    QJsonParseError parseError;
    const auto parsed = QJsonDocument::fromJson(document, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        result.error = QStringLiteral("Build cache record is not a JSON object: %1")
                           .arg(parseError.errorString());
        return result;
    }
    const auto root = parsed.object();
    if (!exactKeys(
            root,
            {
                QStringLiteral("schema"),
                QStringLiteral("schemaVersion"),
                QStringLiteral("fingerprint"),
                QStringLiteral("evidence"),
                QStringLiteral("executableSha256"),
            },
            result.error,
            QStringLiteral("build cache record"))) {
        return result;
    }
    if (root.value(QStringLiteral("schema")).toString()
            != QString::fromLatin1(SimulationBuildCacheSchema)
        || root.value(QStringLiteral("schemaVersion")).toInt(-1) != 1
        || !root.value(QStringLiteral("evidence")).isObject()) {
        result.error = QStringLiteral("Build cache record schema is unsupported.");
        return result;
    }
    SimulationBuildCacheRecord record;
    record.build.value = root.value(QStringLiteral("fingerprint")).toString();
    record.build.evidence = root.value(QStringLiteral("evidence")).toObject();
    record.executableSha256 =
        root.value(QStringLiteral("executableSha256")).toString();
    if (!sha256Identity(record.build.value)
        || !sha256Identity(record.executableSha256)
        || fingerprintForEvidence(record.build.evidence) != record.build.value) {
        result.error = QStringLiteral("Build cache record digest verification failed.");
        return result;
    }
    result.record = std::move(record);
    return result;
}

} // namespace wave
