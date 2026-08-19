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

QString structuredSelectorKindName(
    const ModuleManifestStructuredSelectorKind kind)
{
    switch (kind) {
    case ModuleManifestStructuredSelectorKind::StructMember:
        return QStringLiteral("struct-member");
    case ModuleManifestStructuredSelectorKind::PackedIndex:
        return QStringLiteral("packed-index");
    case ModuleManifestStructuredSelectorKind::UnpackedIndex:
        return QStringLiteral("unpacked-index");
    case ModuleManifestStructuredSelectorKind::InterfaceMember:
        return QStringLiteral("interface-member");
    }
    return {};
}

std::string jsonStructuredSelectorsValue(
    const std::vector<ModuleManifestStructuredSelector>& selectors)
{
    QJsonArray array;
    for (const auto& selector : selectors) {
        array.append(QJsonObject{
            {QStringLiteral("kind"), structuredSelectorKindName(selector.kind)},
            {QStringLiteral("name"), qString(selector.name)},
            {QStringLiteral("sourceIndex"), selector.sourceIndex},
            {QStringLiteral("storageIndex"), selector.storageIndex},
        });
    }
    return QJsonDocument(array).toJson(QJsonDocument::Compact).toStdString();
}

std::string structuredTraceName(
    const std::size_t portIndex,
    const std::size_t leafIndex)
{
    return "zs_structured_" + std::to_string(portIndex) + "_"
        + std::to_string(leafIndex);
}

bool readSignedInteger(
    const QJsonObject& object,
    const QString& name,
    const QString& context,
    int& output,
    QString& error)
{
    const auto value = object.value(name);
    const auto number = value.toDouble(
        std::numeric_limits<double>::quiet_NaN());
    if (!value.isDouble() || !std::isfinite(number)
        || std::floor(number) != number
        || number < static_cast<double>(std::numeric_limits<int>::min())
        || number > static_cast<double>(std::numeric_limits<int>::max())) {
        error = QStringLiteral("%1.%2 must be a supported integer")
                    .arg(context, name);
        return false;
    }
    output = static_cast<int>(number);
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

std::optional<ModuleManifestStructuredSelectorKind> selectorKind(
    const QString& text)
{
    if (text == QStringLiteral("struct-member"))
        return ModuleManifestStructuredSelectorKind::StructMember;
    if (text == QStringLiteral("packed-index"))
        return ModuleManifestStructuredSelectorKind::PackedIndex;
    if (text == QStringLiteral("unpacked-index"))
        return ModuleManifestStructuredSelectorKind::UnpackedIndex;
    if (text == QStringLiteral("interface-member"))
        return ModuleManifestStructuredSelectorKind::InterfaceMember;
    return std::nullopt;
}

QString selectorPath(const ModuleManifestStructuredSelector& selector)
{
    switch (selector.kind) {
    case ModuleManifestStructuredSelectorKind::StructMember:
    case ModuleManifestStructuredSelectorKind::InterfaceMember:
        return QStringLiteral(".%1").arg(qString(selector.name));
    case ModuleManifestStructuredSelectorKind::PackedIndex:
    case ModuleManifestStructuredSelectorKind::UnpackedIndex:
        return QStringLiteral("[%1]").arg(selector.sourceIndex);
    }
    return {};
}

bool parseEditableLeaves(
    const QJsonValue& value,
    const QString& context,
    const ModulePortDirection rootDirection,
    std::vector<ModuleManifestEditableLeaf>& leaves,
    QString& error)
{
    if (!value.isArray()) {
        error = context + QStringLiteral(" must be an array");
        return false;
    }
    QSet<QString> paths;
    for (qsizetype index = 0; index < value.toArray().size(); ++index) {
        const auto leafValue = value.toArray().at(index);
        const auto leafContext = QStringLiteral("%1[%2]")
                                     .arg(context)
                                     .arg(index);
        if (!leafValue.isObject()) {
            error = leafContext + QStringLiteral(" must be an object");
            return false;
        }
        const auto object = leafValue.toObject();
        if (!exactKeys(
                object,
                {QStringLiteral("relativePath"), QStringLiteral("direction"),
                 QStringLiteral("selectors"), QStringLiteral("type"),
                 QStringLiteral("enumValues"),
                 QStringLiteral("packedBitOffsetValid"),
                 QStringLiteral("packedBitOffset")},
                leafContext,
                error)) {
            return false;
        }

        ModuleManifestEditableLeaf leaf;
        std::string directionText;
        if (!readString(object, QStringLiteral("relativePath"), leafContext,
                        leaf.relativePath, error, false)
            || !readString(object, QStringLiteral("direction"), leafContext,
                           directionText, error)
            || !readBool(object, QStringLiteral("packedBitOffsetValid"),
                         leafContext, leaf.packedBitOffsetValid, error)
            || !readInteger(object, QStringLiteral("packedBitOffset"),
                            leafContext, leaf.packedBitOffset, error)) {
            return false;
        }
        if (rootDirection == ModulePortDirection::Interface) {
            if (directionText.empty()) {
                error = leafContext
                    + QStringLiteral(
                        ".direction is required for an interface member");
                return false;
            }
            const auto parsedDirection = portDirection(qString(directionText));
            if (!parsedDirection
                || *parsedDirection == ModulePortDirection::Interface
                || *parsedDirection == ModulePortDirection::Unknown) {
                error = leafContext + QStringLiteral(".direction is unsupported");
                return false;
            }
            leaf.direction = *parsedDirection;
            leaf.inheritsPortDirection = false;
        } else {
            if (!directionText.empty()) {
                error = leafContext
                    + QStringLiteral(
                        ".direction must inherit the root data-port direction");
                return false;
            }
            leaf.direction = rootDirection;
            leaf.inheritsPortDirection = true;
        }

        const auto selectors = object.value(QStringLiteral("selectors"));
        if (!selectors.isArray() || selectors.toArray().isEmpty()) {
            error = leafContext + QStringLiteral(".selectors must be a non-empty array");
            return false;
        }
        QString computedPath;
        for (qsizetype selectorIndex = 0;
             selectorIndex < selectors.toArray().size(); ++selectorIndex) {
            const auto selectorValue = selectors.toArray().at(selectorIndex);
            const auto selectorContext = QStringLiteral("%1.selectors[%2]")
                                             .arg(leafContext)
                                             .arg(selectorIndex);
            if (!selectorValue.isObject()) {
                error = selectorContext + QStringLiteral(" must be an object");
                return false;
            }
            const auto selectorObject = selectorValue.toObject();
            if (!exactKeys(
                    selectorObject,
                    {QStringLiteral("kind"), QStringLiteral("name"),
                     QStringLiteral("sourceIndex"),
                     QStringLiteral("storageIndex")},
                    selectorContext,
                    error)) {
                return false;
            }
            std::string kindText;
            ModuleManifestStructuredSelector selector;
            std::uint64_t storageIndex = 0;
            if (!readString(selectorObject, QStringLiteral("kind"),
                            selectorContext, kindText, error, false)
                || !readString(selectorObject, QStringLiteral("name"),
                               selectorContext, selector.name, error)
                || !readSignedInteger(selectorObject,
                                      QStringLiteral("sourceIndex"),
                                      selectorContext,
                                      selector.sourceIndex,
                                      error)
                || !readInteger(selectorObject,
                                QStringLiteral("storageIndex"),
                                selectorContext,
                                storageIndex,
                                error)
                || storageIndex
                       > static_cast<std::uint64_t>(
                           std::numeric_limits<int>::max())) {
                if (error.isEmpty()) {
                    error = selectorContext
                        + QStringLiteral(".storageIndex is unsupported");
                }
                return false;
            }
            const auto parsedKind = selectorKind(qString(kindText));
            if (!parsedKind) {
                error = selectorContext + QStringLiteral(".kind is unsupported");
                return false;
            }
            selector.kind = *parsedKind;
            selector.storageIndex = static_cast<int>(storageIndex);
            const bool memberSelector =
                selector.kind
                    == ModuleManifestStructuredSelectorKind::StructMember
                || selector.kind
                    == ModuleManifestStructuredSelectorKind::InterfaceMember;
            if (memberSelector != !selector.name.empty()) {
                error = selectorContext
                    + QStringLiteral(".name does not match selector kind");
                return false;
            }
            computedPath += selectorPath(selector);
            leaf.selectors.push_back(std::move(selector));
        }
        if (computedPath != qString(leaf.relativePath)) {
            error = leafContext
                + QStringLiteral(".relativePath does not match selectors");
            return false;
        }
        if (!object.value(QStringLiteral("type")).isObject()
            || !parseTypeShape(
                object.value(QStringLiteral("type")).toObject(),
                leafContext + QStringLiteral(".type"),
                leaf.type,
                error)
            || !leaf.type.semanticAvailable || !leaf.type.fixedSize
            || !leaf.type.integral || leaf.type.bitWidth == 0
            || leaf.type.bitWidth
                   > std::numeric_limits<std::uint32_t>::max()) {
            if (error.isEmpty()) {
                error = leafContext
                    + QStringLiteral(".type must be a fixed integral leaf");
            }
            return false;
        }

        const auto enumValues = object.value(QStringLiteral("enumValues"));
        if (!enumValues.isArray()) {
            error = leafContext + QStringLiteral(".enumValues must be an array");
            return false;
        }
        QSet<QString> enumNames;
        for (qsizetype enumIndex = 0;
             enumIndex < enumValues.toArray().size(); ++enumIndex) {
            const auto enumValue = enumValues.toArray().at(enumIndex);
            const auto enumContext = QStringLiteral("%1.enumValues[%2]")
                                         .arg(leafContext)
                                         .arg(enumIndex);
            if (!enumValue.isObject()) {
                error = enumContext + QStringLiteral(" must be an object");
                return false;
            }
            const auto enumObject = enumValue.toObject();
            if (!exactKeys(
                    enumObject,
                    {QStringLiteral("name"),
                     QStringLiteral("declarationText"),
                     QStringLiteral("valueText"),
                     QStringLiteral("displayValueText"),
                     QStringLiteral("semanticAvailable")},
                    enumContext,
                    error)) {
                return false;
            }
            ModuleManifestEnumValue parsed;
            if (!readString(enumObject, QStringLiteral("name"), enumContext,
                            parsed.name, error, false)
                || !readString(enumObject,
                               QStringLiteral("declarationText"), enumContext,
                               parsed.declarationText, error)
                || !readString(enumObject, QStringLiteral("valueText"),
                               enumContext, parsed.valueText, error)
                || !readString(enumObject,
                               QStringLiteral("displayValueText"), enumContext,
                               parsed.displayValueText, error)
                || !readBool(enumObject,
                             QStringLiteral("semanticAvailable"), enumContext,
                             parsed.semanticAvailable, error)
                || enumNames.contains(qString(parsed.name))) {
                if (error.isEmpty())
                    error = enumContext + QStringLiteral(" duplicates enum name");
                return false;
            }
            enumNames.insert(qString(parsed.name));
            leaf.enumValues.push_back(std::move(parsed));
        }
        if (paths.contains(qString(leaf.relativePath))) {
            error = leafContext + QStringLiteral(" duplicates relativePath");
            return false;
        }
        paths.insert(qString(leaf.relativePath));
        leaves.push_back(std::move(leaf));
    }
    return true;
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

bool parseManifestAssociations(
    const QJsonValue& value,
    const QString& context,
    std::vector<ModuleManifestAssociation>& output,
    QString& error)
{
    if (!value.isArray()) {
        error = context + QStringLiteral(" must be an array");
        return false;
    }
    QSet<QString> namedAssociations;
    for (qsizetype index = 0; index < value.toArray().size(); ++index) {
        const auto itemContext = QStringLiteral("%1[%2]").arg(context).arg(index);
        const auto item = value.toArray().at(index);
        if (!item.isObject()
            || !exactKeys(
                item.toObject(),
                {QStringLiteral("name"), QStringLiteral("position")},
                itemContext,
                error)) {
            if (error.isEmpty())
                error = itemContext + QStringLiteral(" must be an object");
            return false;
        }
        ModuleManifestAssociation association;
        int position = -1;
        if (!readString(item.toObject(), QStringLiteral("name"), itemContext,
                        association.name, error)
            || !readSignedInteger(item.toObject(), QStringLiteral("position"),
                                  itemContext, position, error)
            || position != index) {
            if (error.isEmpty()) {
                error = itemContext
                    + QStringLiteral(".position must match source order");
            }
            return false;
        }
        association.position = position;
        if (!association.name.empty()
            && namedAssociations.contains(qString(association.name))) {
            error = itemContext + QStringLiteral(" duplicates a formal name");
            return false;
        }
        if (!association.name.empty())
            namedAssociations.insert(qString(association.name));
        output.push_back(std::move(association));
    }
    return true;
}

int associationStyle(const std::vector<ModuleManifestAssociation>& associations)
{
    if (associations.empty()) return -1;
    const bool named = std::any_of(
        associations.cbegin(), associations.cend(),
        [](const ModuleManifestAssociation& association) {
            return !association.name.empty();
        });
    const bool positional = std::any_of(
        associations.cbegin(), associations.cend(),
        [](const ModuleManifestAssociation& association) {
            return association.name.empty();
        });
    return named && positional ? 2 : named ? 1 : 0;
}

bool parseUnresolvedDependencies(
    const QJsonValue& value,
    const QSet<QString>& sourcePaths,
    std::vector<ModuleManifestUnresolvedDependency>& output,
    QString& error)
{
    if (!value.isArray()) {
        error = QStringLiteral("manifest.unresolvedDependencies must be an array");
        return false;
    }
    QSet<QString> moduleNames;
    for (qsizetype dependencyIndex = 0;
         dependencyIndex < value.toArray().size(); ++dependencyIndex) {
        const QString context = QStringLiteral(
            "manifest.unresolvedDependencies[%1]").arg(dependencyIndex);
        const QJsonValue dependencyValue = value.toArray().at(dependencyIndex);
        if (!dependencyValue.isObject()
            || !exactKeys(
                dependencyValue.toObject(),
                {QStringLiteral("moduleName"), QStringLiteral("instances"),
                 QStringLiteral("stubSupported"),
                 QStringLiteral("stubUnsupportedReason")},
                context,
                error)) {
            if (error.isEmpty())
                error = context + QStringLiteral(" must be an object");
            return false;
        }
        const QJsonObject dependencyObject = dependencyValue.toObject();
        ModuleManifestUnresolvedDependency dependency;
        if (!readString(dependencyObject, QStringLiteral("moduleName"), context,
                        dependency.moduleName, error, false)
            || !readBool(dependencyObject, QStringLiteral("stubSupported"),
                         context, dependency.stubSupported, error)
            || !readString(dependencyObject,
                           QStringLiteral("stubUnsupportedReason"), context,
                           dependency.stubUnsupportedReason, error)) {
            return false;
        }
        if (moduleNames.contains(qString(dependency.moduleName))) {
            error = context + QStringLiteral(" duplicates moduleName");
            return false;
        }
        moduleNames.insert(qString(dependency.moduleName));

        const QJsonValue instancesValue =
            dependencyObject.value(QStringLiteral("instances"));
        if (!instancesValue.isArray() || instancesValue.toArray().isEmpty()) {
            error = context + QStringLiteral(".instances must be a non-empty array");
            return false;
        }
        int parameterStyle = -1;
        int portStyle = -1;
        for (qsizetype instanceIndex = 0;
             instanceIndex < instancesValue.toArray().size(); ++instanceIndex) {
            const QString instanceContext = QStringLiteral("%1.instances[%2]")
                                                .arg(context)
                                                .arg(instanceIndex);
            const QJsonValue instanceValue =
                instancesValue.toArray().at(instanceIndex);
            if (!instanceValue.isObject()
                || !exactKeys(
                    instanceValue.toObject(),
                    {QStringLiteral("instanceName"),
                     QStringLiteral("constructKind"),
                     QStringLiteral("sourceFile"),
                     QStringLiteral("sourceLine"),
                     QStringLiteral("sourceColumn"),
                     QStringLiteral("parameterAssociations"),
                     QStringLiteral("portAssociations"),
                     QStringLiteral("syntaxComplete"),
                     QStringLiteral("failureReason")},
                    instanceContext,
                    error)) {
                if (error.isEmpty())
                    error = instanceContext + QStringLiteral(" must be an object");
                return false;
            }
            const QJsonObject instanceObject = instanceValue.toObject();
            ModuleManifestUnresolvedInstance instance;
            if (!readString(instanceObject, QStringLiteral("instanceName"),
                            instanceContext, instance.instanceName, error)
                || !readString(instanceObject, QStringLiteral("constructKind"),
                               instanceContext, instance.constructKind, error,
                               false)
                || !readRelativePath(instanceObject,
                                     QStringLiteral("sourceFile"),
                                     instanceContext, instance.sourceFile,
                                     error)
                || !readPositiveInt(instanceObject,
                                    QStringLiteral("sourceLine"),
                                    instanceContext, instance.sourceLine, error)
                || !readPositiveInt(instanceObject,
                                    QStringLiteral("sourceColumn"),
                                    instanceContext, instance.sourceColumn,
                                    error)
                || !readBool(instanceObject,
                             QStringLiteral("syntaxComplete"), instanceContext,
                             instance.syntaxComplete, error)
                || !readString(instanceObject,
                               QStringLiteral("failureReason"), instanceContext,
                               instance.failureReason, error)
                || !parseManifestAssociations(
                    instanceObject.value(
                        QStringLiteral("parameterAssociations")),
                    instanceContext
                        + QStringLiteral(".parameterAssociations"),
                    instance.parameterAssociations, error)
                || !parseManifestAssociations(
                    instanceObject.value(QStringLiteral("portAssociations")),
                    instanceContext + QStringLiteral(".portAssociations"),
                    instance.portAssociations, error)) {
                return false;
            }
            if (instance.constructKind != "module"
                && instance.constructKind != "interface"
                && instance.constructKind != "program") {
                error = instanceContext
                    + QStringLiteral(".constructKind is unsupported");
                return false;
            }
            if (!sourcePaths.contains(qString(instance.sourceFile))) {
                error = instanceContext
                    + QStringLiteral(".sourceFile is absent from sources");
                return false;
            }
            const int instanceParameterStyle =
                associationStyle(instance.parameterAssociations);
            const int instancePortStyle =
                associationStyle(instance.portAssociations);
            const auto mergeStyle = [](const int candidate, int& merged) {
                if (candidate < 0) return true;
                if (candidate == 2) return false;
                if (merged < 0) {
                    merged = candidate;
                    return true;
                }
                return merged == candidate;
            };
            if (dependency.stubSupported
                && (instance.constructKind != "module"
                    || !instance.syntaxComplete
                    || !mergeStyle(instanceParameterStyle, parameterStyle)
                    || !mergeStyle(instancePortStyle, portStyle))) {
                error = context
                    + QStringLiteral(" claims an unsafe dependency is stub-capable");
                return false;
            }
            dependency.instances.push_back(std::move(instance));
        }
        if (dependency.stubSupported
                ? !dependency.stubUnsupportedReason.empty()
                : dependency.stubUnsupportedReason.empty()) {
            error = context
                + QStringLiteral(" has an inconsistent stub support reason");
            return false;
        }
        output.push_back(std::move(dependency));
    }
    return true;
}

} // namespace

std::string moduleManifestStructuredGroupId(
    const std::string_view manifestIdentity,
    const std::string_view rootPortName)
{
    return stableDigestId(
        "zs-port-group",
        std::string(manifestIdentity),
        std::string(rootPortName));
}

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
    const auto version = root.value(QStringLiteral("schemaVersion"));
    if (!version.isDouble()
        || std::floor(version.toDouble()) != version.toDouble()
        || version.toInt() < ZeroSlackModuleManifest::MinimumSupportedSchemaVersion
        || version.toInt() > ZeroSlackModuleManifest::CurrentSchemaVersion) {
        result.error = QStringLiteral("Unsupported ZeroSlack Module Manifest schemaVersion");
        return result;
    }
    const int schemaVersion = version.toInt();
    const bool hasObservationContract = schemaVersion >= 2;
    const bool hasUnresolvedDependencyContract = schemaVersion >= 4;
    const bool keysValid = hasUnresolvedDependencyContract
        ? exactKeys(
              root,
              {QStringLiteral("schemaVersion"), QStringLiteral("workspaceId"),
               QStringLiteral("target"), QStringLiteral("observationScope"),
               QStringLiteral("observations"), QStringLiteral("sources"),
               QStringLiteral("unresolvedDependencies"),
               QStringLiteral("includeDirs"), QStringLiteral("defines"),
               QStringLiteral("parameters"), QStringLiteral("ports"),
               QStringLiteral("clockCandidates"), QStringLiteral("resetCandidates")},
              QStringLiteral("manifest"),
              error)
        : hasObservationContract
        ? exactKeys(
              root,
              {QStringLiteral("schemaVersion"), QStringLiteral("workspaceId"),
               QStringLiteral("target"), QStringLiteral("observationScope"),
               QStringLiteral("observations"), QStringLiteral("sources"),
               QStringLiteral("includeDirs"), QStringLiteral("defines"),
               QStringLiteral("parameters"), QStringLiteral("ports"),
               QStringLiteral("clockCandidates"), QStringLiteral("resetCandidates")},
              QStringLiteral("manifest"),
              error)
        : exactKeys(
              root,
              {QStringLiteral("schemaVersion"), QStringLiteral("workspaceId"),
               QStringLiteral("target"), QStringLiteral("sources"),
               QStringLiteral("includeDirs"), QStringLiteral("defines"),
               QStringLiteral("parameters"), QStringLiteral("ports"),
               QStringLiteral("clockCandidates"), QStringLiteral("resetCandidates")},
              QStringLiteral("manifest"),
              error);
    if (!keysValid) {
        result.error = error;
        return result;
    }

    ZeroSlackModuleManifest manifest;
    manifest.schemaVersion = schemaVersion;
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

    if (hasObservationContract) {
        const auto scopeValue = root.value(QStringLiteral("observationScope"));
        if (!scopeValue.isObject()) {
            result.error = QStringLiteral("manifest.observationScope must be an object");
            return result;
        }
        const auto scope = scopeValue.toObject();
        if (!exactKeys(
                scope,
                {QStringLiteral("mode"), QStringLiteral("label"),
                 QStringLiteral("sourceFile"), QStringLiteral("startLine"),
                 QStringLiteral("endLine")},
                QStringLiteral("manifest.observationScope"),
                error)) {
            result.error = error;
            return result;
        }
        std::string scopeMode;
        std::uint64_t startLine = 0;
        std::uint64_t endLine = 0;
        if (!readString(scope, QStringLiteral("mode"),
                        QStringLiteral("manifest.observationScope"),
                        scopeMode, error, false)
            || !readString(scope, QStringLiteral("label"),
                           QStringLiteral("manifest.observationScope"),
                           manifest.observationScope.label, error)
            || !readRelativePath(scope, QStringLiteral("sourceFile"),
                                 QStringLiteral("manifest.observationScope"),
                                 manifest.observationScope.sourceFile, error)
            || !readInteger(scope, QStringLiteral("startLine"),
                            QStringLiteral("manifest.observationScope"),
                            startLine, error)
            || !readInteger(scope, QStringLiteral("endLine"),
                            QStringLiteral("manifest.observationScope"),
                            endLine, error)
            || startLine > static_cast<std::uint64_t>(std::numeric_limits<int>::max())
            || endLine > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            result.error = error.isEmpty()
                ? QStringLiteral("manifest.observationScope line range is unsupported")
                : error;
            return result;
        }
        manifest.observationScope.startLine = static_cast<int>(startLine);
        manifest.observationScope.endLine = static_cast<int>(endLine);
        if (scopeMode == "module") {
            manifest.observationScope.mode =
                ModuleManifestObservationScopeMode::Module;
        } else if (scopeMode == "always" && startLine > 0
                   && endLine >= startLine) {
            manifest.observationScope.mode =
                ModuleManifestObservationScopeMode::Always;
        } else {
            result.error = QStringLiteral(
                "manifest.observationScope mode or line range is unsupported");
            return result;
        }

        const auto observations = root.value(QStringLiteral("observations"));
        if (!observations.isArray()) {
            result.error = QStringLiteral("manifest.observations must be an array");
            return result;
        }
        QSet<QString> observationPaths;
        for (qsizetype index = 0; index < observations.toArray().size(); ++index) {
            const auto value = observations.toArray().at(index);
            const auto context = QStringLiteral("manifest.observations[%1]").arg(index);
            if (!value.isObject()) {
                result.error = context + QStringLiteral(" must be an object");
                return result;
            }
            const auto object = value.toObject();
            if (!exactKeys(
                    object,
                    {QStringLiteral("name"), QStringLiteral("accessPath"),
                     QStringLiteral("semanticId"), QStringLiteral("declarationText"),
                     QStringLiteral("type"), QStringLiteral("sourceFile"),
                     QStringLiteral("sourceLine"), QStringLiteral("port")},
                    context,
                    error)) {
                result.error = error;
                return result;
            }
            ModuleManifestObservation observation;
            if (!readString(object, QStringLiteral("name"), context,
                            observation.name, error, false)
                || !readString(object, QStringLiteral("accessPath"), context,
                               observation.accessPath, error, false)
                || !readString(object, QStringLiteral("semanticId"), context,
                               observation.semanticId, error, false)
                || !readString(object, QStringLiteral("declarationText"), context,
                               observation.declarationText, error)
                || !readRelativePath(object, QStringLiteral("sourceFile"), context,
                                    observation.sourceFile, error)
                || !readPositiveInt(object, QStringLiteral("sourceLine"), context,
                                   observation.sourceLine, error)
                || !readBool(object, QStringLiteral("port"), context,
                             observation.port, error)) {
                result.error = error;
                return result;
            }
            if (!object.value(QStringLiteral("type")).isObject()
                || !parseType(object.value(QStringLiteral("type")).toObject(),
                              context + QStringLiteral(".type"),
                              observation.type, error)) {
                result.error = error.isEmpty()
                    ? context + QStringLiteral(".type must be an object")
                    : error;
                return result;
            }
            const QString accessPath = qString(observation.accessPath);
            if (observationPaths.contains(accessPath)) {
                result.error = context + QStringLiteral(" duplicates accessPath ")
                    + accessPath;
                return result;
            }
            observationPaths.insert(accessPath);
            manifest.observations.push_back(std::move(observation));
        }
    } else {
        manifest.observationScope.mode =
            ModuleManifestObservationScopeMode::Module;
        manifest.observationScope.sourceFile = manifest.target.sourceFile;
        manifest.observationScope.startLine = manifest.target.sourceLine;
        manifest.observationScope.endLine = manifest.target.sourceLine;
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
    if (hasObservationContract
        && !sourcePaths.contains(qString(manifest.observationScope.sourceFile))) {
        result.error = QStringLiteral(
            "manifest.observationScope.sourceFile is absent from sources");
        return result;
    }
    for (const auto& observation : manifest.observations) {
        if (!sourcePaths.contains(qString(observation.sourceFile))) {
            result.error = QStringLiteral(
                "manifest observation sourceFile is absent from sources");
            return result;
        }
    }
    if (hasUnresolvedDependencyContract
        && !parseUnresolvedDependencies(
            root.value(QStringLiteral("unresolvedDependencies")),
            sourcePaths,
            manifest.unresolvedDependencies,
            error)) {
        result.error = error;
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
        const bool portKeysValid = schemaVersion >= 3
            ? exactKeys(
                  port,
                  {QStringLiteral("name"), QStringLiteral("direction"),
                   QStringLiteral("declarationText"), QStringLiteral("type"),
                   QStringLiteral("structuredLeavesAvailable"),
                   QStringLiteral("editableLeaves"),
                   QStringLiteral("structuredFailureReason"),
                   QStringLiteral("sourceFile"), QStringLiteral("sourceLine")},
                  context,
                  error)
            : exactKeys(
                  port,
                  {QStringLiteral("name"), QStringLiteral("direction"),
                   QStringLiteral("declarationText"), QStringLiteral("type"),
                   QStringLiteral("sourceFile"), QStringLiteral("sourceLine")},
                  context,
                  error);
        if (!portKeysValid) {
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
        if (schemaVersion >= 3) {
            if (!readBool(port,
                          QStringLiteral("structuredLeavesAvailable"),
                          context,
                          parsedPort.structuredLeavesAvailable,
                          error)
                || !readString(port,
                               QStringLiteral("structuredFailureReason"),
                               context,
                               parsedPort.structuredFailureReason,
                               error)
                || !parseEditableLeaves(
                    port.value(QStringLiteral("editableLeaves")),
                    context + QStringLiteral(".editableLeaves"),
                    parsedPort.direction,
                    parsedPort.editableLeaves,
                    error)) {
                result.error = error;
                return result;
            }
            if (parsedPort.structuredLeavesAvailable
                != !parsedPort.editableLeaves.empty()) {
                result.error = context
                    + QStringLiteral(
                        ".structuredLeavesAvailable does not match editableLeaves");
                return result;
            }
            if (parsedPort.structuredLeavesAvailable
                && !parsedPort.structuredFailureReason.empty()) {
                result.error = context
                    + QStringLiteral(
                        ".structuredFailureReason must be empty when leaves are available");
                return result;
            }
        }
        if (portNames.contains(qString(parsedPort.name))) {
            result.error = QStringLiteral("manifest.ports contains duplicate name %1")
                               .arg(qString(parsedPort.name));
            return result;
        }
        portNames.insert(qString(parsedPort.name));
        manifest.ports.push_back(std::move(parsedPort));
    }
    for (const auto& observation : manifest.observations) {
        if (observation.port
            && !portNames.contains(qString(observation.name))) {
            result.error = QStringLiteral(
                "manifest port observation references unknown port %1")
                               .arg(qString(observation.name));
            return result;
        }
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
    if (manifest.schemaVersion
            < ZeroSlackModuleManifest::MinimumSupportedSchemaVersion
        || manifest.schemaVersion
            > ZeroSlackModuleManifest::CurrentSchemaVersion) {
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
            QStringLiteral("Multiple clock candidates will be imported as independent clock domains (%1).")
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
        "waveSimulation.moduleManifestSchemaVersion",
        std::to_string(manifest.schemaVersion));
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
    lanes.reserve(manifest.ports.size() + manifest.observations.size());
    const bool focusedObservationScope =
        manifest.schemaVersion >= 2
        && manifest.observationScope.mode
            == ModuleManifestObservationScopeMode::Always;
    QSet<QString> observedPortNames;
    for (const auto& observation : manifest.observations) {
        if (observation.port)
            observedPortNames.insert(qString(observation.name));
    }

    for (std::size_t index = 0; index < manifest.ports.size(); ++index) {
        const auto& port = manifest.ports[index];
        const auto& shape = port.type.shape;
        const auto baseSourceOrder = index;
        if (port.structuredLeavesAvailable) {
            bool anyStimulus = false;
            bool anyWatch = false;
            for (const auto& leaf : port.editableLeaves) {
                anyStimulus = anyStimulus || stimulusDirection(leaf.direction);
                anyWatch = anyWatch || watchDirection(leaf.direction);
            }

            Lane group;
            group.id = moduleManifestStructuredGroupId(
                manifest.identity, port.name);
            group.name = port.name;
            group.kind = LaneKind::Group;
            group.color = "#90a4ae";
            group.height = 40;
            group.visible = true;
            group.extensions.emplace(
                "sourceApplication", jsonStringValue("ZeroSlack"));
            group.extensions.emplace(
                "waveSimulation.moduleManifestIdentity",
                jsonStringValue(manifest.identity));
            group.extensions.emplace("waveSimulation.structuredGroup", "true");
            group.extensions.emplace(
                "waveSimulation.rootPortName", jsonStringValue(port.name));
            group.extensions.emplace(
                "waveSimulation.sourceOrder", std::to_string(baseSourceOrder));

            int priority = anyStimulus && anyWatch ? 3 : anyStimulus ? 2 : 4;
            lanes.push_back({priority, baseSourceOrder, group, false, false});

            for (std::size_t leafIndex = 0;
                 leafIndex < port.editableLeaves.size(); ++leafIndex) {
                const auto& leaf = port.editableLeaves[leafIndex];
                const auto stimulus = stimulusDirection(leaf.direction);
                const auto watch = watchDirection(leaf.direction);
                const auto sourceOrder = baseSourceOrder;
                const auto leafName = port.name + leaf.relativePath;
                const auto role = stimulus && watch
                    ? std::string("stimulus-watch")
                    : stimulus ? std::string("stimulus")
                               : std::string("watch");

                Lane lane;
                lane.id = stableDigestId(
                    "zs-port-leaf", manifest.identity, leafName);
                lane.name = leafName;
                lane.width = static_cast<std::uint32_t>(leaf.type.bitWidth);
                lane.isSigned = leaf.type.isSigned;
                lane.kind = !leaf.enumValues.empty()
                        || leaf.type.semanticKind == "enum"
                    ? LaneKind::Enum
                    : lane.width == 1 ? LaneKind::Bit : LaneKind::Bus;
                lane.radix = Radix::Hexadecimal;
                lane.height = 56;
                lane.visible = !focusedObservationScope
                    || stimulus
                    || observedPortNames.contains(qString(port.name));
                lane.groupId = group.id;
                lane.color = watch && stimulus
                    ? "#ce93d8"
                    : watch ? "#ffb74d" : "#64b5f6";
                lane.extensions.emplace(
                    "sourceApplication", jsonStringValue("ZeroSlack"));
                lane.extensions.emplace(
                    "waveSimulation.moduleManifestIdentity",
                    jsonStringValue(manifest.identity));
                lane.extensions.emplace(
                    "waveSimulation.role", jsonStringValue(role));
                lane.extensions.emplace(
                    "waveSimulation.direction",
                    jsonStringValue(std::string(toString(leaf.direction))));
                lane.extensions.emplace(
                    "waveSimulation.declarationText",
                    jsonStringValue(port.declarationText));
                lane.extensions.emplace(
                    "waveSimulation.sourceFile", jsonStringValue(port.sourceFile));
                lane.extensions.emplace(
                    "waveSimulation.sourceLine", std::to_string(port.sourceLine));
                lane.extensions.emplace(
                    "waveSimulation.canonicalTypeId",
                    jsonStringValue(leaf.type.canonicalTypeId));
                lane.extensions.emplace(
                    "waveSimulation.declarationShapeId",
                    jsonStringValue(leaf.type.declarationShapeId));
                lane.extensions.emplace(
                    "waveSimulation.sourceOrder", std::to_string(sourceOrder));
                lane.extensions.emplace(
                    "waveSimulation.resolvedTypeText",
                    jsonStringValue(leaf.type.resolvedTypeText));
                lane.extensions.emplace(
                    "waveSimulation.typedefChain",
                    jsonStringArrayValue(leaf.type.typedefChain));
                lane.extensions.emplace("waveSimulation.clockCandidate", "false");
                lane.extensions.emplace("waveSimulation.resetCandidate", "false");
                lane.extensions.emplace("waveSimulation.structured", "true");
                lane.extensions.emplace(
                    "waveSimulation.rootPortName", jsonStringValue(port.name));
                lane.extensions.emplace(
                    "waveSimulation.structuredRelativePath",
                    jsonStringValue(leaf.relativePath));
                lane.extensions.emplace(
                    "waveSimulation.structuredSelectors",
                    jsonStructuredSelectorsValue(leaf.selectors));
                lane.extensions.emplace(
                    "waveSimulation.packedBitOffsetValid",
                    leaf.packedBitOffsetValid ? "true" : "false");
                lane.extensions.emplace(
                    "waveSimulation.packedBitOffset",
                    std::to_string(leaf.packedBitOffset));
                lane.extensions.emplace(
                    "waveSimulation.rootDirection",
                    jsonStringValue(std::string(toString(port.direction))));
                lane.extensions.emplace(
                    "waveSimulation.rootCanonicalTypeId",
                    jsonStringValue(shape.canonicalTypeId));
                lane.extensions.emplace(
                    "waveSimulation.rootDeclarationShapeId",
                    jsonStringValue(shape.declarationShapeId));
                lane.extensions.emplace(
                    "waveSimulation.interfaceName",
                    jsonStringValue(shape.interfaceName));
                lane.extensions.emplace(
                    "waveSimulation.modportName",
                    jsonStringValue(shape.modportName));
                lane.extensions.emplace(
                    "waveSimulation.traceName",
                    jsonStringValue(structuredTraceName(index, leafIndex)));

                for (const auto& value : leaf.enumValues) {
                    const auto normalized = normalizedEnumValue(value);
                    if (!value.semanticAvailable || !normalized) {
                        result.diagnostics.append(
                            QStringLiteral("Enum value %1 for structured input %2 could not be normalized and was omitted.")
                                .arg(qString(value.name), qString(leafName)));
                        continue;
                    }
                    lane.enumMap.emplace(value.name, *normalized);
                }
                if (stimulus) {
                    setSegmentRange(
                        lane,
                        0,
                        options.duration,
                        lane.kind == LaneKind::Bit ? std::string("0")
                                                   : std::string("0x0"),
                        stableDigestId(
                            "zs-segment", manifest.identity, leafName));
                }
                lanes.push_back({
                    priority, sourceOrder, std::move(lane), stimulus, watch});
            }
            result.diagnostics.append(
                QStringLiteral("Port %1 was imported as %2 structured leaf lanes.")
                    .arg(qString(port.name))
                    .arg(port.editableLeaves.size()));
            continue;
        }
        if (!port.structuredFailureReason.empty()) {
            result.diagnostics.append(
                QStringLiteral("Port %1 cannot be edited as structured data: %2")
                    .arg(qString(port.name),
                         qString(port.structuredFailureReason)));
            continue;
        }
        if (port.direction == ModulePortDirection::Interface
            || shape.interfaceType) {
            result.diagnostics.append(
                QStringLiteral("Port %1 cannot be edited as an interface: %2")
                    .arg(qString(port.name),
                         qString(port.structuredFailureReason.empty()
                                     ? std::string("structured member facts are unavailable")
                                     : port.structuredFailureReason)));
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
                QStringLiteral("Port %1 cannot be edited as an unpacked array: %2")
                    .arg(qString(port.name),
                         qString(port.structuredFailureReason.empty()
                                     ? std::string("structured element facts are unavailable")
                                     : port.structuredFailureReason)));
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
        lane.visible = !focusedObservationScope
            || stimulus
            || observedPortNames.contains(qString(port.name));

        const bool selectedClock = std::find(
            manifest.clockCandidates.cbegin(),
            manifest.clockCandidates.cend(),
            port.name) != manifest.clockCandidates.cend();
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
            "waveSimulation.declarationShapeId",
            jsonStringValue(shape.declarationShapeId));
        lane.extensions.emplace(
            "waveSimulation.sourceOrder",
            std::to_string(index));
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
                if (result.clockSuggestion.state == ModuleCandidateState::Unique)
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
            priority, baseSourceOrder, std::move(lane), stimulus, watch});
    }

    QSet<QString> importedObservationPaths;
    for (std::size_t index = 0; index < manifest.observations.size(); ++index) {
        const auto& observation = manifest.observations[index];
        if (observation.port) continue;
        const auto& shape = observation.type.shape;
        if (!shape.semanticAvailable || !shape.fixedSize || !shape.integral
            || shape.unpackedArray || shape.interfaceType || shape.bitWidth == 0
            || shape.bitWidth > std::numeric_limits<std::uint32_t>::max()) {
            result.diagnostics.append(
                QStringLiteral("Observation %1 has no supported fixed integral width and was not imported.")
                    .arg(qString(observation.accessPath)));
            continue;
        }
        const QString accessPath = qString(observation.accessPath);
        if (importedObservationPaths.contains(accessPath)) continue;
        importedObservationPaths.insert(accessPath);

        Lane lane;
        lane.id = stableDigestId(
            "zs-observation",
            manifest.identity,
            observation.semanticId + "\n" + observation.accessPath);
        lane.name = observation.accessPath;
        lane.width = static_cast<std::uint32_t>(shape.bitWidth);
        lane.isSigned = shape.isSigned;
        lane.kind = !observation.type.enumValues.empty()
                || shape.semanticKind == "enum"
            ? LaneKind::Enum
            : lane.width == 1 ? LaneKind::Bit : LaneKind::Bus;
        lane.radix = Radix::Hexadecimal;
        lane.height = 56;
        lane.visible = true;
        lane.color = "#ffb74d";
        lane.extensions.emplace("sourceApplication", jsonStringValue("ZeroSlack"));
        lane.extensions.emplace(
            "waveSimulation.moduleManifestIdentity",
            jsonStringValue(manifest.identity));
        lane.extensions.emplace("waveSimulation.role", jsonStringValue("watch"));
        lane.extensions.emplace("waveSimulation.direction", jsonStringValue("output"));
        lane.extensions.emplace(
            "waveSimulation.observation", "true");
        lane.extensions.emplace(
            "waveSimulation.accessPath",
            jsonStringValue(observation.accessPath));
        lane.extensions.emplace(
            "waveSimulation.semanticId",
            jsonStringValue(observation.semanticId));
        lane.extensions.emplace(
            "waveSimulation.declarationText",
            jsonStringValue(observation.declarationText));
        lane.extensions.emplace(
            "waveSimulation.sourceFile",
            jsonStringValue(observation.sourceFile));
        lane.extensions.emplace(
            "waveSimulation.sourceLine",
            std::to_string(observation.sourceLine));
        lane.extensions.emplace(
            "waveSimulation.canonicalTypeId",
            jsonStringValue(shape.canonicalTypeId));
        lane.extensions.emplace(
            "waveSimulation.declarationShapeId",
            jsonStringValue(shape.declarationShapeId));
        lane.extensions.emplace(
            "waveSimulation.sourceOrder",
            std::to_string(manifest.ports.size() + index));
        lane.extensions.emplace(
            "waveSimulation.resolvedTypeText",
            jsonStringValue(shape.resolvedTypeText));
        lane.extensions.emplace(
            "waveSimulation.typedefChain",
            jsonStringArrayValue(shape.typedefChain));
        lane.extensions.emplace("waveSimulation.clockCandidate", "false");
        lane.extensions.emplace("waveSimulation.resetCandidate", "false");
        for (const auto& value : observation.type.enumValues) {
            const auto normalized = normalizedEnumValue(value);
            if (value.semanticAvailable && normalized)
                lane.enumMap.emplace(value.name, *normalized);
        }
        lanes.push_back({
            4,
            manifest.ports.size() + index,
            std::move(lane),
            false,
            true});
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
