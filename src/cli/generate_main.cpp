#include "wave/export.h"
#include "wave/project_io.h"

#include "scenario_selection.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>

#include <iostream>
#include <optional>

namespace {

void printUsage()
{
    std::cerr
        << "Usage: wave-generate <project.wave.json> <output-directory> "
           "[--scenario=SELECTOR]\n"
           "       wave-generate <project.wave.json> <output-directory> "
           "[legacy-scenario-id]\n";
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("wave-generate"));
    const auto arguments = application.arguments();
    if (arguments.size() < 3) {
        printUsage();
        return 1;
    }

    std::optional<QString> scenarioSelector;
    std::optional<QString> legacyScenarioId;
    for (int index = 3; index < arguments.size(); ++index) {
        const auto& argument = arguments[index];
        if (argument.startsWith(QStringLiteral("--scenario="))) {
            if (scenarioSelector) {
                std::cerr << "--scenario may be specified only once.\n";
                return 2;
            }
            scenarioSelector =
                argument.mid(QStringLiteral("--scenario=").size());
        } else if (argument.startsWith(QStringLiteral("--"))) {
            std::cerr << "Unknown option: "
                      << argument.toStdString() << '\n';
            return 2;
        } else if (legacyScenarioId) {
            std::cerr << "Only one legacy scenario ID may be specified.\n";
            return 2;
        } else {
            legacyScenarioId = argument;
        }
    }
    if (scenarioSelector && legacyScenarioId) {
        std::cerr
            << "Use either --scenario or the legacy positional scenario ID, not both.\n";
        return 2;
    }
    if (!scenarioSelector && legacyScenarioId) {
        scenarioSelector = legacyScenarioId;
    }

    const auto projectPath = QFileInfo(arguments[1]).absoluteFilePath();
    const auto outputDirectory = QFileInfo(arguments[2]).absoluteFilePath();
    const auto loadResult = wave::loadProjectFile(projectPath);
    if (!loadResult.ok()) {
        std::cerr << loadResult.error.toStdString() << '\n';
        return 2;
    }
    const auto& project = *loadResult.project;
    QString scenarioError;
    const auto scenarioIndex =
        wave::cli::resolveScenarioIndex(
            project,
            scenarioSelector,
            scenarioError);
    if (!scenarioIndex) {
        std::cerr << scenarioError.toStdString() << '\n';
        return 2;
    }
    const auto& scenario =
        project.scenarios.at(*scenarioIndex);

    wave::ExportOptions options;
    options.pngDpi = 192;
    const auto result =
        wave::generateArtifactBundle(project, scenario, options);
    for (const auto& diagnostic : result.diagnostics) {
        const auto code = wave::toString(diagnostic.code);
        std::cerr << (diagnostic.severity == wave::Severity::Error ? "error" : "warning")
                  << " [" << code << "] " << diagnostic.message << '\n';
    }
    if (!result.ok()) {
        if (!result.error.isEmpty()) {
            std::cerr << result.error.toStdString() << '\n';
        }
        return 3;
    }

    auto baseName =
        QString::fromStdString(
            wave::sanitizeIdentifier(scenario.name));
    if (baseName.isEmpty()) {
        baseName =
            QString::fromStdString(
                wave::sanitizeIdentifier(scenario.id));
    }
    QString writeError;
    if (!wave::writeArtifactBundleAtomic(
            *result.artifacts,
            outputDirectory,
            baseName,
            &writeError)) {
        std::cerr << writeError.toStdString() << '\n';
        return 4;
    }
    std::cout << "Generated 7 artifacts for "
              << scenario.name << " (" << scenario.id << ") in "
              << QDir(outputDirectory).absolutePath().toStdString() << '\n';
    return 0;
}
