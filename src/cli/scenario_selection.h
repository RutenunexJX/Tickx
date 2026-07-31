#pragma once

#include "wave/model.h"

#include <QString>

#include <algorithm>
#include <optional>

namespace wave::cli {

inline std::optional<std::size_t> resolveScenarioIndex(
    const Project& project,
    const std::optional<QString>& selector,
    QString& error)
{
    if (project.scenarios.empty()) {
        error = QStringLiteral(
            "Project contains no scenario.");
        return std::nullopt;
    }

    std::optional<std::size_t> selectedIndex;
    if (!selector) {
        if (project.scenarios.size() != 1) {
            error = QStringLiteral(
                "Project contains %1 scenarios; specify "
                "--scenario=<stable-id-or-unique-name>.")
                        .arg(
                            static_cast<qulonglong>(
                                project.scenarios.size()));
            return std::nullopt;
        }
        selectedIndex = 0;
    } else {
        const auto normalizedSelector =
            selector->trimmed();
        if (normalizedSelector.isEmpty()) {
            error = QStringLiteral(
                "--scenario requires a non-empty stable ID or unique name.");
            return std::nullopt;
        }

        std::size_t idMatchCount = 0;
        for (std::size_t index = 0;
             index < project.scenarios.size();
             ++index) {
            if (QString::fromStdString(
                    project.scenarios.at(index).id)
                != normalizedSelector) {
                continue;
            }
            selectedIndex = index;
            ++idMatchCount;
        }
        if (idMatchCount > 1) {
            error = QStringLiteral(
                        "Scenario stable ID '%1' is ambiguous; repair duplicate "
                        "Scenario IDs before continuing.")
                        .arg(normalizedSelector);
            return std::nullopt;
        }

        if (!selectedIndex) {
            std::size_t nameMatchCount = 0;
            for (std::size_t index = 0;
                 index < project.scenarios.size();
                 ++index) {
                if (QString::compare(
                        QString::fromStdString(
                            project.scenarios.at(index).name),
                        normalizedSelector,
                        Qt::CaseInsensitive)
                    != 0) {
                    continue;
                }
                selectedIndex = index;
                ++nameMatchCount;
            }
            if (nameMatchCount > 1) {
                error = QStringLiteral(
                    "Scenario name '%1' is ambiguous; use a stable ID.")
                            .arg(normalizedSelector);
                return std::nullopt;
            }
        }
        if (!selectedIndex) {
            error = QStringLiteral(
                "Scenario '%1' does not exist by stable ID or unique name.")
                        .arg(normalizedSelector);
            return std::nullopt;
        }
    }

    const auto& selected =
        project.scenarios.at(*selectedIndex);
    if (selected.id.empty()) {
        error = QStringLiteral(
            "Selected Scenario has an empty stable ID; repair it before continuing.");
        return std::nullopt;
    }
    const auto idMatchCount =
        static_cast<std::size_t>(
            std::count_if(
                project.scenarios.begin(),
                project.scenarios.end(),
                [&selected](const Scenario& candidate) {
                    return candidate.id == selected.id;
                }));
    if (idMatchCount != 1) {
        error = QStringLiteral(
            "Selected Scenario stable ID '%1' is ambiguous; repair duplicate "
            "Scenario IDs before continuing.")
                    .arg(
                        QString::fromStdString(
                            selected.id));
        return std::nullopt;
    }
    return selectedIndex;
}

} // namespace wave::cli
