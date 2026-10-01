#pragma once

#include <QCoreApplication>
#include <QGuiApplication>

namespace wave {

inline void configureStandaloneIdentity(const QString& version)
{
    // Keep QSettings and QStandardPaths on the established storage identity.
    // Embedded wavewidgets must leave the host application's identity alone.
    QCoreApplication::setOrganizationName(QStringLiteral("WaveWorkbench"));
    QCoreApplication::setApplicationName(QStringLiteral("Wave Workbench"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Tickx"));
    QCoreApplication::setApplicationVersion(version);
}

} // namespace wave
