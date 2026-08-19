#pragma once

#include "wave/model.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wave {

enum class ModuleManifestTargetMode {
    ModuleDefinition,
    Instance,
};

enum class ModuleManifestObservationScopeMode {
    Module,
    Always,
};

enum class ModulePortDirection {
    Input,
    Output,
    Inout,
    Ref,
    Interface,
    Unknown,
};

enum class ModuleCandidateState {
    None,
    Unique,
    Ambiguous,
};

enum class ModuleManifestStructuredSelectorKind {
    StructMember,
    PackedIndex,
    UnpackedIndex,
    InterfaceMember,
};

struct ModuleManifestSource {
    std::string path;
    std::string role;
};

struct ModuleManifestAssociation {
    std::string name;
    int position{0};
};

struct ModuleManifestUnresolvedInstance {
    std::string instanceName;
    std::string constructKind;
    std::string sourceFile;
    int sourceLine{0};
    int sourceColumn{0};
    std::vector<ModuleManifestAssociation> parameterAssociations;
    std::vector<ModuleManifestAssociation> portAssociations;
    bool syntaxComplete{false};
    std::string failureReason;
};

struct ModuleManifestUnresolvedDependency {
    std::string moduleName;
    std::vector<ModuleManifestUnresolvedInstance> instances;
    bool stubSupported{false};
    std::string stubUnsupportedReason;
};

struct ModuleManifestTypeShape {
    bool semanticAvailable{false};
    std::string rawTypeText;
    std::string resolvedTypeName;
    std::string semanticKind;
    std::string resolvedTypeText;
    std::string canonicalTypeId;
    std::string declarationShapeId;
    bool fixedSize{false};
    bool integral{false};
    bool isSigned{false};
    bool unpackedArray{false};
    bool interfaceType{false};
    std::uint64_t bitWidth{0};
    std::string packedDimensions;
    std::string unpackedDimensions;
    std::string unpackedElementCount;
    std::string interfaceName;
    std::string modportName;
    std::vector<std::string> typedefChain;
    std::string failureReason;
};

struct ModuleManifestEnumValue {
    std::string name;
    std::string declarationText;
    std::string valueText;
    std::string displayValueText;
    bool semanticAvailable{false};
};

struct ModuleManifestStructMember {
    std::string name;
    std::string declarationText;
    ModuleManifestTypeShape type;
};

struct ModuleManifestStructuredSelector {
    ModuleManifestStructuredSelectorKind kind{
        ModuleManifestStructuredSelectorKind::StructMember};
    std::string name;
    int sourceIndex{0};
    int storageIndex{0};

    [[nodiscard]] bool operator==(
        const ModuleManifestStructuredSelector&) const = default;
};

struct ModuleManifestEditableLeaf {
    std::string relativePath;
    ModulePortDirection direction{ModulePortDirection::Unknown};
    bool inheritsPortDirection{true};
    std::vector<ModuleManifestStructuredSelector> selectors;
    ModuleManifestTypeShape type;
    std::vector<ModuleManifestEnumValue> enumValues;
    bool packedBitOffsetValid{false};
    std::uint64_t packedBitOffset{0};
};

struct ModuleManifestType {
    ModuleManifestTypeShape shape;
    std::vector<ModuleManifestEnumValue> enumValues;
    std::vector<ModuleManifestStructMember> structMembers;
};

struct ModuleManifestParameter {
    std::string name;
    std::string declarationText;
    std::string expressionText;
    std::string valueText;
    std::string displayValueText;
    bool semanticAvailable{false};
    ModuleManifestType type;
    std::string sourceFile;
    int sourceLine{0};
};

struct ModuleManifestSourceLink {
    std::string kind;
    std::string sourceFile;
    int sourceLine{0};
    int sourceColumn{0};
    std::string label;
};

struct ModuleManifestPort {
    std::string name;
    std::string semanticId;
    ModulePortDirection direction{ModulePortDirection::Unknown};
    std::string declarationText;
    ModuleManifestType type;
    bool structuredLeavesAvailable{false};
    std::vector<ModuleManifestEditableLeaf> editableLeaves;
    std::string structuredFailureReason;
    std::string sourceFile;
    int sourceLine{0};
    int sourceColumn{1};
    std::vector<ModuleManifestSourceLink> sourceLinks;
};

struct ModuleManifestTarget {
    ModuleManifestTargetMode mode{ModuleManifestTargetMode::ModuleDefinition};
    std::string module;
    std::string instancePath;
    std::string sourceFile;
    int sourceLine{0};
};

struct ModuleManifestObservationScope {
    ModuleManifestObservationScopeMode mode{
        ModuleManifestObservationScopeMode::Module};
    std::string label;
    std::string sourceFile;
    int startLine{0};
    int endLine{0};
};

struct ModuleManifestObservation {
    std::string name;
    std::string accessPath;
    std::string semanticId;
    std::string declarationText;
    ModuleManifestType type;
    std::string sourceFile;
    int sourceLine{0};
    int sourceColumn{1};
    std::vector<ModuleManifestSourceLink> sourceLinks;
    bool port{false};
};

struct ZeroSlackModuleManifest {
    static constexpr int MinimumSupportedSchemaVersion = 1;
    static constexpr int CurrentSchemaVersion = 5;

    int schemaVersion{CurrentSchemaVersion};
    std::string identity;
    std::string workspaceId;
    ModuleManifestTarget target;
    ModuleManifestObservationScope observationScope;
    std::vector<ModuleManifestObservation> observations;
    std::vector<ModuleManifestSource> sources;
    std::vector<ModuleManifestUnresolvedDependency> unresolvedDependencies;
    std::vector<std::string> includeDirs;
    std::map<std::string, std::string> defines;
    std::vector<ModuleManifestParameter> parameters;
    std::vector<ModuleManifestPort> ports;
    std::vector<std::string> clockCandidates;
    std::vector<std::string> resetCandidates;
};

struct ModuleManifestParseResult {
    std::optional<ZeroSlackModuleManifest> manifest;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return manifest.has_value(); }
};

struct ModuleCandidateSuggestion {
    ModuleCandidateState state{ModuleCandidateState::None};
    std::vector<std::string> candidates;
    std::string selectedPortName;
    std::string selectedLaneId;
};

struct ModuleManifestImportOptions {
    Tick duration{200'000};
    Tick defaultClockPeriod{10'000};
};

struct ModuleManifestImportResult {
    std::optional<Project> project;
    ModuleCandidateSuggestion clockSuggestion;
    ModuleCandidateSuggestion resetSuggestion;
    std::vector<std::string> stimulusLaneIds;
    std::vector<std::string> watchLaneIds;
    QStringList diagnostics;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return project.has_value(); }
};

[[nodiscard]] ModuleManifestParseResult parseZeroSlackModuleManifest(
    const QByteArray& document);
[[nodiscard]] ModuleManifestImportResult importZeroSlackModuleManifest(
    const ZeroSlackModuleManifest& manifest,
    const ModuleManifestImportOptions& options = {});
[[nodiscard]] std::string moduleManifestStructuredGroupId(
    std::string_view manifestIdentity,
    std::string_view rootPortName);

[[nodiscard]] std::string_view toString(ModulePortDirection direction) noexcept;
[[nodiscard]] std::string_view toString(ModuleCandidateState state) noexcept;

} // namespace wave
