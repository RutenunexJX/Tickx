#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace wave {

inline constexpr auto ToolchainProbeReportSchema =
    "wave-workbench.toolchain-probe/v1";

enum class ProcessRunState {
    Succeeded,
    ProgramNotFound,
    StartFailed,
    NonZeroExit,
    Crashed,
    TimedOut,
    Cancelled,
};

struct ProcessRunRequest {
    QString program;
    QStringList arguments;
    QString workingDirectory;
    QProcessEnvironment environment{QProcessEnvironment::systemEnvironment()};
    bool inheritCurrentProcessPath{true};
    int timeoutMs{5'000};
    int maxOutputBytes{256 * 1024};
};

struct ProcessRunResult {
    ProcessRunState state{ProcessRunState::StartFailed};
    QString requestedProgram;
    QString resolvedProgram;
    QStringList arguments;
    QByteArray standardOutput;
    QByteArray standardError;
    QString errorMessage;
    int exitCode{-1};
    qint64 durationMs{0};
    bool standardOutputTruncated{false};
    bool standardErrorTruncated{false};

    [[nodiscard]] bool ok() const noexcept
    {
        return state == ProcessRunState::Succeeded;
    }
};

class ProcessRunner final {
public:
    using Completion = std::function<void(ProcessRunResult)>;

    ProcessRunner();
    ~ProcessRunner();

    ProcessRunner(const ProcessRunner&) = delete;
    ProcessRunner& operator=(const ProcessRunner&) = delete;
    ProcessRunner(ProcessRunner&&) = delete;
    ProcessRunner& operator=(ProcessRunner&&) = delete;

    [[nodiscard]] bool start(ProcessRunRequest request, Completion completion);
    [[nodiscard]] bool cancel();
    [[nodiscard]] bool running() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct ToolVersion {
    int major{-1};
    int minor{-1};
    int patch{-1};
    QString sourceText;

    [[nodiscard]] bool valid() const noexcept
    {
        return major >= 0 && minor >= 0 && patch >= 0;
    }

    bool operator==(const ToolVersion&) const = default;
};

enum class ToolKind {
    Verilator,
    CxxCompiler,
};

enum class CxxCompilerFamily {
    NotApplicable,
    Gcc,
    Clang,
    Msvc,
    Unknown,
};

enum class ToolProbeStatus {
    Ready,
    NotFound,
    IncompatibleVersion,
    VersionUnrecognized,
    ExecutionFailed,
    TimedOut,
    Cancelled,
};

enum class ToolchainProbeStatus {
    Ready,
    Unavailable,
    Incompatible,
    Failed,
    TimedOut,
    Cancelled,
};

struct ToolchainRequirements {
    ToolVersion minimumVerilator{5, 0, 0, QStringLiteral("5.0.0")};
    ToolVersion minimumGcc{8, 0, 0, QStringLiteral("8.0.0")};
    ToolVersion minimumClang{7, 0, 0, QStringLiteral("7.0.0")};
    ToolVersion minimumMsvc{19, 20, 0, QStringLiteral("19.20.0")};
};

struct ToolchainProbeOptions {
    QString verilatorProgram;
    QStringList verilatorArguments;
    QString cxxProgram;
    QStringList cxxArguments;
    QProcessEnvironment environment{QProcessEnvironment::systemEnvironment()};
    bool inheritCurrentProcessPath{true};
    ToolchainRequirements requirements;
    int timeoutMs{5'000};
    int maxOutputBytes{256 * 1024};
};

struct ToolProbeResult {
    ToolKind kind{ToolKind::Verilator};
    ToolProbeStatus status{ToolProbeStatus::ExecutionFailed};
    CxxCompilerFamily compilerFamily{CxxCompilerFamily::NotApplicable};
    std::optional<ToolVersion> version;
    ToolVersion minimumVersion;
    QString diagnostic;
    ProcessRunResult process;

    [[nodiscard]] bool ready() const noexcept
    {
        return status == ToolProbeStatus::Ready;
    }
};

struct ToolchainProbeReport {
    ToolchainProbeStatus status{ToolchainProbeStatus::Failed};
    ToolProbeResult verilator;
    ToolProbeResult cxxCompiler;
    qint64 durationMs{0};

    [[nodiscard]] bool ready() const noexcept
    {
        return status == ToolchainProbeStatus::Ready;
    }
};

class ToolchainProbeRunner final {
public:
    using Completion = std::function<void(ToolchainProbeReport)>;

    ToolchainProbeRunner();
    ~ToolchainProbeRunner();

    ToolchainProbeRunner(const ToolchainProbeRunner&) = delete;
    ToolchainProbeRunner& operator=(const ToolchainProbeRunner&) = delete;
    ToolchainProbeRunner(ToolchainProbeRunner&&) = delete;
    ToolchainProbeRunner& operator=(ToolchainProbeRunner&&) = delete;

    [[nodiscard]] bool start(
        ToolchainProbeOptions options,
        Completion completion);
    [[nodiscard]] bool cancel();
    [[nodiscard]] bool running() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] QJsonObject toolchainProbeReportJson(
    const ToolchainProbeReport& report);

[[nodiscard]] std::string_view toString(ProcessRunState state) noexcept;
[[nodiscard]] std::string_view toString(ToolKind kind) noexcept;
[[nodiscard]] std::string_view toString(CxxCompilerFamily family) noexcept;
[[nodiscard]] std::string_view toString(ToolProbeStatus status) noexcept;
[[nodiscard]] std::string_view toString(ToolchainProbeStatus status) noexcept;

} // namespace wave
