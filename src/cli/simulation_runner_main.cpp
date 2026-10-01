#include "wave/simulation_pipeline.h"
#include "wave/simulation_runner.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>

#include <optional>

namespace {

void usage(QTextStream& stream)
{
    stream
        << "Tickx simulation runner (experimental)\n"
           "Usage:\n"
           "  wave-sim-runner probe [--verilator=PATH] [--cxx=PATH] "
           "[--timeout-ms=N] [--cancel-after-ms=N] [--pretty]\n"
           "  wave-sim-runner run-fixture --manifest=FILE --stimulus=FILE "
           "--workspace=DIR --artifacts=DIR [--build-cache=DIR] "
           "[--scenario-directory=DIR] "
           "[--verilator=PATH] [--cxx=PATH] "
           "[--probe-timeout-ms=N] [--build-timeout-ms=N] "
           "[--run-timeout-ms=N] [--cancel-after-ms=N] [--pretty]\n\n"
           "  wave-sim-runner run-module --manifest=FILE --stimulus=FILE "
           "--workspace=DIR --artifacts=DIR --result-project=FILE "
           "[--build-cache=DIR] [--generation=N] "
           "[--scenario-directory=DIR] "
           "[--verilator=PATH] [--cxx=PATH] [--probe-timeout-ms=N] "
           "[--build-timeout-ms=N] [--run-timeout-ms=N] "
           "[--cancel-after-ms=N] [--pretty]\n\n"
           "run-fixture is an experimental fixed-contract path; it is not a GUI entry.\n"
           "Exit codes: 0 success, 2 usage, 3 unavailable, 4 invalid contract, "
           "5 failed, 6 timed out, 7 cancelled, 8 superseded.\n";
}

std::optional<int> positiveInteger(
    const QString& argument,
    const QString& prefix)
{
    if (!argument.startsWith(prefix)) return std::nullopt;
    bool valid = false;
    const auto value = argument.mid(prefix.size()).toInt(&valid);
    return valid && value > 0 ? std::optional<int>{value} : std::nullopt;
}

std::optional<QString> nonEmptyValue(
    const QString& argument,
    const QString& prefix)
{
    if (!argument.startsWith(prefix)) return std::nullopt;
    const auto value = argument.mid(prefix.size());
    return value.isEmpty() ? std::nullopt : std::optional<QString>{value};
}

std::optional<quint64> positiveUnsigned(
    const QString& argument,
    const QString& prefix)
{
    if (!argument.startsWith(prefix)) return std::nullopt;
    bool ok = false;
    const auto value = argument.mid(prefix.size()).toULongLong(&ok);
    return ok && value > 0 ? std::optional<quint64>{value} : std::nullopt;
}

int exitCode(const wave::ToolchainProbeStatus status)
{
    switch (status) {
    case wave::ToolchainProbeStatus::Ready: return 0;
    case wave::ToolchainProbeStatus::Unavailable: return 3;
    case wave::ToolchainProbeStatus::Incompatible: return 4;
    case wave::ToolchainProbeStatus::Failed: return 5;
    case wave::ToolchainProbeStatus::TimedOut: return 6;
    case wave::ToolchainProbeStatus::Cancelled: return 7;
    }
    return 5;
}

int exitCode(const wave::SimulationRunStatus status)
{
    switch (status) {
    case wave::SimulationRunStatus::Succeeded: return 0;
    case wave::SimulationRunStatus::InvalidRequest:
    case wave::SimulationRunStatus::InvalidManifest:
    case wave::SimulationRunStatus::InvalidStimulus:
    case wave::SimulationRunStatus::ContractMismatch:
    case wave::SimulationRunStatus::UnsupportedFixture:
        return 4;
    case wave::SimulationRunStatus::ToolchainUnavailable: return 3;
    case wave::SimulationRunStatus::TimedOut: return 6;
    case wave::SimulationRunStatus::Cancelled: return 7;
    case wave::SimulationRunStatus::Superseded: return 8;
    case wave::SimulationRunStatus::HarnessGenerationFailed:
    case wave::SimulationRunStatus::BuildFailed:
    case wave::SimulationRunStatus::RunFailed:
    case wave::SimulationRunStatus::TraceImportFailed:
    case wave::SimulationRunStatus::ResultProjectFailed:
        return 5;
    }
    return 5;
}

int runProbe(QCoreApplication& application, const QStringList& arguments)
{
    wave::ToolchainProbeOptions options;
    bool pretty = false;
    std::optional<int> cancelAfterMs;
    for (qsizetype index = 2; index < arguments.size(); ++index) {
        const auto& argument = arguments[index];
        if (argument == QStringLiteral("--pretty")) {
            pretty = true;
            continue;
        }
        if (const auto value = nonEmptyValue(argument, QStringLiteral("--verilator="))) {
            options.verilatorProgram = *value;
            continue;
        }
        if (const auto value = nonEmptyValue(argument, QStringLiteral("--cxx="))) {
            options.cxxProgram = *value;
            continue;
        }
        if (const auto value = positiveInteger(argument, QStringLiteral("--timeout-ms="))) {
            options.timeoutMs = *value;
            continue;
        }
        if (const auto value = positiveInteger(
                argument, QStringLiteral("--cancel-after-ms="))) {
            cancelAfterMs = *value;
            continue;
        }
        QTextStream error(stderr);
        error << "Invalid probe option: " << argument << '\n';
        usage(error);
        return 2;
    }

    wave::ToolchainProbeRunner runner;
    const auto started = runner.start(
        std::move(options),
        [&](wave::ToolchainProbeReport report) {
            QTextStream output(stdout);
            output << QJsonDocument(wave::toolchainProbeReportJson(report))
                          .toJson(pretty ? QJsonDocument::Indented
                                         : QJsonDocument::Compact);
            output.flush();
            application.exit(exitCode(report.status));
        });
    if (!started) {
        QTextStream(stderr) << "The toolchain probe could not be started.\n";
        return 5;
    }
    if (cancelAfterMs) {
        QTimer::singleShot(*cancelAfterMs, &application, [&runner] {
            static_cast<void>(runner.cancel());
        });
    }
    return application.exec();
}

int runSimulation(
    QCoreApplication& application,
    const QStringList& arguments,
    const bool requireResultProject)
{
    wave::SimulationRunRequest request;
    bool pretty = false;
    std::optional<int> cancelAfterMs;
    for (qsizetype index = 2; index < arguments.size(); ++index) {
        const auto& argument = arguments[index];
        if (argument == QStringLiteral("--pretty")) {
            pretty = true;
            continue;
        }
        if (const auto value = nonEmptyValue(argument, QStringLiteral("--manifest="))) {
            request.manifestPath = *value;
            continue;
        }
        if (const auto value = nonEmptyValue(argument, QStringLiteral("--stimulus="))) {
            request.stimulusPath = *value;
            continue;
        }
        if (const auto value = nonEmptyValue(argument, QStringLiteral("--workspace="))) {
            request.workspaceRoot = *value;
            continue;
        }
        if (const auto value = nonEmptyValue(argument, QStringLiteral("--artifacts="))) {
            request.artifactDirectory = *value;
            continue;
        }
        if (const auto value = nonEmptyValue(argument, QStringLiteral("--build-cache="))) {
            request.buildCacheDirectory = *value;
            continue;
        }
        if (const auto value = nonEmptyValue(
                argument, QStringLiteral("--scenario-directory="))) {
            request.scenarioDirectory = *value;
            continue;
        }
        if (const auto value = nonEmptyValue(
                argument, QStringLiteral("--result-project="))) {
            request.resultProjectPath = *value;
            continue;
        }
        if (const auto value = nonEmptyValue(argument, QStringLiteral("--verilator="))) {
            request.toolchain.verilatorProgram = *value;
            continue;
        }
        if (const auto value = nonEmptyValue(argument, QStringLiteral("--cxx="))) {
            request.toolchain.cxxProgram = *value;
            continue;
        }
        if (const auto value = positiveInteger(
                argument, QStringLiteral("--probe-timeout-ms="))) {
            request.toolchain.timeoutMs = *value;
            continue;
        }
        if (const auto value = positiveInteger(
                argument, QStringLiteral("--build-timeout-ms="))) {
            request.buildTimeoutMs = *value;
            continue;
        }
        if (const auto value = positiveInteger(
                argument, QStringLiteral("--run-timeout-ms="))) {
            request.runTimeoutMs = *value;
            continue;
        }
        if (const auto value = positiveInteger(
                argument, QStringLiteral("--cancel-after-ms="))) {
            cancelAfterMs = *value;
            continue;
        }
        if (const auto value = positiveUnsigned(
                argument, QStringLiteral("--generation="))) {
            request.generation = *value;
            continue;
        }
        QTextStream error(stderr);
        error << "Invalid simulation option: " << argument << '\n';
        usage(error);
        return 2;
    }
    if (request.manifestPath.isEmpty() || request.stimulusPath.isEmpty()
        || request.workspaceRoot.isEmpty() || request.artifactDirectory.isEmpty()) {
        QTextStream error(stderr);
        error << "Simulation requires manifest, stimulus, workspace, and artifacts.\n";
        usage(error);
        return 2;
    }
    if (requireResultProject && request.resultProjectPath.isEmpty()) {
        QTextStream error(stderr);
        error << "run-module requires result-project.\n";
        usage(error);
        return 2;
    }

    wave::VerilatorSimulationRunner runner;
    const auto started = runner.start(
        std::move(request),
        [&](wave::SimulationRunReport report) {
            QTextStream output(stdout);
            output << QJsonDocument(wave::simulationRunReportJson(report))
                          .toJson(pretty ? QJsonDocument::Indented
                                         : QJsonDocument::Compact);
            output.flush();
            application.exit(exitCode(report.status));
        });
    if (!started) {
        QTextStream(stderr) << "The simulation could not be started.\n";
        return 5;
    }
    if (cancelAfterMs) {
        QTimer::singleShot(*cancelAfterMs, &application, [&runner] {
            static_cast<void>(runner.cancel());
        });
    }
    return application.exec();
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() == 2
        && (arguments[1] == QStringLiteral("--help")
            || arguments[1] == QStringLiteral("-h"))) {
        QTextStream output(stdout);
        usage(output);
        return 0;
    }
    if (arguments.size() < 2) {
        QTextStream error(stderr);
        usage(error);
        return 2;
    }
    if (arguments[1] == QStringLiteral("probe")) {
        return runProbe(application, arguments);
    }
    if (arguments[1] == QStringLiteral("run-fixture")) {
        return runSimulation(application, arguments, false);
    }
    if (arguments[1] == QStringLiteral("run-module")) {
        return runSimulation(application, arguments, true);
    }
    QTextStream error(stderr);
    usage(error);
    return 2;
}
