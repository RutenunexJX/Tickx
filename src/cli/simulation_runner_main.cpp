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
        << "Wave Workbench simulation runner (experimental)\n"
           "Usage:\n"
           "  wave-sim-runner probe [--verilator=PATH] [--cxx=PATH] "
           "[--timeout-ms=N] [--cancel-after-ms=N] [--pretty]\n\n"
           "The probe is asynchronous and does not compile or elaborate a DUT.\n"
           "Exit codes: 0 ready, 2 usage, 3 unavailable, 4 incompatible, "
           "5 failed, 6 timed out, 7 cancelled.\n";
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
    if (arguments.size() < 2 || arguments[1] != QStringLiteral("probe")) {
        QTextStream error(stderr);
        usage(error);
        return 2;
    }

    wave::ToolchainProbeOptions options;
    bool pretty = false;
    std::optional<int> cancelAfterMs;
    for (qsizetype index = 2; index < arguments.size(); ++index) {
        const auto& argument = arguments[index];
        if (argument == QStringLiteral("--pretty")) {
            pretty = true;
            continue;
        }
        if (argument.startsWith(QStringLiteral("--verilator="))) {
            options.verilatorProgram = argument.mid(
                QStringLiteral("--verilator=").size());
            if (!options.verilatorProgram.isEmpty()) continue;
        }
        if (argument.startsWith(QStringLiteral("--cxx="))) {
            options.cxxProgram = argument.mid(QStringLiteral("--cxx=").size());
            if (!options.cxxProgram.isEmpty()) continue;
        }
        if (argument.startsWith(QStringLiteral("--timeout-ms="))) {
            const auto parsed = positiveInteger(
                argument, QStringLiteral("--timeout-ms="));
            if (parsed) {
                options.timeoutMs = *parsed;
                continue;
            }
        }
        if (argument.startsWith(QStringLiteral("--cancel-after-ms="))) {
            const auto parsed = positiveInteger(
                argument, QStringLiteral("--cancel-after-ms="));
            if (parsed) {
                cancelAfterMs = *parsed;
                continue;
            }
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
                          .toJson(
                              pretty ? QJsonDocument::Indented
                                     : QJsonDocument::Compact);
            output.flush();
            application.exit(exitCode(report.status));
        });
    if (!started) {
        QTextStream error(stderr);
        error << "The toolchain probe could not be started.\n";
        return 5;
    }
    if (cancelAfterMs) {
        QTimer::singleShot(*cancelAfterMs, &application, [&runner] {
            static_cast<void>(runner.cancel());
        });
    }
    return application.exec();
}
