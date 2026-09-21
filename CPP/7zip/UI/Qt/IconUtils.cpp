// Qt/IconUtils.cpp

#include "StdAfx.h"

#include "IconUtils.h"
#include "Z7Qt.h"
#include "Codecs.h"

#include <QFileIconProvider>
#include <QFileInfo>
#include <QHash>
#include <QMimeDatabase>
#include <QPainter>
#include <QSet>
#include <QStyle>
#include <QApplication>

static QFileIconProvider &Provider()
{
  static QFileIconProvider p;
  return p;
}

QIcon GetFolderIcon()
{
  static QIcon icon;
  if (icon.isNull())
  {
    icon = QIcon::fromTheme(QStringLiteral("folder"));
    if (icon.isNull())
      icon = Provider().icon(QFileIconProvider::Folder);
  }
  return icon;
}

QIcon GetParentFolderIcon()
{
  static QIcon icon;
  if (icon.isNull())
  {
    icon = QIcon::fromTheme(QStringLiteral("go-up"));
    if (icon.isNull())
      icon = qApp->style()->standardIcon(QStyle::SP_FileDialogToParent);
  }
  return icon;
}

QIcon GetArchiveIcon()
{
  static QIcon icon;
  if (icon.isNull())
  {
    icon = QIcon::fromTheme(QStringLiteral("package-x-generic"));
    if (icon.isNull())
      icon = QIcon::fromTheme(QStringLiteral("application-x-archive"));
    if (icon.isNull())
      icon = Provider().icon(QFileIconProvider::File);
  }
  return icon;
}

QIcon GetAppIcon()
{
  static QIcon icon;
  if (icon.isNull())
  {
    icon = QIcon::fromTheme(QStringLiteral("7zip"));
    if (icon.isNull())
      icon = GetArchiveIcon();
  }
  return icon;
}

bool IsArchiveExtension(const QString &name)
{
  const int dot = name.lastIndexOf(QLatin1Char('.'));
  if (dot < 0)
    return false;
  return ArchiveExtensions().contains(name.mid(dot + 1).toLower());
}

QIcon GetFileIconForName(const QString &name)
{
  static QHash<QString, QIcon> cache;
  const int dot = name.lastIndexOf(QLatin1Char('.'));
  const QString ext = dot < 0 ? QString() : name.mid(dot + 1).toLower();

  const auto it = cache.constFind(ext);
  if (it != cache.constEnd())
    return it.value();

  QIcon icon;
  if (!ext.isEmpty() && ArchiveExtensions().contains(ext))
    icon = GetArchiveIcon();
  if (icon.isNull())
  {
    QMimeDatabase db;
    const QMimeType mt = db.mimeTypeForFile(name, QMimeDatabase::MatchExtension);
    if (mt.isValid())
    {
      icon = QIcon::fromTheme(mt.iconName());
      if (icon.isNull())
        icon = QIcon::fromTheme(mt.genericIconName());
    }
  }
  if (icon.isNull())
    icon = Provider().icon(QFileIconProvider::File);
  cache.insert(ext, icon);
  return icon;
}

QIcon GetFileSystemIcon(const QString &fullPath, bool isDir)
{
  if (isDir)
    return GetFolderIcon();
  return GetFileIconForName(QFileInfo(fullPath).fileName());
}

QIcon GetCommandIcon(const QString &command)
{
  static QHash<QString, QIcon> cache;
  const auto it = cache.constFind(command);
  if (it != cache.constEnd())
    return it.value();

  // 7-Zip's toolbar: Add, Extract, Test, Copy, Move, Delete, Info.
  static const struct { const char *cmd; const char *theme; QStyle::StandardPixmap fallback; } kMap[] =
  {
    { "Add",     "list-add",             QStyle::SP_FileDialogNewFolder },
    { "Extract", "archive-extract",      QStyle::SP_DialogSaveButton },
    { "Test",    "dialog-ok-apply",      QStyle::SP_DialogApplyButton },
    { "Copy",    "edit-copy",            QStyle::SP_DialogSaveButton },
    { "Move",    "edit-cut",             QStyle::SP_ArrowRight },
    { "Delete",  "edit-delete",          QStyle::SP_TrashIcon },
    { "Info",    "dialog-information",   QStyle::SP_MessageBoxInformation },
    { "Up",      "go-up",                QStyle::SP_FileDialogToParent },
    { "Back",    "go-previous",          QStyle::SP_ArrowBack },
    { "Forward", "go-next",              QStyle::SP_ArrowForward },
    { "Refresh", "view-refresh",         QStyle::SP_BrowserReload }
  };

  QIcon icon;
  for (unsigned i = 0; i < sizeof(kMap) / sizeof(kMap[0]); i++)
    if (command == QLatin1String(kMap[i].cmd))
    {
      icon = QIcon::fromTheme(QLatin1String(kMap[i].theme));
      if (icon.isNull())
        icon = qApp->style()->standardIcon(kMap[i].fallback);
      break;
    }
  cache.insert(command, icon);
  return icon;
}
