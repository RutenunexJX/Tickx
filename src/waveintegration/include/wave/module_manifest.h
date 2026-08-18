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

struct ModuleManifestSource {
    std::string path;
    std::string role;
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

struct ModuleManifestPort {
    std::string name;
    ModulePortDirection direction{ModulePortDirection::Unknown};
    std::string declarationText;
    ModuleManifestType type;
    std::string sourceFile;
    int sourceLine{0};
};

struct ModuleManifestTarget {
    ModuleManifestTargetMode mode{ModuleManifestTargetMode::ModuleDefinition};
    std::string module;
    std::string instancePath;
    std::string sourceFile;
    int sourceLine{0};
};

struct ZeroSlackModuleManifest {
    static constexpr int CurrentSchemaVersion = 1;

    int schemaVersion{CurrentSchemaVersion};
    std::string identity;
    std::string workspaceId;
    ModuleManifestTarget target;
    std::vector<ModuleManifestSource> sources;
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

[[nodiscard]] std::string_view toString(ModulePortDirection direction) noexcept;
[[nodiscard]] std::string_view toString(ModuleCandidateState state) noexcept;

} // namespace wave
