// Qt/OptionsDialog.h
//
// Port of UI/FileManager/OptionsDialog.cpp and its six property pages, in the
// original order: System, 7-Zip, Folders, Editor, Settings, Language.
//
// The two pages that are Windows-registry work on the original get the
// equivalent Linux behaviour instead: System writes XDG MIME associations
// (~/.config/mimeapps.list) rather than HKCR keys, and the 7-Zip page
// configures this program's own context menu rather than the shell's, since
// there is no shell to integrate with.

#ifndef ZIP7_INC_QT_OPTIONS_DIALOG_H
#define ZIP7_INC_QT_OPTIONS_DIALOG_H

#include <QDialog>
#include <QSet>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSpinBox;
class QTreeWidget;

// Flags of UI/Explorer/ContextMenuFlags.h, kept so that the "Context menu
// items" list means the same thing it does in 7-Zip.
namespace NContextMenuFlags
{
  const quint32 kExtract      = 1u << 0;
  const quint32 kExtractHere  = 1u << 1;
  const quint32 kExtractTo    = 1u << 2;
  const quint32 kTest         = 1u << 4;
  const quint32 kOpen         = 1u << 5;
  const quint32 kOpenAs       = 1u << 6;
  const quint32 kCompress     = 1u << 8;
  const quint32 kCompressTo7z = 1u << 9;
  const quint32 kCompressToZip = 1u << 12;
  const quint32 kCRC_Cascaded = 1u << 30;
  const quint32 kCRC          = 1u << 31;

  const quint32 kDefault =
      kExtract | kExtractHere | kExtractTo | kTest | kOpen |
      kCompress | kCompressTo7z | kCompressToZip | kCRC_Cascaded;
}

// The settings the file manager reads. They live in QSettings under 7-Zip/7zQ,
// which is where the Windows build would use HKCU\Software\7-Zip.
struct CFmSettings
{
  // ---- Settings page ----
  bool ShowDots;
  bool ShowRealFileIcons;
  bool FullRow;
  bool ShowGrid;
  bool SingleClick;
  bool AlternativeSelection;
  bool MemLimitDefined;
  int  MemLimitGb;          // maximum RAM for unpacking, in GB

  // ---- 7-Zip page ----
  bool CascadedMenu;
  bool MenuIcons;
  bool ElimDup;             // eliminate duplication of root folder
  quint32 ContextMenuFlags;

  // ---- Folders page ----
  int  WorkDirMode;         // 0 = system temp, 1 = current, 2 = specified
  QString WorkDirPath;
  bool WorkDirForRemovableOnly;

  // ---- Editor page ----
  QString Viewer;
  QString Editor;
  QString Diff;

  // ---- Language page ----
  QString Language;         // "" = system default, "en" = English, or a .qm name

  CFmSettings();
  void Load();
  void Save() const;
};

CFmSettings &GlobalFmSettings();

// Translation files the Language page offers: display name -> .qm base name.
// An empty base name means "use the untranslated English strings".
QList<QPair<QString, QString> > AvailableLanguages();


class COptionsDialog: public QDialog
{
  Q_OBJECT
public:
  explicit COptionsDialog(QWidget *parent = nullptr);

protected:
  void accept() override;

private:
  QWidget *CreateSystemPage();
  QWidget *CreateSevenZipPage();
  QWidget *CreateFoldersPage();
  QWidget *CreateEditorPage();
  QWidget *CreateSettingsPage();
  QWidget *CreateLanguagePage();

  void ApplyAssociations();

  // System page
  QTreeWidget *_assocList;

  // 7-Zip page
  QCheckBox *_cascadedMenu;
  QCheckBox *_menuIcons;
  QCheckBox *_elimDup;
  QTreeWidget *_menuItems;

  // Folders page
  QRadioButton *_workSystem;
  QRadioButton *_workCurrent;
  QRadioButton *_workSpecified;
  QLineEdit *_workPath;
  QCheckBox *_workRemovable;

  // Editor page
  QLineEdit *_viewer;
  QLineEdit *_editor;
  QLineEdit *_diff;

  // Settings page
  QCheckBox *_showDots;
  QCheckBox *_showRealFileIcons;
  QCheckBox *_fullRow;
  QCheckBox *_showGrid;
  QCheckBox *_singleClick;
  QCheckBox *_alternativeSelection;
  QCheckBox *_memLimit;
  QSpinBox *_memLimitValue;

  // Language page
  QComboBox *_language;
  QLabel *_languageInfo;
};

#endif
