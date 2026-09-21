// Qt/main.cpp
//
// Entry point of the Qt port of the 7-Zip file manager.
// MyInitGuid.h must be included exactly once in the program, so this is where
// the 7-Zip interface GUIDs are instantiated.

#include "StdAfx.h"

#include "../../../Common/MyInitGuid.h"

// Every 7-Zip interface GUID must be instantiated exactly once in the
// program. Including the interface headers here, right after MyInitGuid.h
// (which defines INITGUID), is what does that -- the same role FM.cpp and
// Console/Main.cpp play in the original build.
#include "../../IPassword.h"
#include "../../IProgress.h"
#include "../../IStream.h"
#include "../../ICoder.h"
#include "../../Archive/IArchive.h"
#include "../FileManager/IFolder.h"
#include "../Agent/IFolderArchive.h"
#include "../Common/IFileExtractCallback.h"
#include "../Agent/Agent.h"

#include "../../MyVersion.h"

#include "Codecs.h"
#include "IconUtils.h"
#include "MainWindow.h"
#include "OptionsDialog.h"
#include "Z7Qt.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTranslator>

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("7-Zip"));
  QCoreApplication::setApplicationName(QStringLiteral("7zQ"));
  QCoreApplication::setApplicationVersion(QLatin1String(MY_VERSION));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QCoreApplication::translate("main", "7-Zip File Manager (Qt)"));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument(
      QCoreApplication::translate("main", "path"),
      QCoreApplication::translate("main", "Folder or archive to open."));
  parser.process(app);

  // Loading the codecs up front means the format list is ready for the panel
  // and for the Add dialog, and it surfaces a broken build immediately.
  if (EnsureCodecsLoaded() != S_OK)
  {
    QMessageBox::critical(nullptr, QStringLiteral("7-Zip"),
        QCoreApplication::translate("main", "Cannot load the 7-Zip codecs."));
    return 1;
  }

  GlobalFmSettings().Load();

  // Options > Language. An empty setting means the system locale; "" with no
  // matching .qm simply leaves the built-in English strings in place.
  QTranslator translator;
  {
    const QString code = GlobalFmSettings().Language;
    if (!code.isEmpty())
    {
      QStringList dirs;
      dirs << QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
      const QStringList data =
          QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
      for (const QString &d : data)
        dirs << d + QStringLiteral("/translations");
      for (const QString &d : dirs)
        if (translator.load(QStringLiteral("7zQ_") + code, d))
        {
          QCoreApplication::installTranslator(&translator);
          break;
        }
    }
  }

  CMainWindow window;
  const QStringList args = parser.positionalArguments();
  if (!args.isEmpty())
    window.OpenPath(QDir(args.first()).absolutePath());
  window.show();

  return app.exec();
}
