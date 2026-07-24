#include "wave/export.h"
#include "wave/project_io.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>

#include <algorithm>
#include <iostream>

namespace {

void printUsage()
{
    std::cerr
        << "Usage: wave-generate <project.wave.json> <output-directory> [scenario-id]\n";
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("wave-generate"));
    if (argc < 3 || argc > 4) {
        printUsage();
        return 1;
    }

    const auto projectPath = QString::fromLocal8Bit(argv[1]);
    const auto outputDirectory = QString::fromLocal8Bit(argv[2]);
    const auto scenarioId = argc == 4 ? QString::fromLocal8Bit(argv[3]) : QString{};
    const auto loadResult = wave::loadProjectFile(projectPath);
    if (!loadResult.ok()) {
        std::cerr << loadResult.error.toStdString() << '\n';
        return 2;
    }
    const auto& project = *loadResult.project;
    const auto scenario = std::find_if(
        project.scenarios.begin(),
        project.scenarios.end(),
        [&scenarioId](const wave::Scenario& candidate) {
            return scenarioId.isEmpty()
                || QString::fromStdString(candidate.id) == scenarioId;
        });
    if (scenario == project.scenarios.end()) {
        std::cerr << "Scenario not found: " << scenarioId.toStdString() << '\n';
        return 2;
    }

    wave::ExportOptions options;
    options.pngDpi = 192;
    const auto result = wave::generateArtifactBundle(project, *scenario, options);
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

    auto baseName = QString::fromStdString(wave::sanitizeIdentifier(scenario->name));
    if (baseName.isEmpty()) {
        baseName = QString::fromStdString(wave::sanitizeIdentifier(scenario->id));
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
    std::cout << "Generated 7 artifacts in "
              << QDir(outputDirectory).absolutePath().toStdString() << '\n';
    return 0;
}
