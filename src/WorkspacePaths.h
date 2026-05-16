#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>

namespace WorkspacePaths {

inline QString findWorkspaceRoot(const QString& packageRoot) {
    QDir dir(QFileInfo(packageRoot).absoluteFilePath());
    for (int depth = 0; depth < 6; ++depth) {
        if (dir.exists("dist_qt") || dir.exists("dist_android") || dir.exists("dist_media"))
            return dir.absolutePath();
        if (!dir.cdUp())
            break;
    }

    return QFileInfo(QDir(packageRoot).filePath("../..")).absoluteFilePath();
}

inline QString toPortablePath(const QString& path) {
    return QDir::fromNativeSeparators(path);
}

inline QString relativizeToWorkspace(const QString& workspaceRoot, const QString& absolutePath) {
    const QFileInfo info(absolutePath);
    if (!info.isAbsolute())
        return toPortablePath(absolutePath);

    const QString rootAbs = QDir(workspaceRoot).absolutePath();
    const QString fileAbs = info.absoluteFilePath();
    const QString rel = QDir(rootAbs).relativeFilePath(fileAbs);

    if (rel.startsWith(".."))
        return toPortablePath(fileAbs);

    return toPortablePath(rel);
}

inline QString absoluteFromWorkspace(const QString& workspaceRoot, const QString& storedPath) {
    if (QFileInfo(storedPath).isAbsolute())
        return storedPath;
    return QDir(workspaceRoot).filePath(storedPath);
}

inline QString preferredWorkspaceChildDir(const QString& workspaceRoot, const QString& childName) {
    const QString path = QDir(workspaceRoot).filePath(childName);
    return QFileInfo::exists(path) ? path : workspaceRoot;
}

}  // namespace WorkspacePaths
