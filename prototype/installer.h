#pragma once

#include <QString>

QString installArchive(int product, const QString &archivePath, bool update = false,
                       const QString &version = {});
QString rollbackInstallation(int product);
QString removeInstallation(int product);
bool hasInstallation(int product);
QString launchInstallation(int product);
QString managedInstallationFolder(int product);
bool hasPreviousInstallation(int product);
QString installedRelease(int product);
bool hasRtl(int product);
bool wantsRtl(int product);
bool supportsRtl(int product);
QString setInstallationRtl(int product, bool enable);
