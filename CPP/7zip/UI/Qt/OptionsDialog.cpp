// Qt/OptionsDialog.cpp

#include "StdAfx.h"

#include "../Common/LoadCodecs.h"

#include "Codecs.h"
#include "IconUtils.h"
#include "OptionsDialog.h"
#include "Z7Qt.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>

static const char * const kSettingsOrg = "7-Zip";
static const char * const kSettingsApp = "7zQ";
static const char * const kDesktopFile = "7zQ.desktop";


// ---------------------------------------------------------------------------
// CFmSettings
// ---------------------------------------------------------------------------

CFmSettings::CFmSettings():
    ShowDots(true),
    ShowRealFileIcons(true),
    FullRow(true),
    ShowGrid(false),
    SingleClick(false),
    AlternativeSelection(false),
    MemLimitDefined(false),
    MemLimitGb(4),
    CascadedMenu(true),
    MenuIcons(true),
    ElimDup(true),
    ContextMenuFlags(NContextMenuFlags::kDefault),
    WorkDirMode(0),
    WorkDirForRemovableOnly(false)
{}

void CFmSettings::Load()
{
  QSettings s{QLatin1String(kSettingsOrg), QLatin1String(kSettingsApp)};
  s.beginGroup(QStringLiteral("FM"));
  ShowDots             = s.value(QStringLiteral("ShowDots"), ShowDots).toBool();
  ShowRealFileIcons    = s.value(QStringLiteral("ShowRealFileIcons"), ShowRealFileIcons).toBool();
  FullRow              = s.value(QStringLiteral("FullRow"), FullRow).toBool();
  ShowGrid             = s.value(QStringLiteral("ShowGrid"), ShowGrid).toBool();
  SingleClick          = s.value(QStringLiteral("SingleClick"), SingleClick).toBool();
  AlternativeSelection = s.value(QStringLiteral("AlternativeSelection"), AlternativeSelection).toBool();
  MemLimitDefined      = s.value(QStringLiteral("MemLimitDefined"), MemLimitDefined).toBool();
  MemLimitGb           = s.value(QStringLiteral("MemLimitGb"), MemLimitGb).toInt();

  CascadedMenu         = s.value(QStringLiteral("CascadedMenu"), CascadedMenu).toBool();
  MenuIcons            = s.value(QStringLiteral("MenuIcons"), MenuIcons).toBool();
  ElimDup              = s.value(QStringLiteral("ElimDup"), ElimDup).toBool();
  ContextMenuFlags     = s.value(QStringLiteral("ContextMenuFlags"),
                                 (uint)ContextMenuFlags).toUInt();

  WorkDirMode          = s.value(QStringLiteral("WorkDirMode"), WorkDirMode).toInt();
  WorkDirPath          = s.value(QStringLiteral("WorkDirPath")).toString();
  WorkDirForRemovableOnly = s.value(QStringLiteral("WorkDirForRemovableOnly"),
                                    WorkDirForRemovableOnly).toBool();

  Viewer               = s.value(QStringLiteral("Viewer")).toString();
  Editor               = s.value(QStringLiteral("Editor")).toString();
  Diff                 = s.value(QStringLiteral("Diff")).toString();

  Language             = s.value(QStringLiteral("Language")).toString();
  s.endGroup();
}

void CFmSettings::Save() const
{
  QSettings s{QLatin1String(kSettingsOrg), QLatin1String(kSettingsApp)};
  s.beginGroup(QStringLiteral("FM"));
  s.setValue(QStringLiteral("ShowDots"), ShowDots);
  s.setValue(QStringLiteral("ShowRealFileIcons"), ShowRealFileIcons);
  s.setValue(QStringLiteral("FullRow"), FullRow);
  s.setValue(QStringLiteral("ShowGrid"), ShowGrid);
  s.setValue(QStringLiteral("SingleClick"), SingleClick);
  s.setValue(QStringLiteral("AlternativeSelection"), AlternativeSelection);
  s.setValue(QStringLiteral("MemLimitDefined"), MemLimitDefined);
  s.setValue(QStringLiteral("MemLimitGb"), MemLimitGb);

  s.setValue(QStringLiteral("CascadedMenu"), CascadedMenu);
  s.setValue(QStringLiteral("MenuIcons"), MenuIcons);
  s.setValue(QStringLiteral("ElimDup"), ElimDup);
  s.setValue(QStringLiteral("ContextMenuFlags"), (uint)ContextMenuFlags);

  s.setValue(QStringLiteral("WorkDirMode"), WorkDirMode);
  s.setValue(QStringLiteral("WorkDirPath"), WorkDirPath);
  s.setValue(QStringLiteral("WorkDirForRemovableOnly"), WorkDirForRemovableOnly);

  s.setValue(QStringLiteral("Viewer"), Viewer);
  s.setValue(QStringLiteral("Editor"), Editor);
  s.setValue(QStringLiteral("Diff"), Diff);

  s.setValue(QStringLiteral("Language"), Language);
  s.endGroup();
}

CFmSettings &GlobalFmSettings()
{
  static CFmSettings settings;
  static bool loaded = false;
  if (!loaded)
  {
    loaded = true;
    settings.Load();
  }
  return settings;
}


// ---------------------------------------------------------------------------
// languages
// ---------------------------------------------------------------------------

static QStringList TranslationDirs()
{
  QStringList dirs;
  dirs << QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
  const QStringList data = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
  for (const QString &d : data)
    dirs << d + QStringLiteral("/translations");
  return dirs;
}

QList<QPair<QString, QString> > AvailableLanguages()
{
  QList<QPair<QString, QString> > res;
  res.append(qMakePair(QObject::tr("English"), QString()));
  QSet<QString> seen;
  for (const QString &dirPath : TranslationDirs())
  {
    QDir dir(dirPath);
    if (!dir.exists())
      continue;
    const QStringList files = dir.entryList(QStringList() << QStringLiteral("7zQ_*.qm"),
        QDir::Files, QDir::Name);
    for (const QString &file : files)
    {
      const QString code = QFileInfo(file).completeBaseName().mid(4);
      if (seen.contains(code))
        continue;
      seen.insert(code);
      const QLocale locale(code);
      res.append(qMakePair(locale.nativeLanguageName() + QStringLiteral(" (") + code
          + QStringLiteral(")"), code));
    }
  }
  return res;
}


// ---------------------------------------------------------------------------
// XDG file associations (the Linux counterpart of the System page)
// ---------------------------------------------------------------------------

static QString MimeAppsPath()
{
  return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
      + QStringLiteral("/mimeapps.list");
}

// MIME types that are currently pointed at our .desktop file.
static QSet<QString> ReadAssociatedMimeTypes()
{
  QSet<QString> res;
  QFile f(MimeAppsPath());
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return res;
  QTextStream in(&f);
  bool inDefaults = false;
  while (!in.atEnd())
  {
    const QString line = in.readLine().trimmed();
    if (line.startsWith(QLatin1Char('[')))
    {
      inDefaults = (line == QLatin1String("[Default Applications]"));
      continue;
    }
    if (!inDefaults)
      continue;
    const int eq = line.indexOf(QLatin1Char('='));
    if (eq < 0)
      continue;
    if (line.mid(eq + 1).split(QLatin1Char(';')).contains(QLatin1String(kDesktopFile)))
      res.insert(line.left(eq).trimmed());
  }
  return res;
}

// Rewrites [Default Applications] so that exactly (wanted) point at us; other
// applications' entries are left untouched.
static bool WriteAssociatedMimeTypes(const QSet<QString> &wanted)
{
  const QString path = MimeAppsPath();
  QStringList lines;
  {
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
      QTextStream in(&f);
      while (!in.atEnd())
        lines.append(in.readLine());
    }
  }

  // drop every existing default that names us, and remember where the
  // [Default Applications] group is
  int groupStart = -1;
  for (int i = 0; i < lines.size(); i++)
  {
    const QString t = lines[i].trimmed();
    if (t == QLatin1String("[Default Applications]"))
    {
      groupStart = i;
      continue;
    }
    if (groupStart >= 0 && t.startsWith(QLatin1Char('[')))
      break;
  }

  QStringList kept;
  bool inDefaults = false;
  for (const QString &line : lines)
  {
    const QString t = line.trimmed();
    if (t.startsWith(QLatin1Char('[')))
      inDefaults = (t == QLatin1String("[Default Applications]"));
    if (inDefaults && !t.startsWith(QLatin1Char('[')))
    {
      const int eq = t.indexOf(QLatin1Char('='));
      if (eq >= 0)
      {
        QStringList apps = t.mid(eq + 1).split(QLatin1Char(';'), Qt::SkipEmptyParts);
        apps.removeAll(QLatin1String(kDesktopFile));
        const QString mime = t.left(eq).trimmed();
        if (apps.isEmpty() && !wanted.contains(mime))
          continue;                       // nothing left for this type
        if (wanted.contains(mime))
          apps.prepend(QLatin1String(kDesktopFile));
        kept.append(mime + QLatin1Char('=') + apps.join(QLatin1Char(';')));
        continue;
      }
    }
    kept.append(line);
  }

  // add the types that were not in the file yet
  QSet<QString> present;
  inDefaults = false;
  for (const QString &line : kept)
  {
    const QString t = line.trimmed();
    if (t.startsWith(QLatin1Char('[')))
      inDefaults = (t == QLatin1String("[Default Applications]"));
    else if (inDefaults)
    {
      const int eq = t.indexOf(QLatin1Char('='));
      if (eq >= 0)
        present.insert(t.left(eq).trimmed());
    }
  }

  QStringList missing;
  for (const QString &mime : wanted)
    if (!present.contains(mime))
      missing.append(mime + QLatin1Char('=') + QLatin1String(kDesktopFile));
  missing.sort();

  if (!missing.isEmpty())
  {
    int insertAt = -1;
    for (int i = 0; i < kept.size(); i++)
      if (kept[i].trimmed() == QLatin1String("[Default Applications]"))
      {
        insertAt = i + 1;
        while (insertAt < kept.size() && !kept[insertAt].trimmed().startsWith(QLatin1Char('[')))
          insertAt++;
        break;
      }
    if (insertAt < 0)
    {
      if (!kept.isEmpty() && !kept.last().trimmed().isEmpty())
        kept.append(QString());
      kept.append(QStringLiteral("[Default Applications]"));
      insertAt = kept.size();
    }
    for (int i = 0; i < missing.size(); i++)
      kept.insert(insertAt + i, missing[i]);
  }

  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile out(path);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    return false;
  QTextStream ts(&out);
  for (const QString &line : kept)
    ts << line << Qt::endl;
  return true;
}


// ---------------------------------------------------------------------------
// COptionsDialog
// ---------------------------------------------------------------------------

static QWidget *MakeBrowseRow(QLineEdit **edit, QWidget *parent, bool directory,
    QLabel *labelForBuddy = nullptr)
{
  QWidget *w = new QWidget(parent);
  QHBoxLayout *l = new QHBoxLayout(w);
  l->setContentsMargins(0, 0, 0, 0);
  *edit = new QLineEdit;
  if (labelForBuddy)
    labelForBuddy->setBuddy(*edit);
  QPushButton *b = new QPushButton(QStringLiteral("..."));
  b->setFixedWidth(32);
  l->addWidget(*edit, 1);
  l->addWidget(b);

  QLineEdit *e = *edit;
  QObject::connect(b, &QPushButton::clicked, w, [e, w, directory]
  {
    const QString path = directory
        ? QFileDialog::getExistingDirectory(w, QObject::tr("Select folder"), e->text())
        : QFileDialog::getOpenFileName(w, QObject::tr("Select program"), e->text());
    if (!path.isEmpty())
      e->setText(path);
  });
  return w;
}


QWidget *COptionsDialog::CreateSystemPage()
{
  QWidget *page = new QWidget;
  QVBoxLayout *l = new QVBoxLayout(page);

  QLabel *label = new QLabel(tr("Associate 7-Zip with:"));
  l->addWidget(label);

  _assocList = new QTreeWidget;
  _assocList->setColumnCount(2);
  _assocList->setHeaderLabels(QStringList() << tr("Type") << tr("Extensions"));
  _assocList->setRootIsDecorated(false);
  _assocList->setAlternatingRowColors(true);
  label->setBuddy(_assocList);

  const QSet<QString> associated = ReadAssociatedMimeTypes();
  QMimeDatabase db;
  QSet<QString> seenMime;

  if (CCodecs *codecs = GetCodecs())
  {
    FOR_VECTOR (i, codecs->Formats)
    {
      const CArcInfoEx &ai = codecs->Formats[i];
      if (ai.Exts.IsEmpty())
        continue;
      QStringList exts;
      QString mime;
      FOR_VECTOR (k, ai.Exts)
      {
        const QString ext = Us2Q(ai.Exts[k].Ext);
        exts << ext;
        if (mime.isEmpty())
        {
          const QMimeType mt = db.mimeTypeForFile(QStringLiteral("x.") + ext,
              QMimeDatabase::MatchExtension);
          if (mt.isValid() && !mt.isDefault())
            mime = mt.name();
        }
      }
      if (mime.isEmpty() || seenMime.contains(mime))
        continue;
      seenMime.insert(mime);

      QTreeWidgetItem *item = new QTreeWidgetItem(_assocList);
      item->setText(0, Us2Q(ai.Name));
      item->setText(1, exts.join(QLatin1Char(' ')));
      item->setData(0, Qt::UserRole, mime);
      item->setToolTip(0, mime);
      item->setIcon(0, GetFileIconForName(QStringLiteral("x.") + exts.first()));
      item->setCheckState(0, associated.contains(mime)
          ? Qt::Checked : Qt::Unchecked);
    }
  }
  _assocList->resizeColumnToContents(0);
  l->addWidget(_assocList, 1);

  QPushButton *selectAll = new QPushButton(tr("Select &all"));
  QPushButton *clearAll = new QPushButton(tr("&Clear all"));
  QHBoxLayout *buttons = new QHBoxLayout;
  buttons->addWidget(selectAll);
  buttons->addWidget(clearAll);
  buttons->addStretch(1);
  l->addLayout(buttons);

  connect(selectAll, &QPushButton::clicked, this, [this]
  {
    for (int i = 0; i < _assocList->topLevelItemCount(); i++)
      _assocList->topLevelItem(i)->setCheckState(0, Qt::Checked);
  });
  connect(clearAll, &QPushButton::clicked, this, [this]
  {
    for (int i = 0; i < _assocList->topLevelItemCount(); i++)
      _assocList->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
  });

  l->addWidget(new QLabel(tr(
      "Associations are written to the XDG default-application list\n"
      "(%1), which is this system's equivalent of the file type\n"
      "registry the Windows version writes.").arg(MimeAppsPath())));
  return page;
}


QWidget *COptionsDialog::CreateSevenZipPage()
{
  const CFmSettings &s = GlobalFmSettings();

  QWidget *page = new QWidget;
  QVBoxLayout *l = new QVBoxLayout(page);

  _cascadedMenu = new QCheckBox(tr("Cascaded context menu"));
  _menuIcons = new QCheckBox(tr("Icons in context menu"));
  _elimDup = new QCheckBox(tr("Eliminate duplication of root folder"));
  _cascadedMenu->setChecked(s.CascadedMenu);
  _menuIcons->setChecked(s.MenuIcons);
  _elimDup->setChecked(s.ElimDup);

  l->addWidget(_cascadedMenu);
  l->addWidget(_menuIcons);
  l->addWidget(_elimDup);
  l->addSpacing(8);

  QLabel *label = new QLabel(tr("Context menu items:"));
  l->addWidget(label);

  _menuItems = new QTreeWidget;
  _menuItems->setColumnCount(1);
  _menuItems->setHeaderHidden(true);
  _menuItems->setRootIsDecorated(false);
  label->setBuddy(_menuItems);

  // Same list and order as kMenuItems in UI/FileManager/MenuPage.cpp.
  static const struct { const char *Text; quint32 Flag; } kMenuItems[] =
  {
    { "Open",                  NContextMenuFlags::kOpen },
    { "Open archive",          NContextMenuFlags::kOpenAs },
    { "Extract files...",      NContextMenuFlags::kExtract },
    { "Extract Here",          NContextMenuFlags::kExtractHere },
    { "Extract to subfolder",  NContextMenuFlags::kExtractTo },
    { "Test archive",          NContextMenuFlags::kTest },
    { "Add to archive...",     NContextMenuFlags::kCompress },
    { "Add to .7z",            NContextMenuFlags::kCompressTo7z },
    { "Add to .zip",           NContextMenuFlags::kCompressToZip },
    { "CRC checksum",          NContextMenuFlags::kCRC },
    { "CRC checksum submenu",  NContextMenuFlags::kCRC_Cascaded }
  };
  for (unsigned i = 0; i < sizeof(kMenuItems) / sizeof(kMenuItems[0]); i++)
  {
    QTreeWidgetItem *item = new QTreeWidgetItem(_menuItems);
    item->setText(0, tr(kMenuItems[i].Text));
    item->setData(0, Qt::UserRole, (uint)kMenuItems[i].Flag);
    item->setCheckState(0, (s.ContextMenuFlags & kMenuItems[i].Flag)
        ? Qt::Checked : Qt::Unchecked);
  }
  l->addWidget(_menuItems, 1);
  return page;
}


QWidget *COptionsDialog::CreateFoldersPage()
{
  const CFmSettings &s = GlobalFmSettings();

  QWidget *page = new QWidget;
  QVBoxLayout *l = new QVBoxLayout(page);

  _workSystem = new QRadioButton(tr("&System temp folder"));
  l->addWidget(MakeLabel(tr("&Working folder"), _workSystem));
  l->addWidget(_workSystem);
  l->addWidget(_workCurrent = new QRadioButton(tr("&Current")));
  l->addWidget(_workSpecified = new QRadioButton(tr("Specified:")));
  l->addWidget(MakeBrowseRow(&_workPath, page, true));
  l->addWidget(_workRemovable = new QCheckBox(tr("Use for removable drives only")));
  l->addStretch(1);

  _workSystem->setChecked(s.WorkDirMode == 0);
  _workCurrent->setChecked(s.WorkDirMode == 1);
  _workSpecified->setChecked(s.WorkDirMode == 2);
  _workPath->setText(s.WorkDirPath);
  _workRemovable->setChecked(s.WorkDirForRemovableOnly);

  _workPath->setEnabled(s.WorkDirMode == 2);
  connect(_workSpecified, &QRadioButton::toggled, _workPath, &QWidget::setEnabled);
  return page;
}


QWidget *COptionsDialog::CreateEditorPage()
{
  const CFmSettings &s = GlobalFmSettings();

  QWidget *page = new QWidget;
  QVBoxLayout *l = new QVBoxLayout(page);

  QLabel *viewLabel = new QLabel(tr("&View:"));
  l->addWidget(viewLabel);
  l->addWidget(MakeBrowseRow(&_viewer, page, false, viewLabel));

  QLabel *editLabel = new QLabel(tr("&Editor:"));
  l->addWidget(editLabel);
  l->addWidget(MakeBrowseRow(&_editor, page, false, editLabel));

  QLabel *diffLabel = new QLabel(tr("&Diff:"));
  l->addWidget(diffLabel);
  l->addWidget(MakeBrowseRow(&_diff, page, false, diffLabel));
  l->addStretch(1);

  _viewer->setText(s.Viewer);
  _editor->setText(s.Editor);
  _diff->setText(s.Diff);
  return page;
}


QWidget *COptionsDialog::CreateSettingsPage()
{
  const CFmSettings &s = GlobalFmSettings();

  QWidget *page = new QWidget;
  QVBoxLayout *l = new QVBoxLayout(page);

  l->addWidget(_showDots = new QCheckBox(tr("Show \"..\" item")));
  l->addWidget(_showRealFileIcons = new QCheckBox(tr("Show real file &icons")));
  l->addWidget(_fullRow = new QCheckBox(tr("&Full row select")));
  l->addWidget(_showGrid = new QCheckBox(tr("Show &grid lines")));
  l->addWidget(_singleClick = new QCheckBox(tr("&Single-click to open an item")));
  l->addWidget(_alternativeSelection = new QCheckBox(tr("&Alternative selection mode")));

  _showDots->setChecked(s.ShowDots);
  _showRealFileIcons->setChecked(s.ShowRealFileIcons);
  _fullRow->setChecked(s.FullRow);
  _showGrid->setChecked(s.ShowGrid);
  _singleClick->setChecked(s.SingleClick);
  _alternativeSelection->setChecked(s.AlternativeSelection);

  l->addSpacing(12);
  l->addWidget(new QLabel(
      tr("Maximum amount of RAM memory usage allowed to unpack archives:")));

  _memLimit = new QCheckBox;
  _memLimitValue = new QSpinBox;
  _memLimitValue->setRange(1, 1024);
  _memLimitValue->setValue(s.MemLimitGb);
  _memLimit->setChecked(s.MemLimitDefined);
  _memLimitValue->setEnabled(s.MemLimitDefined);
  connect(_memLimit, &QCheckBox::toggled, _memLimitValue, &QWidget::setEnabled);

  QHBoxLayout *memRow = new QHBoxLayout;
  memRow->addSpacing(16);
  memRow->addWidget(_memLimit);
  memRow->addWidget(_memLimitValue);
  memRow->addWidget(new QLabel(tr("GB")));
  memRow->addStretch(1);
  l->addLayout(memRow);

  l->addStretch(1);
  return page;
}


QWidget *COptionsDialog::CreateLanguagePage()
{
  const CFmSettings &s = GlobalFmSettings();

  QWidget *page = new QWidget;
  QVBoxLayout *l = new QVBoxLayout(page);

  QLabel *label = new QLabel(tr("Language:"));
  _language = new QComboBox;
  label->setBuddy(_language);

  const QList<QPair<QString, QString> > langs = AvailableLanguages();
  for (const auto &lang : langs)
    _language->addItem(lang.first, lang.second);
  const int index = _language->findData(s.Language);
  _language->setCurrentIndex(index >= 0 ? index : 0);

  _languageInfo = new QLabel;
  _languageInfo->setWordWrap(true);
  if (langs.size() <= 1)
    _languageInfo->setText(tr(
        "No translation files were found.\n"
        "Place 7zQ_<code>.qm files in a \"translations\" folder next to the\n"
        "program, or in %1, to make more languages available.")
        .arg(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
             + QStringLiteral("/translations")));
  else
    _languageInfo->setText(tr("A language change takes effect after a restart."));

  l->addWidget(label);
  l->addWidget(_language);
  l->addWidget(_languageInfo);
  l->addStretch(1);
  return page;
}


COptionsDialog::COptionsDialog(QWidget *parent): QDialog(parent)
{
  setWindowTitle(tr("Options"));

  // Same page order as OptionsDialog.cpp.
  QTabWidget *tabs = new QTabWidget;
  tabs->addTab(CreateSystemPage(),   tr("System"));
  tabs->addTab(CreateSevenZipPage(), tr("7-Zip"));
  tabs->addTab(CreateFoldersPage(),  tr("Folders"));
  tabs->addTab(CreateEditorPage(),   tr("Editor"));
  tabs->addTab(CreateSettingsPage(), tr("Settings"));
  tabs->addTab(CreateLanguagePage(), tr("Language"));

  QDialogButtonBox *box = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addWidget(tabs, 1);
  main->addWidget(box);

  connect(box, &QDialogButtonBox::accepted, this, &COptionsDialog::accept);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  resize(560, 520);
}


void COptionsDialog::ApplyAssociations()
{
  QSet<QString> wanted;
  for (int i = 0; i < _assocList->topLevelItemCount(); i++)
  {
    QTreeWidgetItem *item = _assocList->topLevelItem(i);
    if (item->checkState(0) == Qt::Checked)
      wanted.insert(item->data(0, Qt::UserRole).toString());
  }
  WriteAssociatedMimeTypes(wanted);
}


void COptionsDialog::accept()
{
  CFmSettings &s = GlobalFmSettings();

  s.ShowDots = _showDots->isChecked();
  s.ShowRealFileIcons = _showRealFileIcons->isChecked();
  s.FullRow = _fullRow->isChecked();
  s.ShowGrid = _showGrid->isChecked();
  s.SingleClick = _singleClick->isChecked();
  s.AlternativeSelection = _alternativeSelection->isChecked();
  s.MemLimitDefined = _memLimit->isChecked();
  s.MemLimitGb = _memLimitValue->value();

  s.CascadedMenu = _cascadedMenu->isChecked();
  s.MenuIcons = _menuIcons->isChecked();
  s.ElimDup = _elimDup->isChecked();
  {
    quint32 flags = 0;
    for (int i = 0; i < _menuItems->topLevelItemCount(); i++)
    {
      QTreeWidgetItem *item = _menuItems->topLevelItem(i);
      if (item->checkState(0) == Qt::Checked)
        flags |= (quint32)item->data(0, Qt::UserRole).toUInt();
    }
    s.ContextMenuFlags = flags;
  }

  s.WorkDirMode = _workSpecified->isChecked() ? 2 : (_workCurrent->isChecked() ? 1 : 0);
  s.WorkDirPath = _workPath->text();
  s.WorkDirForRemovableOnly = _workRemovable->isChecked();

  s.Viewer = _viewer->text();
  s.Editor = _editor->text();
  s.Diff = _diff->text();

  s.Language = _language->currentData().toString();

  s.Save();
  ApplyAssociations();
  QDialog::accept();
}
