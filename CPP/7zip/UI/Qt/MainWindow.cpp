// Qt/MainWindow.cpp

#include "StdAfx.h"

#include "../../PropID.h"

#include "MainWindow.h"
#include "PanelModel.h"
#include "BenchmarkDialog.h"
#include "IconUtils.h"
#include "OptionsDialog.h"
#include "Panel.h"
#include <QIcon>
#include "SmallDialogs.h"
#include "Z7Qt.h"

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeView>

static const char * const kSettingsOrg = "7-Zip";
static const char * const kSettingsApp = "7zQ";


// The size a first run gets. A fixed pixel size cannot be right for both a
// 1366x768 laptop and a 3440x1440 desktop, so this takes a share of the work
// area and bounds it: big enough for the default columns, never wider than a
// comfortable reading width, and never larger than the screen itself.
QSize DefaultMainWindowSize(const QRect &available)
{
  const int w = qBound(720, available.width()  * 3 / 4, 1600);
  const int h = qBound(480, available.height() * 3 / 4, 1000);
  return QSize(qMin(w, available.width()), qMin(h, available.height()));
}


CMainWindow::CMainWindow(QWidget *parent):
    QMainWindow(parent), _contextMenu(nullptr), _activePanel(0)
{
  setWindowTitle(tr("7-Zip File Manager"));
  setWindowIcon(GetAppIcon());
  setMinimumSize(480, 320);

  _splitter = new QSplitter(Qt::Horizontal);
  for (int i = 0; i < 2; i++)
  {
    CPanel *panel = new CPanel;
    _panels.append(panel);
    _splitter->addWidget(panel);
    connect(panel, &CPanel::FolderChanged, this, &CMainWindow::OnPanelFolderChanged);
    connect(panel, &CPanel::SelectionChanged, this, &CMainWindow::OnPanelSelectionChanged);
    connect(panel, &CPanel::PanelActivated, this, &CMainWindow::OnPanelActivated);
    connect(panel, &CPanel::ContextMenuRequested, this, &CMainWindow::OnPanelContextMenu);
  }
  _panels[1]->hide();
  setCentralWidget(_splitter);

  CreateActions();
  CreateMenus();
  CreateToolBar();
  CreateStatusBar();
  LoadSettings();

  for (CPanel *panel : _panels)
  {
    panel->ApplySettings();
    if (panel->BindToPathAndRefresh(QDir::currentPath()) != S_OK)
      panel->OpenRootFolder();
  }

  _panels[0]->View()->setFocus();
  UpdateActionStates();
  UpdateStatusBar();
}

CMainWindow::~CMainWindow()
{
  SaveSettings();
}


void CMainWindow::CreateActions()
{
  // ---- File ----
  _actOpen = new QAction(tr("&Open"), this);
  _actOpen->setShortcut(QKeySequence(Qt::Key_Return));
  _actOpen->setShortcutContext(Qt::WidgetShortcut);
  connect(_actOpen, &QAction::triggered, this, [this]{ ActivePanel()->OpenSelectedItem(); });

  _actOpenInside = new QAction(tr("Open &Inside"), this);
  _actOpenInside->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown));
  connect(_actOpenInside, &QAction::triggered, this, [this]
  {
    const int i = ActivePanel()->FocusedItemIndex();
    if (i >= 0)
      ActivePanel()->OpenItemInside(i);
  });

  _actOpenInsideOne = new QAction(tr("Open Inside *"), this);
  connect(_actOpenInsideOne, &QAction::triggered, this, [this]
  {
    const int i = ActivePanel()->FocusedItemIndex();
    if (i >= 0)
      ActivePanel()->OpenItemInside(i, QStringLiteral("*"));
  });

  _actOpenInsideParser = new QAction(tr("Open Inside #"), this);
  connect(_actOpenInsideParser, &QAction::triggered, this, [this]
  {
    const int i = ActivePanel()->FocusedItemIndex();
    if (i >= 0)
      ActivePanel()->OpenItemInside(i, QStringLiteral("#"));
  });

  _actOpenOutside = new QAction(tr("Open O&utside"), this);
  _actOpenOutside->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return));
  connect(_actOpenOutside, &QAction::triggered, this, [this]
  {
    const int i = ActivePanel()->FocusedItemIndex();
    if (i >= 0)
      ActivePanel()->OpenItemOutside(i);
  });

  _actView = new QAction(tr("&View"), this);
  _actView->setShortcut(QKeySequence(Qt::Key_F3));
  connect(_actView, &QAction::triggered, this, [this]
  {
    ActivePanel()->ViewItem(ActivePanel()->FocusedItemIndex(), false);
  });

  _actEdit = new QAction(tr("&Edit"), this);
  _actEdit->setShortcut(QKeySequence(Qt::Key_F4));
  connect(_actEdit, &QAction::triggered, this, [this]
  {
    ActivePanel()->ViewItem(ActivePanel()->FocusedItemIndex(), true);
  });

  _actRename = new QAction(tr("Rena&me"), this);
  _actRename->setShortcut(QKeySequence(Qt::Key_F2));
  connect(_actRename, &QAction::triggered, this, [this]{ ActivePanel()->RenameCommand(); });

  _actCopyTo = new QAction(GetCommandIcon(QStringLiteral("Copy")), tr("&Copy To..."), this);
  _actCopyTo->setShortcut(QKeySequence(Qt::Key_F5));
  connect(_actCopyTo, &QAction::triggered, this, [this]{ ActivePanel()->CopyTo(); });

  _actMoveTo = new QAction(GetCommandIcon(QStringLiteral("Move")), tr("&Move To..."), this);
  _actMoveTo->setShortcut(QKeySequence(Qt::Key_F6));
  connect(_actMoveTo, &QAction::triggered, this, [this]{ ActivePanel()->MoveTo(); });

  _actDelete = new QAction(GetCommandIcon(QStringLiteral("Delete")), tr("&Delete"), this);
  _actDelete->setShortcut(QKeySequence(Qt::Key_Delete));
  connect(_actDelete, &QAction::triggered, this, [this]{ ActivePanel()->DeleteItems(false); });

  _actSplit = new QAction(tr("&Split file..."), this);
  connect(_actSplit, &QAction::triggered, this, [this]{ ActivePanel()->SplitFileCommand(); });

  _actCombine = new QAction(tr("Com&bine files..."), this);
  connect(_actCombine, &QAction::triggered, this, [this]{ ActivePanel()->CombineFilesCommand(); });

  _actProperties = new QAction(tr("P&roperties"), this);
  _actProperties->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));
  connect(_actProperties, &QAction::triggered, this, [this]{ ActivePanel()->ShowPropertiesCommand(); });

  _actCreateFolder = new QAction(tr("Create Folder"), this);
  _actCreateFolder->setShortcut(QKeySequence(Qt::Key_F7));
  connect(_actCreateFolder, &QAction::triggered, this, [this]{ ActivePanel()->CreateFolderCommand(); });

  _actCreateFile = new QAction(tr("Create File"), this);
  _actCreateFile->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
  connect(_actCreateFile, &QAction::triggered, this, [this]{ ActivePanel()->CreateFileCommand(); });

  _actExit = new QAction(tr("E&xit"), this);
  _actExit->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F4));
  connect(_actExit, &QAction::triggered, this, &QWidget::close);

  // ---- Edit ----
  _actSelectAll = new QAction(tr("Select &All"), this);
  _actSelectAll->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
  connect(_actSelectAll, &QAction::triggered, this, [this]{ ActivePanel()->SelectAll(true); });

  _actDeselectAll = new QAction(tr("Deselect All"), this);
  connect(_actDeselectAll, &QAction::triggered, this, [this]{ ActivePanel()->SelectAll(false); });

  _actInvertSelection = new QAction(tr("&Invert Selection"), this);
  _actInvertSelection->setShortcut(QKeySequence(Qt::Key_Asterisk));
  connect(_actInvertSelection, &QAction::triggered, this, [this]{ ActivePanel()->InvertSelection(); });

  _actSelect = new QAction(tr("Select..."), this);
  _actSelect->setShortcut(QKeySequence(Qt::Key_Plus));
  connect(_actSelect, &QAction::triggered, this, [this]{ ActivePanel()->SelectByMask(true); });

  _actDeselect = new QAction(tr("Deselect..."), this);
  _actDeselect->setShortcut(QKeySequence(Qt::Key_Minus));
  connect(_actDeselect, &QAction::triggered, this, [this]{ ActivePanel()->SelectByMask(false); });

  _actSelectByType = new QAction(tr("Select by Type"), this);
  connect(_actSelectByType, &QAction::triggered, this, [this]{ ActivePanel()->SelectSameExtension(true); });

  _actDeselectByType = new QAction(tr("Deselect by Type"), this);
  connect(_actDeselectByType, &QAction::triggered, this, [this]{ ActivePanel()->SelectSameExtension(false); });

  // ---- View ----
  _actFlatView = new QAction(tr("Flat View"), this);
  _actFlatView->setCheckable(true);
  connect(_actFlatView, &QAction::toggled, this, &CMainWindow::OnFlatView);

  _actTwoPanels = new QAction(tr("&2 Panels"), this);
  _actTwoPanels->setCheckable(true);
  _actTwoPanels->setShortcut(QKeySequence(Qt::Key_F9));
  connect(_actTwoPanels, &QAction::toggled, this, &CMainWindow::OnTwoPanels);

  _actOpenRootFolder = new QAction(tr("Open Root Folder"), this);
  _actOpenRootFolder->setShortcut(QKeySequence(Qt::Key_Backslash));
  connect(_actOpenRootFolder, &QAction::triggered, this, [this]{ ActivePanel()->OpenRootFolder(); });

  _actOpenParentFolder = new QAction(GetCommandIcon(QStringLiteral("Up")), tr("Up One Level"), this);
  connect(_actOpenParentFolder, &QAction::triggered, this, [this]{ ActivePanel()->OpenParentFolder(); });

  _actRefresh = new QAction(GetCommandIcon(QStringLiteral("Refresh")), tr("&Refresh"), this);
  _actRefresh->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
  connect(_actRefresh, &QAction::triggered, this, &CMainWindow::OnRefresh);

  _actBack = new QAction(GetCommandIcon(QStringLiteral("Back")), tr("Back"), this);
  _actBack->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
  connect(_actBack, &QAction::triggered, this, [this]{ ActivePanel()->HistoryBack(); });

  _actForward = new QAction(GetCommandIcon(QStringLiteral("Forward")), tr("Forward"), this);
  _actForward->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
  connect(_actForward, &QAction::triggered, this, [this]{ ActivePanel()->HistoryForward(); });

  struct { QAction **act; const char *text; PROPID propID; int key; } kSorts[] =
  {
    { &_actSortName, "Name", kpidName,      Qt::Key_F3 },
    { &_actSortType, "Type", kpidExtension, Qt::Key_F4 },
    { &_actSortDate, "Date", kpidMTime,     Qt::Key_F5 },
    { &_actSortSize, "Size", kpidSize,      Qt::Key_F6 },
    { &_actSortNone, "Unsorted", kpidNoProperty, Qt::Key_F7 }
  };
  for (unsigned i = 0; i < sizeof(kSorts) / sizeof(kSorts[0]); i++)
  {
    QAction *a = new QAction(tr(kSorts[i].text), this);
    a->setShortcut(QKeySequence(Qt::CTRL | kSorts[i].key));
    const PROPID propID = kSorts[i].propID;
    connect(a, &QAction::triggered, this, [this, propID]
    {
      ActivePanel()->Model()->SortByPropID(propID);
    });
    *kSorts[i].act = a;
  }

  // ---- Tools / Help ----
  _actOptions = new QAction(tr("&Options..."), this);
  connect(_actOptions, &QAction::triggered, this, &CMainWindow::OnOptions);

  _actBenchmark = new QAction(tr("&Benchmark"), this);
  connect(_actBenchmark, &QAction::triggered, this, &CMainWindow::OnBenchmark);

  _actAbout = new QAction(tr("&About 7-Zip..."), this);
  connect(_actAbout, &QAction::triggered, this, &CMainWindow::OnAbout);

  // ---- archive toolbar ----
  _actAdd = new QAction(GetCommandIcon(QStringLiteral("Add")), tr("Add"), this);
  connect(_actAdd, &QAction::triggered, this, [this]{ ActivePanel()->CompressCommand(); });

  _actExtract = new QAction(GetCommandIcon(QStringLiteral("Extract")), tr("Extract"), this);
  connect(_actExtract, &QAction::triggered, this, [this]{ ActivePanel()->ExtractCommand(true, false); });

  _actExtractHere = new QAction(tr("Extract Here"), this);
  connect(_actExtractHere, &QAction::triggered, this, [this]{ ActivePanel()->ExtractCommand(false, false); });

  _actExtractTo = new QAction(tr("Extract to subfolder"), this);
  connect(_actExtractTo, &QAction::triggered, this, [this]{ ActivePanel()->ExtractCommand(false, true); });

  _actTest = new QAction(GetCommandIcon(QStringLiteral("Test")), tr("Test"), this);
  connect(_actTest, &QAction::triggered, this, [this]{ ActivePanel()->TestArchiveCommand(); });

  _actInfo = new QAction(GetCommandIcon(QStringLiteral("Info")), tr("Info"), this);
  connect(_actInfo, &QAction::triggered, this, [this]{ ActivePanel()->ShowPropertiesCommand(); });
}


void CMainWindow::CreateMenus()
{
  QMenu *file = menuBar()->addMenu(tr("&File"));
  FillFileMenu(file, true);   // programMenu: includes Exit

  QMenu *edit = menuBar()->addMenu(tr("&Edit"));
  edit->addAction(_actSelectAll);
  edit->addAction(_actDeselectAll);
  edit->addAction(_actInvertSelection);
  edit->addAction(_actSelect);
  edit->addAction(_actDeselect);
  edit->addSeparator();
  edit->addAction(_actSelectByType);
  edit->addAction(_actDeselectByType);

  QMenu *view = menuBar()->addMenu(tr("&View"));
  view->addAction(_actSortName);
  view->addAction(_actSortType);
  view->addAction(_actSortDate);
  view->addAction(_actSortSize);
  view->addAction(_actSortNone);
  view->addSeparator();
  view->addAction(_actFlatView);
  view->addAction(_actTwoPanels);
  view->addSeparator();
  view->addAction(_actOpenRootFolder);
  view->addAction(_actOpenParentFolder);
  view->addAction(_actBack);
  view->addAction(_actForward);
  view->addAction(_actRefresh);

  QMenu *tools = menuBar()->addMenu(tr("&Tools"));
  tools->addAction(_actOptions);
  tools->addSeparator();
  tools->addAction(_actBenchmark);

  QMenu *help = menuBar()->addMenu(tr("&Help"));
  help->addAction(_actAbout);
}


// The File menu of UI/FileManager/resource.rc, in its original order. 7-Zip
// builds the panel context menu from exactly this list, dropping Exit
// (CFileMenu::Load() skips IDCLOSE when programMenu is false).
void CMainWindow::FillFileMenu(QMenu *menu, bool programMenu)
{
  menu->addAction(_actOpen);
  menu->addAction(_actOpenInside);
  menu->addAction(_actOpenInsideOne);
  menu->addAction(_actOpenInsideParser);
  menu->addAction(_actOpenOutside);
  menu->addAction(_actView);
  menu->addAction(_actEdit);
  menu->addSeparator();
  menu->addAction(_actRename);
  menu->addAction(_actCopyTo);
  menu->addAction(_actMoveTo);
  menu->addAction(_actDelete);
  menu->addSeparator();
  menu->addAction(_actSplit);
  menu->addAction(_actCombine);
  menu->addSeparator();
  menu->addAction(_actProperties);
  {
    QMenu *crc = menu->addMenu(tr("CRC"));
    static const char * const kHashNames[] =
    {
      "CRC32", "CRC64", "XXH64", "MD5", "SHA1", "SHA256",
      "SHA384", "SHA512", "SHA3-256", "BLAKE2sp", "*"
    };
    for (unsigned i = 0; i < sizeof(kHashNames) / sizeof(kHashNames[0]); i++)
    {
      const QString name = QString::fromLatin1(kHashNames[i]);
      QAction *a = crc->addAction(name);
      connect(a, &QAction::triggered, this, [this, name]
      {
        ActivePanel()->CalculateCrcCommand(name);
      });
    }
  }
  menu->addSeparator();
  menu->addAction(_actCreateFolder);
  menu->addAction(_actCreateFile);
  if (programMenu)
  {
    menu->addSeparator();
    menu->addAction(_actExit);
  }
}


void CMainWindow::OnPanelContextMenu(const QPoint &globalPos)
{
  // Rebuilt every time, because Options > 7-Zip decides which commands are
  // in it and whether they sit in a submenu.
  delete _contextMenu;
  _contextMenu = new QMenu(this);

  const CFmSettings &s = GlobalFmSettings();
  QMenu *target = _contextMenu;
  if (s.CascadedMenu)
    target = _contextMenu->addMenu(s.MenuIcons ? GetAppIcon() : QIcon(), tr("7-&Zip"));

  bool any = false;
  const struct { quint32 Flag; QAction *Action; } kItems[] =
  {
    { NContextMenuFlags::kOpenAs,       _actOpenInside },
    { NContextMenuFlags::kExtract,      _actExtract },
    { NContextMenuFlags::kExtractHere,  _actExtractHere },
    { NContextMenuFlags::kExtractTo,    _actExtractTo },
    { NContextMenuFlags::kTest,         _actTest },
    { NContextMenuFlags::kCompress,     _actAdd }
  };
  for (unsigned i = 0; i < sizeof(kItems) / sizeof(kItems[0]); i++)
    if (s.ContextMenuFlags & kItems[i].Flag)
    {
      target->addAction(kItems[i].Action);
      any = true;
    }
  if (any && !s.CascadedMenu)
    _contextMenu->addSeparator();

  FillFileMenu(_contextMenu, false);   // no Exit in a context menu
  UpdateActionStates();
  _contextMenu->popup(globalPos);
}


void CMainWindow::CreateToolBar()
{
  QToolBar *bar = addToolBar(tr("Standard Toolbar"));
  bar->setObjectName(QStringLiteral("standardToolBar"));
  bar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  bar->addAction(_actAdd);
  bar->addAction(_actExtract);
  bar->addAction(_actTest);
  bar->addSeparator();
  bar->addAction(_actCopyTo);
  bar->addAction(_actMoveTo);
  bar->addAction(_actDelete);
  bar->addAction(_actInfo);
}


void CMainWindow::CreateStatusBar()
{
  _statusLabel = new QLabel;
  statusBar()->addWidget(_statusLabel, 1);
}

void CMainWindow::UpdateStatusBar()
{
  _statusLabel->setText(ActivePanel()->StatusText());
}

// The same rules as CFileMenu::Load() in UI/FileManager/MyLoadMenu.cpp.
void CMainWindow::UpdateActionStates()
{
  CPanel *panel = ActivePanel();
  const CPanelModel *model = panel->Model();

  const bool isFsFolder = model->IsFsFolder();
  const bool readOnly = !isFsFolder && model->Operations() == NULL;
  const bool hasOps = model->Operations() != NULL;

  const QVector<int> operated = panel->OperatedItemIndices();
  const unsigned numItems = (unsigned)operated.size();

  bool allAreFiles = true;
  for (int i : operated)
    if (model->IsItemFolder(i))
      allAreFiles = false;

  // Split and Combine only work on exactly one file that lives on disk.
  const bool isOneFsFile = (isFsFolder && numItems == 1 && allAreFiles);
  const bool hasItem = (numItems != 0);

  _actOpen->setEnabled(hasItem);
  _actOpenInside->setEnabled(hasItem && allAreFiles);
  _actOpenInsideOne->setEnabled(hasItem && allAreFiles);
  _actOpenInsideParser->setEnabled(hasItem && allAreFiles);
  _actOpenOutside->setEnabled(hasItem);
  _actView->setEnabled(numItems == 1 && allAreFiles);
  _actEdit->setEnabled(numItems == 1 && allAreFiles);

  _actRename->setEnabled(hasOps && !readOnly && numItems == 1);
  _actCopyTo->setEnabled(hasItem);
  _actMoveTo->setEnabled(hasItem && hasOps && !readOnly);
  _actDelete->setEnabled(hasItem && hasOps && !readOnly);

  _actSplit->setEnabled(isOneFsFile);
  _actCombine->setEnabled(isOneFsFile);
  _actProperties->setEnabled(hasItem);

  _actCreateFolder->setEnabled(hasOps && !readOnly);
  _actCreateFile->setEnabled(hasOps && !readOnly);

  _actAdd->setEnabled(isFsFolder && hasItem);
  _actExtract->setEnabled(hasItem);
  _actExtractHere->setEnabled(hasItem);
  _actExtractTo->setEnabled(isFsFolder && hasItem);
  _actTest->setEnabled(hasItem);
  _actInfo->setEnabled(hasItem);

  _actBack->setEnabled(panel->CanGoBack());
  _actForward->setEnabled(panel->CanGoForward());
}


void CMainWindow::OnPanelFolderChanged()
{
  CPanel *panel = qobject_cast<CPanel *>(sender());
  if (panel && panel == ActivePanel())
  {
    setWindowTitle(panel->CurrentPath().isEmpty()
        ? tr("7-Zip File Manager")
        : panel->CurrentPath() + QStringLiteral(" - ") + tr("7-Zip File Manager"));
  }
  UpdateActionStates();
  UpdateStatusBar();
}

void CMainWindow::OnPanelSelectionChanged()
{
  UpdateStatusBar();
  UpdateActionStates();
}

void CMainWindow::OnPanelActivated()
{
  CPanel *panel = qobject_cast<CPanel *>(sender());
  if (!panel)
    return;
  const int index = _panels.indexOf(panel);
  if (index >= 0 && index != _activePanel)
  {
    _activePanel = index;
    UpdateActionStates();
    UpdateStatusBar();
  }
}


void CMainWindow::OnTwoPanels(bool on)
{
  _panels[1]->setVisible(on);
  if (on && _panels[1]->Model()->Folder() == NULL)
    _panels[1]->BindToPathAndRefresh(QDir::homePath());
  if (!on && _activePanel == 1)
  {
    _activePanel = 0;
    _panels[0]->View()->setFocus();
  }
}

void CMainWindow::OnFlatView(bool on)
{
  ActivePanel()->SetFlatMode(on);
}

void CMainWindow::OnRefresh()
{
  ActivePanel()->RefreshListing();
}

void CMainWindow::OnOptions()
{
  COptionsDialog dlg(this);
  if (dlg.exec() != QDialog::Accepted)
    return;
  for (CPanel *panel : _panels)
    panel->ApplySettings();
}

void CMainWindow::OnBenchmark()
{
  CBenchmarkDialog dlg(this);
  dlg.exec();
}

void CMainWindow::OnAbout()
{
  CAboutDialog dlg(this);
  dlg.exec();
}


void CMainWindow::OpenPath(const QString &path)
{
  if (ActivePanel()->BindToPathAndRefresh(path) != S_OK)
    ActivePanel()->OpenRootFolder();
}


void CMainWindow::SaveSettings()
{
  QSettings s{QLatin1String(kSettingsOrg), QLatin1String(kSettingsApp)};
  s.setValue(QStringLiteral("geometry"), saveGeometry());
  s.setValue(QStringLiteral("windowState"), saveState());
  s.setValue(QStringLiteral("twoPanels"), _actTwoPanels->isChecked());
  s.setValue(QStringLiteral("splitter"), _splitter->saveState());
  for (int i = 0; i < _panels.size(); i++)
    s.setValue(QStringLiteral("panel%1/path").arg(i), _panels[i]->CurrentPath());
}

void CMainWindow::LoadSettings()
{
  QSettings s{QLatin1String(kSettingsOrg), QLatin1String(kSettingsApp)};
  // restoreGeometry() fails on an empty or unusable value, which is also the
  // first-run case.
  if (!restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray()))
  {
    const QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen();
    const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1024, 768);
    const QSize size = DefaultMainWindowSize(avail);
    resize(size);
    move(avail.center() - QPoint(size.width() / 2, size.height() / 2));
  }
  restoreState(s.value(QStringLiteral("windowState")).toByteArray());
  _actTwoPanels->setChecked(s.value(QStringLiteral("twoPanels"), false).toBool());
  _splitter->restoreState(s.value(QStringLiteral("splitter")).toByteArray());
}
