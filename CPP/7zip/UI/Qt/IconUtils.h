// Qt/IconUtils.h
//
// The port of UI/FileManager/SysIconUtils.cpp: on Windows the panel asks the
// shell for a per-extension icon index; here the icons come from the active
// Qt/XDG icon theme instead, with the same caching behaviour.

#ifndef ZIP7_INC_QT_ICON_UTILS_H
#define ZIP7_INC_QT_ICON_UTILS_H

#include <QIcon>
#include <QString>

QIcon GetFolderIcon();
QIcon GetParentFolderIcon();
QIcon GetFileIconForName(const QString &name);
QIcon GetFileSystemIcon(const QString &fullPath, bool isDir);
QIcon GetArchiveIcon();
QIcon GetAppIcon();

// The toolbar / menu icons, named after 7-Zip's own buttons.
QIcon GetCommandIcon(const QString &command);

// true when (name) has an extension 7-Zip can open as an archive.
bool IsArchiveExtension(const QString &name);

#endif
