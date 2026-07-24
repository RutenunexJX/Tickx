#pragma once

#include "wave/model.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

namespace wave {

struct ProjectLoadResult {
    std::optional<Project> project;
    QString error;
    QStringList warnings;
    bool migrated{false};

    [[nodiscard]] bool ok() const noexcept { return project.has_value(); }
};

[[nodiscard]] QByteArray serializeProject(const Project& project);
[[nodiscard]] ProjectLoadResult deserializeProject(const QByteArray& data);

[[nodiscard]] ProjectLoadResult loadProjectFile(const QString& filePath);
[[nodiscard]] bool saveProjectFileAtomic(
    const Project& project,
    const QString& filePath,
    QString* error = nullptr);

[[nodiscard]] ProjectLoadResult loadProjectDirectory(const QString& directoryPath);
[[nodiscard]] bool saveProjectDirectory(
    const Project& project,
    const QString& directoryPath,
    QString* error = nullptr);

[[nodiscard]] QString projectFilePath(const QString& directoryPath);

} // namespace wave
