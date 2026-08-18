#include "wave/module_manifest.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace wave {
namespace {

std::string stdString(const QString& value)
{
    return value.toUtf8().toStdString();
}

QString qString(const std::string& value)
{
    return QString::fromUtf8(value);
}

std::string jsonStringValue(const std::string& value)
{
    QJsonArray array;
    array.append(qString(value));
    auto encoded = QJsonDocument(array).toJson(QJsonDocument::Compact);
    encoded.remove(0, 1);
    encoded.chop(1);
    return encoded.toStdString();
}

std::string jsonStringArrayValue(const std::vector<std::string>& values)
{
    QJsonArray array;
    for (const auto& value : values) array.append(qString(value));
    return QJsonDocument(array).toJson(QJsonDocument::Compact).toStdString();
}

bool exactKeys(
    const QJsonObject& object,
    const std::initializer_list<QString> required,
    const QString& context,
    QString& error)
{
    QSet<QString> allowed;
    for (const auto& key : required) {
        allowed.insert(key);
        if (!object.contains(key)) {
            error = QStringLiteral("%1.%2 is required").arg(context, key);
            return false;
        }
    }
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
        if (!allowed.contains(iterator.key())) {
            error = QStringLiteral("%1 contains unsupported property %2")
                        .arg(context, iterator.key());
            return false;
        }
    }
    return true;
}

bool readString(
    const QJsonObject& object,
    const QString& name,
    const QString& context,
    std::string& output,
    QString& error,
    const bool allowEmpty = true)
{
    const auto value = object.value(name);
    if (!value.isString() || (!allowEmpty && value.toString().isEmpty())) {
        error = QStringLiteral("%1.%2 must be %3 string")
                    .arg(
                        context,
                        name,
                        allowEmpty ? QStringLiteral("a")
                                   : QStringLiteral("a non-empty"));
        return false;
    }
    output = stdString(value.toString());
    return true;
}

bool readBool(
    const QJsonObject& object,
    const QString& name,
    const QString& context,
    bool& output,
    QString& error)
{
    const auto value = object.value(name);
    if (!value.isBool()) {
        error = QStringLiteral("%1.%2 must be boolean").arg(context, name);
        return false;
    }
    output = value.toBool();
    return true;
}

bool readInteger(
    const QJsonObject& object,
    const QString& name,
    const QString& context,
    std::uint64_t& output,
    QString& error,
    const std::uint64_t minimum = 0)
{
    const auto value = object.value(name);
    const auto number = value.toDouble(-1.0);
    if (!value.isDouble() || !std::isfinite(number) || number < 0.0
        || std::floor(number) != number
        || number > 9'007'199'254'740'991.0
        || number < static_cast<double>(minimum)) {
        error = QStringLiteral("%1.%2 must be an integer of at least %3")
                    .arg(context, name)
                    .arg(minimum);
        return false;
    }
    output = static_cast<std::uint64_t>(number);
    return true;
}

bool readPositiveInt(
    const QJsonObject& object,
    const QString& name,
    const QString& context,
    int& output,
    QString& error)
{
    std::uint64_t value = 0;
    if (!readInteger(object, name, context, value, error, 1)
        || value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        if (error.isEmpty()) {
            error = QStringLiteral("%1.%2 exceeds the supported integer range")
                        .arg(context, name);
        }
        return false;
    }
    output = static_cast<int>(value);
    return true;
}

bool relativeProjectPath(const std::string& path)
{
    const auto text = qString(path);
    if (text.isEmpty() || text.contains(QLatin1Char('\\'))
        || QDir::isAbsolutePath(text)
        || QRegularExpression(QStringLiteral("^[A-Za-z]:")).match(text).hasMatch()) {
        return false;
    }
    const auto parts = text.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    return std::none_of(parts.begin(), parts.end(), [](const QString& part) {
        return part.isEmpty() || part == QStringLiteral("..");
    });
}

bool readRelativePath(
    const QJsonObject& object,
    const QString& name,
    const QString& context,
    std::string& output,
    QString& error)
{
    if (!readString(object, name, context, output, error, false)) return false;
    if (!relativeProjectPath(output)) {
        error = QStringLiteral("%1.%2 must be a normalized workspace-relative path")
                    .arg(context, name);
        return false;
    }
    return true;
}

bool readStringArray(
    const QJsonValue& value,
    const QString& context,
    std::vector<std::string>& output,
    QString& error,
    const bool nonEmptyEntries,
    const bool uniqueEntries,
    const bool paths)
{
    if (!value.isArray()) {
        error = QStringLiteral("%1 must be an array").arg(context);
        return false;
    }
    QSet<QString> seen;
    for (qsizetype index = 0; index < value.toArray().size(); ++index) {
        const auto item = value.toArray().at(index);
        if (!item.isString()
            || (nonEmptyEntries && item.toString().isEmpty())) {
            error = QStringLiteral("%1[%2] must be %3 string")
                        .arg(context)
                        .arg(index)
                        .arg(nonEmptyEntries ? QStringLiteral("a non-empty")
                                             : QStringLiteral("a"));
            return false;
        }
        const auto text = item.toString();
        if (paths && !relativeProjectPath(stdString(text))) {
            error = QStringLiteral("%1[%2] must be a normalized workspace-relative path")
                        .arg(context)
                        .arg(index);
            return false;
        }
        if (uniqueEntries && seen.contains(text)) {
            error = QStringLiteral("%1 contains duplicate value %2")
                        .arg(context, text);
            return false;
        }
        seen.insert(text);
        output.push_back(stdString(text));
    }
    return true;
}

bool parseTypeShape(
    const QJsonObject& object,
    const QString& context,
    ModuleManifestTypeShape& shape,
    QString& error)
{
    if (!exactKeys(
            object,
            {QStringLiteral("semanticAvailable"),
             QStringLiteral("rawTypeText"),
             QStringLiteral("resolvedTypeName"),
             QStringLiteral("semanticKind"),
             QStringLiteral("resolvedTypeText"),
             QStringLiteral("canonicalTypeId"),
             QStringLiteral("declarationShapeId"),
             QStringLiteral("fixedSize"),
             QStringLiteral("integral"),
             QStringLiteral("signed"),
             QStringLiteral("unpackedArray"),
             QStringLiteral("interfaceType"),
             QStringLiteral("bitWidth"),
             QStringLiteral("packedDimensions"),
             QStringLiteral("unpackedDimensions"),
             QStringLiteral("unpackedElementCount"),
             QStringLiteral("interfaceName"),
             QStringLiteral("modportName"),
             QStringLiteral("typedefChain"),
             QStringLiteral("failureReason")},
            context,
            error)) {
        return false;
    }
    return readBool(object, QStringLiteral("semanticAvailable"), context,
                    shape.semanticAvailable, error)
        && readString(object, QStringLiteral("rawTypeText"), context,
                      shape.rawTypeText, error)
        && readString(object, QStringLiteral("resolvedTypeName"), context,
                      shape.resolvedTypeName, error)
        && readString(object, QStringLiteral("semanticKind"), context,
                      shape.semanticKind, error)
        && readString(object, QStringLiteral("resolvedTypeText"), context,
                      shape.resolvedTypeText, error)
        && readString(object, QStringLiteral("canonicalTypeId"), context,
                      shape.canonicalTypeId, error)
        && readString(object, QStringLiteral("declarationShapeId"), context,
                      shape.declarationShapeId, error)
        && readBool(object, QStringLiteral("fixedSize"), context,
                    shape.fixedSize, error)
        && readBool(object, QStringLiteral("integral"), context,
                    shape.integral, error)
        && readBool(object, QStringLiteral("signed"), context,
                    shape.isSigned, error)
        && readBool(object, QStringLiteral("unpackedArray"), context,
                    shape.unpackedArray, error)
        && readBool(object, QStringLiteral("interfaceType"), context,
                    shape.interfaceType, error)
        && readInteger(object, QStringLiteral("bitWidth"), context,
                       shape.bitWidth, error)
        && readString(object, QStringLiteral("packedDimensions"), context,
                      shape.packedDimensions, error)
        && readString(object, QStringLiteral("unpackedDimensions"), context,
                      shape.unpackedDimensions, error)
        && readString(object, QStringLiteral("unpackedElementCount"), context,
                      shape.unpackedElementCount, error)
        && readString(object, QStringLiteral("interfaceName"), context,
                      shape.interfaceName, error)
        && readString(object, QStringLiteral("modportName"), context,
                      shape.modportName, error)
        && readStringArray(object.value(QStringLiteral("typedefChain")),
                           context + QStringLiteral(".typedefChain"),
                           shape.typedefChain, error, false, false, false)
        && readString(object, QStringLiteral("failureReason"), context,
                      shape.failureReason, error);
}

bool parseType(
    const QJsonObject& object,
    const QString& context,
    ModuleManifestType& type,
    QString& error)
{
    QJsonObject shapeObject = object;
    const auto enumValues = shapeObject.take(QStringLiteral("enumValues"));
    const auto structMembers = shapeObject.take(QStringLiteral("structMembers"));
    if (enumValues.isUndefined() || structMembers.isUndefined()) {
        error = QStringLiteral("%1 requires enumValues and structMembers")
                    .arg(context);
        return false;
    }
    if (!parseTypeShape(shapeObject, context, type.shape, error)) return false;
    if (!enumValues.isArray()) {
        error = QStringLiteral("%1.enumValues must be an array").arg(context);
        return false;
    }
    QSet<QString> enumNames;
    for (qsizetype index = 0; index < enumValues.toArray().size(); ++index) {
        const auto value = enumValues.toArray().at(index);
        const auto entryContext = QStringLiteral("%1.enumValues[%2]")
                                      .arg(context)
                                      .arg(index);
        if (!value.isObject()) {
            error = entryContext + QStringLiteral(" must be an object");
            return false;
        }
        const auto entry = value.toObject();
        if (!exactKeys(
                entry,
                {QStringLiteral("name"), QStringLiteral("declarationText"),
                 QStringLiteral("valueText"), QStringLiteral("displayValueText"),
                 QStringLiteral("semanticAvailable")},
                entryContext,
                error)) {
            return false;
        }
        ModuleManifestEnumValue parsed;
        if (!readString(entry, QStringLiteral("name"), entryContext,
                        parsed.name, error, false)
            || !readString(entry, QStringLiteral("declarationText"), entryContext,
                           parsed.declarationText, error)
            || !readString(entry, QStringLiteral("valueText"), entryContext,
                           parsed.valueText, error)
            || !readString(entry, QStringLiteral("displayValueText"), entryContext,
                           parsed.displayValueText, error)
            || !readBool(entry, QStringLiteral("semanticAvailable"), entryContext,
                         parsed.semanticAvailable, error)) {
            return false;
        }
        if (enumNames.contains(qString(parsed.name))) {
            error = entryContext + QStringLiteral(" duplicates enum name ")
                + qString(parsed.name);
            return false;
        }
        enumNames.insert(qString(parsed.name));
        type.enumValues.push_back(std::move(parsed));
    }
    if (!structMembers.isArray()) {
        error = QStringLiteral("%1.structMembers must be an array").arg(context);
        return false;
    }
    QSet<QString> memberNames;
    for (qsizetype index = 0; index < structMembers.toArray().size(); ++index) {
        const auto value = structMembers.toArray().at(index);
        const auto entryContext = QStringLiteral("%1.structMembers[%2]")
                                      .arg(context)
                                      .arg(index);
        if (!value.isObject()) {
            error = entryContext + QStringLiteral(" must be an object");
            return false;
        }
        const auto entry = value.toObject();
        if (!exactKeys(
                entry,
                {QStringLiteral("name"), QStringLiteral("declarationText"),
                 QStringLiteral("type")},
                entryContext,
                error)) {
            return false;
        }
        ModuleManifestStructMember parsed;
        if (!readString(entry, QStringLiteral("name"), entryContext,
                        parsed.name, error, false)
            || !readString(entry, QStringLiteral("declarationText"), entryContext,
                           parsed.declarationText, error)) {
            return false;
        }
        if (!entry.value(QStringLiteral("type")).isObject()
            || !parseTypeShape(entry.value(QStringLiteral("type")).toObject(),
                               entryContext + QStringLiteral(".type"),
                               parsed.type, error)) {
            if (error.isEmpty()) error = entryContext + QStringLiteral(".type must be an object");
            return false;
        }
        if (memberNames.contains(qString(parsed.name))) {
            error = entryContext + QStringLiteral(" duplicates member name ")
                + qString(parsed.name);
            return false;
        }
        memberNames.insert(qString(parsed.name));
        type.structMembers.push_back(std::move(parsed));
    }
    return true;
}

std::optional<ModulePortDirection> portDirection(const QString& text)
{
    if (text == QStringLiteral("input")) return ModulePortDirection::Input;
    if (text == QStringLiteral("output")) return ModulePortDirection::Output;
    if (text == QStringLiteral("inout")) return ModulePortDirection::Inout;
    if (text == QStringLiteral("ref")) return ModulePortDirection::Ref;
    if (text == QStringLiteral("interface")) return ModulePortDirection::Interface;
    if (text == QStringLiteral("unknown")) return ModulePortDirection::Unknown;
    return std::nullopt;
}

ModuleCandidateSuggestion suggestion(const std::vector<std::string>& candidates)
{
    ModuleCandidateSuggestion result;
    result.candidates = candidates;
    result.state = candidates.empty()
        ? ModuleCandidateState::None
        : candidates.size() == 1
            ? ModuleCandidateState::Unique
            : ModuleCandidateState::Ambiguous;
    if (result.state == ModuleCandidateState::Unique) {
        result.selectedPortName = candidates.front();
    }
    return result;
}

std::string stableDigestId(
    const std::string_view prefix,
    const std::string& identity,
    const std::string& suffix)
{
    const auto input = QByteArray::fromStdString(identity + "|" + suffix);
    const auto hash = QCryptographicHash::hash(input, QCryptographicHash::Sha256)
                          .toHex()
                          .left(24);
    return std::string(prefix) + "-" + hash.toStdString();
}

std::optional<std::string> normalizedEnumValue(const ModuleManifestEnumValue& value)
{
    auto text = qString(!value.valueText.empty()
                            ? value.valueText
                            : value.displayValueText);
    text.remove(QRegularExpression(QStringLiteral("[\\s_]")));
    if (text.isEmpty()) return std::nullopt;
    static const QRegularExpression literal(
        QStringLiteral("^(?:[0-9]+)?'[sS]?([bBoOdDhH])([+\\-]?[0-9a-fA-FxXzZ]+)$"));
    const auto match = literal.match(text);
    if (match.hasMatch()) {
        const auto radix = match.captured(1).toLower();
        const auto digits = match.captured(2);
        if (radix == QStringLiteral("b")) return stdString(QStringLiteral("0b") + digits);
        if (radix == QStringLiteral("o")) return stdString(QStringLiteral("0o") + digits);
        if (radix == QStringLiteral("h")) return stdString(QStringLiteral("0x") + digits);
        if (radix == QStringLiteral("d")) return stdString(digits);
    }
    static const QRegularExpression plain(
        QStringLiteral("^(?:[+\\-]?[0-9]+|0[bBoOxX][0-9a-fA-FxXzZ]+)$"));
    return plain.match(text).hasMatch()
        ? std::optional<std::string>{stdString(text)}
        : std::nullopt;
}

QString candidateList(const std::vector<std::string>& candidates)
{
    QStringList result;
    for (const auto& candidate : candidates) result.append(qString(candidate));
    return result.join(QStringLiteral(", "));
}

bool stimulusDirection(const ModulePortDirection direction)
{
    return direction == ModulePortDirection::Input
        || direction == ModulePortDirection::Inout
        || direction == ModulePortDirection::Ref;
}

bool watchDirection(const ModulePortDirection direction)
{
    return direction == ModulePortDirection::Output
        || direction == ModulePortDirection::Inout
        || direction == ModulePortDirection::Ref;
}

} // namespace

ModuleManifestParseResult parseZeroSlackModuleManifest(const QByteArray& document)
{
    ModuleManifestParseResult result;
    QJsonParseError parseError;
    const auto parsed = QJsonDocument::fromJson(document, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        result.error = QStringLiteral("Invalid ZeroSlack Module Manifest JSON: %1")
                           .arg(parseError.errorString());
        return result;
    }
    const auto root = parsed.object();
    QString error;
    if (!exactKeys(
            root,
            {QStringLiteral("schemaVersion"), QStringLiteral("workspaceId"),
             QStringLiteral("target"), QStringLiteral("sources"),
             QStringLiteral("includeDirs"), QStringLiteral("defines"),
             QStringLiteral("parameters"), QStringLiteral("ports"),
             QStringLiteral("clockCandidates"), QStringLiteral("resetCandidates")},
            QStringLiteral("manifest"),
            error)) {
        result.error = error;
        return result;
    }
    const auto version = root.value(QStringLiteral("schemaVersion"));
    if (!version.isDouble()
        || version.toDouble() != ZeroSlackModuleManifest::CurrentSchemaVersion) {
        result.error = QStringLiteral("Unsupported ZeroSlack Module Manifest schemaVersion");
        return result;
    }

    ZeroSlackModuleManifest manifest;
    if (!readString(root, QStringLiteral("workspaceId"), QStringLiteral("manifest"),
                    manifest.workspaceId, error, false)
        || !QRegularExpression(QStringLiteral("^sha256:[0-9a-f]{64}$"))
                .match(qString(manifest.workspaceId))
                .hasMatch()) {
        result.error = error.isEmpty()
            ? QStringLiteral("manifest.workspaceId must be a lowercase SHA-256 identity")
            : error;
        return result;
    }

    const auto targetValue = root.value(QStringLiteral("target"));
    if (!targetValue.isObject()) {
        result.error = QStringLiteral("manifest.target must be an object");
        return result;
    }
    const auto target = targetValue.toObject();
    if (!exactKeys(
            target,
            {QStringLiteral("mode"), QStringLiteral("module"),
             QStringLiteral("instancePath"), QStringLiteral("sourceFile"),
             QStringLiteral("sourceLine")},
            QStringLiteral("manifest.target"),
            error)) {
        result.error = error;
        return result;
    }
    std::string mode;
    if (!readString(target, QStringLiteral("mode"), QStringLiteral("manifest.target"),
                    mode, error, false)
        || !readString(target, QStringLiteral("module"), QStringLiteral("manifest.target"),
                       manifest.target.module, error, false)
        || !readString(target, QStringLiteral("instancePath"), QStringLiteral("manifest.target"),
                       manifest.target.instancePath, error)
        || !readRelativePath(target, QStringLiteral("sourceFile"),
                            QStringLiteral("manifest.target"),
                            manifest.target.sourceFile, error)
        || !readPositiveInt(target, QStringLiteral("sourceLine"),
                           QStringLiteral("manifest.target"),
                           manifest.target.sourceLine, error)) {
        result.error = error;
        return result;
    }
    if (mode == "module-definition") {
        manifest.target.mode = ModuleManifestTargetMode::ModuleDefinition;
        if (!manifest.target.instancePath.empty()) {
            result.error = QStringLiteral("module-definition target must have an empty instancePath");
            return result;
        }
    } else if (mode == "instance") {
        manifest.target.mode = ModuleManifestTargetMode::Instance;
        if (manifest.target.instancePath.empty()) {
            result.error = QStringLiteral("instance target requires instancePath");
            return result;
        }
    } else {
        result.error = QStringLiteral("manifest.target.mode is unsupported");
        return result;
    }

    const auto sources = root.value(QStringLiteral("sources"));
    if (!sources.isArray()) {
        result.error = QStringLiteral("manifest.sources must be an array");
        return result;
    }
    QSet<QString> sourcePaths;
    for (qsizetype index = 0; index < sources.toArray().size(); ++index) {
        const auto value = sources.toArray().at(index);
        const auto context = QStringLiteral("manifest.sources[%1]").arg(index);
        if (!value.isObject()) {
            result.error = context + QStringLiteral(" must be an object");
            return result;
        }
        const auto source = value.toObject();
        if (!exactKeys(source,
                       {QStringLiteral("path"), QStringLiteral("role")},
                       context, error)) {
            result.error = error;
            return result;
        }
        ModuleManifestSource parsedSource;
        if (!readRelativePath(source, QStringLiteral("path"), context,
                              parsedSource.path, error)
            || !readString(source, QStringLiteral("role"), context,
                           parsedSource.role, error, false)) {
            result.error = error;
            return result;
        }
        static const QSet<QString> roles{
            QStringLiteral("design"), QStringLiteral("header"),
            QStringLiteral("external-header"), QStringLiteral("generated"),
            QStringLiteral("unknown")};
        if (!roles.contains(qString(parsedSource.role))) {
            result.error = context + QStringLiteral(".role is unsupported");
            return result;
        }
        if (sourcePaths.contains(qString(parsedSource.path))) {
            result.error = QStringLiteral("manifest.sources contains duplicate path %1")
                               .arg(qString(parsedSource.path));
            return result;
        }
        sourcePaths.insert(qString(parsedSource.path));
        manifest.sources.push_back(std::move(parsedSource));
    }
    if (!sourcePaths.contains(qString(manifest.target.sourceFile))) {
        result.error = QStringLiteral("manifest.target.sourceFile is absent from sources");
        return result;
    }
    if (!readStringArray(root.value(QStringLiteral("includeDirs")),
                         QStringLiteral("manifest.includeDirs"),
                         manifest.includeDirs, error, true, true, true)) {
        result.error = error;
        return result;
    }

    const auto defines = root.value(QStringLiteral("defines"));
    if (!defines.isObject()) {
        result.error = QStringLiteral("manifest.defines must be an object");
        return result;
    }
    const auto defineObject = defines.toObject();
    for (auto iterator = defineObject.begin();
         iterator != defineObject.end(); ++iterator) {
        if (iterator.key().isEmpty() || !iterator.value().isString()) {
            result.error = QStringLiteral("manifest.defines values must be strings with non-empty keys");
            return result;
        }
        manifest.defines.emplace(stdString(iterator.key()),
                                 stdString(iterator.value().toString()));
    }

    const auto parameters = root.value(QStringLiteral("parameters"));
    if (!parameters.isObject()) {
        result.error = QStringLiteral("manifest.parameters must be an object");
        return result;
    }
    const auto parameterObject = parameters.toObject();
    for (auto iterator = parameterObject.begin();
         iterator != parameterObject.end(); ++iterator) {
        const auto context = QStringLiteral("manifest.parameters.%1").arg(iterator.key());
        if (iterator.key().isEmpty() || !iterator.value().isObject()) {
            result.error = context + QStringLiteral(" must be an object");
            return result;
        }
        const auto parameter = iterator.value().toObject();
        if (!exactKeys(
                parameter,
                {QStringLiteral("name"), QStringLiteral("declarationText"),
                 QStringLiteral("expressionText"), QStringLiteral("valueText"),
                 QStringLiteral("displayValueText"), QStringLiteral("semanticAvailable"),
                 QStringLiteral("type"), QStringLiteral("sourceFile"),
                 QStringLiteral("sourceLine")},
                context,
                error)) {
            result.error = error;
            return result;
        }
        ModuleManifestParameter parsedParameter;
        if (!readString(parameter, QStringLiteral("name"), context,
                        parsedParameter.name, error, false)
            || qString(parsedParameter.name) != iterator.key()
            || !readString(parameter, QStringLiteral("declarationText"), context,
                           parsedParameter.declarationText, error)
            || !readString(parameter, QStringLiteral("expressionText"), context,
                           parsedParameter.expressionText, error)
            || !readString(parameter, QStringLiteral("valueText"), context,
                           parsedParameter.valueText, error)
            || !readString(parameter, QStringLiteral("displayValueText"), context,
                           parsedParameter.displayValueText, error)
            || !readBool(parameter, QStringLiteral("semanticAvailable"), context,
                         parsedParameter.semanticAvailable, error)
            || !readRelativePath(parameter, QStringLiteral("sourceFile"), context,
                                parsedParameter.sourceFile, error)
            || !readPositiveInt(parameter, QStringLiteral("sourceLine"), context,
                               parsedParameter.sourceLine, error)) {
            result.error = error.isEmpty()
                ? context + QStringLiteral(".name must match its object key")
                : error;
            return result;
        }
        if (!parameter.value(QStringLiteral("type")).isObject()
            || !parseType(parameter.value(QStringLiteral("type")).toObject(),
                          context + QStringLiteral(".type"),
                          parsedParameter.type, error)) {
            result.error = error.isEmpty()
                ? context + QStringLiteral(".type must be an object")
                : error;
            return result;
        }
        manifest.parameters.push_back(std::move(parsedParameter));
    }

    const auto ports = root.value(QStringLiteral("ports"));
    if (!ports.isArray()) {
        result.error = QStringLiteral("manifest.ports must be an array");
        return result;
    }
    QSet<QString> portNames;
    for (qsizetype index = 0; index < ports.toArray().size(); ++index) {
        const auto value = ports.toArray().at(index);
        const auto context = QStringLiteral("manifest.ports[%1]").arg(index);
        if (!value.isObject()) {
            result.error = context + QStringLiteral(" must be an object");
            return result;
        }
        const auto port = value.toObject();
        if (!exactKeys(
                port,
                {QStringLiteral("name"), QStringLiteral("direction"),
                 QStringLiteral("declarationText"), QStringLiteral("type"),
                 QStringLiteral("sourceFile"), QStringLiteral("sourceLine")},
                context,
                error)) {
            result.error = error;
            return result;
        }
        ModuleManifestPort parsedPort;
        std::string direction;
        if (!readString(port, QStringLiteral("name"), context,
                        parsedPort.name, error, false)
            || !readString(port, QStringLiteral("direction"), context,
                           direction, error, false)
            || !readString(port, QStringLiteral("declarationText"), context,
                           parsedPort.declarationText, error)
            || !readRelativePath(port, QStringLiteral("sourceFile"), context,
                                parsedPort.sourceFile, error)
            || !readPositiveInt(port, QStringLiteral("sourceLine"), context,
                               parsedPort.sourceLine, error)) {
            result.error = error;
            return result;
        }
        const auto parsedDirection = portDirection(qString(direction));
        if (!parsedDirection) {
            result.error = context + QStringLiteral(".direction is unsupported");
            return result;
        }
        parsedPort.direction = *parsedDirection;
        if (!port.value(QStringLiteral("type")).isObject()
            || !parseType(port.value(QStringLiteral("type")).toObject(),
                          context + QStringLiteral(".type"),
                          parsedPort.type, error)) {
            result.error = error.isEmpty()
                ? context + QStringLiteral(".type must be an object")
                : error;
            return result;
        }
        if (portNames.contains(qString(parsedPort.name))) {
            result.error = QStringLiteral("manifest.ports contains duplicate name %1")
                               .arg(qString(parsedPort.name));
            return result;
        }
        portNames.insert(qString(parsedPort.name));
        manifest.ports.push_back(std::move(parsedPort));
    }

    if (!readStringArray(root.value(QStringLiteral("clockCandidates")),
                         QStringLiteral("manifest.clockCandidates"),
                         manifest.clockCandidates, error, true, true, false)
        || !readStringArray(root.value(QStringLiteral("resetCandidates")),
                           QStringLiteral("manifest.resetCandidates"),
                           manifest.resetCandidates, error, true, true, false)) {
        result.error = error;
        return result;
    }
    const auto validateCandidates = [&](const std::vector<std::string>& candidates,
                                        const QString& name) {
        for (const auto& candidate : candidates) {
            if (!portNames.contains(qString(candidate))) {
                error = QStringLiteral("manifest.%1 references unknown port %2")
                            .arg(name, qString(candidate));
                return false;
            }
        }
        return true;
    };
    if (!validateCandidates(manifest.clockCandidates, QStringLiteral("clockCandidates"))
        || !validateCandidates(manifest.resetCandidates, QStringLiteral("resetCandidates"))) {
        result.error = error;
        return result;
    }

    const auto canonical = QJsonDocument(root).toJson(QJsonDocument::Compact);
    manifest.identity = "sha256:"
        + QCryptographicHash::hash(canonical, QCryptographicHash::Sha256)
              .toHex()
              .toStdString();
    result.manifest = std::move(manifest);
    return result;
}

ModuleManifestImportResult importZeroSlackModuleManifest(
    const ZeroSlackModuleManifest& manifest,
    const ModuleManifestImportOptions& options)
{
    ModuleManifestImportResult result;
    if (manifest.schemaVersion != ZeroSlackModuleManifest::CurrentSchemaVersion) {
        result.error = QStringLiteral("Unsupported ZeroSlack Module Manifest schemaVersion");
        return result;
    }
    if (manifest.identity.empty() || options.duration <= 0
        || options.defaultClockPeriod <= 0
        || options.defaultClockPeriod > options.duration) {
        result.error = QStringLiteral("Module Manifest import options or identity are invalid");
        return result;
    }

    result.clockSuggestion = suggestion(manifest.clockCandidates);
    result.resetSuggestion = suggestion(manifest.resetCandidates);
    if (result.clockSuggestion.state == ModuleCandidateState::Ambiguous) {
        result.diagnostics.append(
            QStringLiteral("Clock candidates are ambiguous (%1); no clock was selected.")
                .arg(candidateList(manifest.clockCandidates)));
    }
    if (result.resetSuggestion.state == ModuleCandidateState::Ambiguous) {
        result.diagnostics.append(
            QStringLiteral("Reset candidates are ambiguous (%1); no reset was selected.")
                .arg(candidateList(manifest.resetCandidates)));
    }

    Project project;
    project.id = stableDigestId("zs-project", manifest.identity, "project");
    project.name = manifest.target.module + " simulation";
    project.timeBase = {1};
    project.extensions.emplace(
        "waveSimulation.moduleManifestIdentity",
        jsonStringValue(manifest.identity));
    project.extensions.emplace(
        "waveSimulation.workspaceId",
        jsonStringValue(manifest.workspaceId));
    project.extensions.emplace(
        "waveSimulation.targetMode",
        jsonStringValue(
            manifest.target.mode == ModuleManifestTargetMode::Instance
                ? "instance"
                : "module-definition"));
    project.extensions.emplace(
        "waveSimulation.targetModule",
        jsonStringValue(manifest.target.module));
    project.extensions.emplace(
        "waveSimulation.targetInstancePath",
        jsonStringValue(manifest.target.instancePath));
    project.extensions.emplace(
        "waveSimulation.clockSuggestionState",
        jsonStringValue(std::string(toString(result.clockSuggestion.state))));
    project.extensions.emplace(
        "waveSimulation.clockCandidates",
        jsonStringArrayValue(manifest.clockCandidates));
    project.extensions.emplace(
        "waveSimulation.resetSuggestionState",
        jsonStringValue(std::string(toString(result.resetSuggestion.state))));
    project.extensions.emplace(
        "waveSimulation.resetCandidates",
        jsonStringArrayValue(manifest.resetCandidates));

    Scenario scenario;
    scenario.id = stableDigestId("zs-scenario", manifest.identity, "default");
    scenario.name = manifest.target.instancePath.empty()
        ? manifest.target.module
        : manifest.target.instancePath;
    scenario.duration = options.duration;
    scenario.extensions = project.extensions;

    struct OrderedLane {
        int priority{0};
        std::size_t sourceOrder{0};
        Lane lane;
        bool stimulus{false};
        bool watch{false};
    };
    std::vector<OrderedLane> lanes;
    lanes.reserve(manifest.ports.size());

    for (std::size_t index = 0; index < manifest.ports.size(); ++index) {
        const auto& port = manifest.ports[index];
        const auto& shape = port.type.shape;
        if (port.direction == ModulePortDirection::Interface
            || shape.interfaceType) {
            result.diagnostics.append(
                QStringLiteral("Port %1 is an interface and is deferred to the structured-input slice.")
                    .arg(qString(port.name)));
            continue;
        }
        if (port.direction == ModulePortDirection::Unknown) {
            result.diagnostics.append(
                QStringLiteral("Port %1 has unknown direction and was not imported.")
                    .arg(qString(port.name)));
            continue;
        }
        if (shape.unpackedArray) {
            result.diagnostics.append(
                QStringLiteral("Port %1 is an unpacked array and is deferred to the structured-input slice.")
                    .arg(qString(port.name)));
            continue;
        }
        if (!shape.semanticAvailable || !shape.fixedSize || !shape.integral
            || shape.bitWidth == 0
            || shape.bitWidth > std::numeric_limits<std::uint32_t>::max()) {
            result.diagnostics.append(
                QStringLiteral("Port %1 has no supported fixed integral width and was not imported.")
                    .arg(qString(port.name)));
            continue;
        }

        const bool stimulus = stimulusDirection(port.direction);
        const bool watch = watchDirection(port.direction);
        Lane lane;
        lane.id = stableDigestId("zs-port", manifest.identity, port.name);
        lane.name = port.name;
        lane.width = static_cast<std::uint32_t>(shape.bitWidth);
        lane.isSigned = shape.isSigned;
        lane.kind = !port.type.enumValues.empty()
                || shape.semanticKind == "enum"
            ? LaneKind::Enum
            : lane.width == 1 ? LaneKind::Bit : LaneKind::Bus;
        lane.radix = Radix::Hexadecimal;
        lane.height = 56;
        lane.visible = true;

        const bool selectedClock =
            result.clockSuggestion.state == ModuleCandidateState::Unique
            && result.clockSuggestion.selectedPortName == port.name;
        const bool selectedReset =
            result.resetSuggestion.state == ModuleCandidateState::Unique
            && result.resetSuggestion.selectedPortName == port.name;
        const auto role = stimulus && watch
            ? std::string("stimulus-watch")
            : stimulus ? std::string("stimulus") : std::string("watch");

        lane.color = watch && stimulus
            ? "#ce93d8"
            : watch ? "#ffb74d" : "#64b5f6";
        lane.extensions.emplace("sourceApplication", jsonStringValue("ZeroSlack"));
        lane.extensions.emplace(
            "waveSimulation.moduleManifestIdentity",
            jsonStringValue(manifest.identity));
        lane.extensions.emplace(
            "waveSimulation.role",
            jsonStringValue(role));
        lane.extensions.emplace(
            "waveSimulation.direction",
            jsonStringValue(std::string(toString(port.direction))));
        lane.extensions.emplace(
            "waveSimulation.declarationText",
            jsonStringValue(port.declarationText));
        lane.extensions.emplace(
            "waveSimulation.sourceFile",
            jsonStringValue(port.sourceFile));
        lane.extensions.emplace(
            "waveSimulation.sourceLine",
            std::to_string(port.sourceLine));
        lane.extensions.emplace(
            "waveSimulation.canonicalTypeId",
            jsonStringValue(shape.canonicalTypeId));
        lane.extensions.emplace(
            "waveSimulation.resolvedTypeText",
            jsonStringValue(shape.resolvedTypeText));
        lane.extensions.emplace(
            "waveSimulation.typedefChain",
            jsonStringArrayValue(shape.typedefChain));
        lane.extensions.emplace(
            "waveSimulation.clockCandidate",
            selectedClock ? "true" : "false");
        lane.extensions.emplace(
            "waveSimulation.resetCandidate",
            selectedReset ? "true" : "false");

        for (const auto& value : port.type.enumValues) {
            const auto normalized = normalizedEnumValue(value);
            if (!value.semanticAvailable || !normalized) {
                result.diagnostics.append(
                    QStringLiteral("Enum value %1 for port %2 could not be normalized and was omitted.")
                        .arg(qString(value.name), qString(port.name)));
                continue;
            }
            lane.enumMap.emplace(value.name, *normalized);
        }

        if (selectedClock) {
            if (!stimulus || lane.width != 1) {
                result.diagnostics.append(
                    QStringLiteral("Clock candidate %1 is not a one-bit stimulus port; no clock was selected.")
                        .arg(qString(port.name)));
            } else {
                ClockDomain clock;
                clock.id = stableDigestId(
                    "zs-clock", manifest.identity, port.name);
                clock.name = port.name;
                clock.period = options.defaultClockPeriod;
                clock.phase = 0;
                clock.dutyCycle = {1, 2};
                clock.activeEdge = ClockEdge::Rising;
                clock.extensions = lane.extensions;
                lane.kind = LaneKind::Clock;
                lane.clockDomainId = clock.id;
                lane.color = "#81c784";
                project.clockDomains.push_back(std::move(clock));
                result.clockSuggestion.selectedLaneId = lane.id;
            }
        }
        if (selectedReset) {
            result.resetSuggestion.selectedLaneId = lane.id;
            lane.color = "#ef9a9a";
        }

        if (stimulus && lane.kind != LaneKind::Clock) {
            const auto defaultValue = lane.kind == LaneKind::Bit
                ? std::string("0")
                : std::string("0x0");
            setSegmentRange(
                lane,
                0,
                options.duration,
                defaultValue,
                stableDigestId("zs-segment", manifest.identity, port.name));
        }

        int priority = 4;
        if (lane.kind == LaneKind::Clock) priority = 0;
        else if (selectedReset) priority = 1;
        else if (stimulus && !watch) priority = 2;
        else if (stimulus) priority = 3;
        lanes.push_back({
            priority, index, std::move(lane), stimulus, watch});
    }

    if (result.clockSuggestion.state == ModuleCandidateState::Unique
        && result.clockSuggestion.selectedLaneId.empty()) {
        result.diagnostics.append(
            QStringLiteral("Unique clock candidate %1 could not be imported.")
                .arg(qString(result.clockSuggestion.selectedPortName)));
    }
    if (result.resetSuggestion.state == ModuleCandidateState::Unique
        && result.resetSuggestion.selectedLaneId.empty()) {
        result.diagnostics.append(
            QStringLiteral("Unique reset candidate %1 could not be imported.")
                .arg(qString(result.resetSuggestion.selectedPortName)));
    }

    std::stable_sort(
        lanes.begin(),
        lanes.end(),
        [](const OrderedLane& left, const OrderedLane& right) {
            return left.priority < right.priority
                || (left.priority == right.priority
                    && left.sourceOrder < right.sourceOrder);
        });
    scenario.lanes.reserve(lanes.size());
    for (auto& ordered : lanes) {
        if (ordered.stimulus) result.stimulusLaneIds.push_back(ordered.lane.id);
        if (ordered.watch) result.watchLaneIds.push_back(ordered.lane.id);
        scenario.lanes.push_back(std::move(ordered.lane));
    }

    project.scenarios.push_back(std::move(scenario));
    project.linkedResources.push_back({
        "zeroslack-wave-module-manifest",
        {},
        manifest.identity,
        manifest.identity,
        "Imported ZeroSlack Module Manifest for " + manifest.target.module,
        {{"workspaceId", jsonStringValue(manifest.workspaceId)},
         {"targetModule", jsonStringValue(manifest.target.module)},
         {"targetInstancePath", jsonStringValue(manifest.target.instancePath)}}
    });

    result.project = std::move(project);
    return result;
}

std::string_view toString(const ModulePortDirection direction) noexcept
{
    switch (direction) {
    case ModulePortDirection::Input: return "input";
    case ModulePortDirection::Output: return "output";
    case ModulePortDirection::Inout: return "inout";
    case ModulePortDirection::Ref: return "ref";
    case ModulePortDirection::Interface: return "interface";
    case ModulePortDirection::Unknown: return "unknown";
    }
    return "unknown";
}

std::string_view toString(const ModuleCandidateState state) noexcept
{
    switch (state) {
    case ModuleCandidateState::None: return "none";
    case ModuleCandidateState::Unique: return "unique";
    case ModuleCandidateState::Ambiguous: return "ambiguous";
    }
    return "none";
}

} // namespace wave
