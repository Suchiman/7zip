// Qt/MainWindow.h
//
// Port of UI/FileManager/App.{h,cpp}, MyLoadMenu.cpp and resource.rc:
// the frame that owns one or two panels, the menu bar, the toolbar and the
// status bar.

#ifndef ZIP7_INC_QT_MAIN_WINDOW_H
#define ZIP7_INC_QT_MAIN_WINDOW_H

#include <QMainWindow>
#include <QRect>
#include <QSize>
#include <QVector>

class CPanel;
class QAction;
class QMenu;
class QLabel;
class QSplitter;

// The default window size for a given screen work area. Free function so the
// rule can be checked without a display.
QSize DefaultMainWindowSize(const QRect &available);

class CMainWindow: public QMainWindow
{
  Q_OBJECT
public:
  explicit CMainWindow(QWidget *parent = nullptr);
  ~CMainWindow() override;

  // Opens (path) in the active panel; used for the command-line argument.
  void OpenPath(const QString &path);

private slots:
  void OnPanelFolderChanged();
  void OnPanelSelectionChanged();
  void OnPanelActivated();
  void OnPanelContextMenu(const QPoint &globalPos);

  void OnTwoPanels(bool on);
  void OnFlatView(bool on);
  void OnRefresh();
  void OnOptions();
  void OnBenchmark();
  void OnAbout();

private:
  void CreateActions();
  void CreateMenus();
  // Fills (menu) with the File-menu commands. The menu bar and the panel
  // context menu are built from the same list, as in 7-Zip.
  void FillFileMenu(QMenu *menu, bool programMenu);
  void CreateToolBar();
  void CreateStatusBar();
  void UpdateStatusBar();
  void UpdateActionStates();
  void SaveSettings();
  void LoadSettings();

  CPanel *ActivePanel() const { return _panels[_activePanel]; }

  QMenu *_contextMenu;
  QVector<CPanel *> _panels;
  int _activePanel;
  QSplitter *_splitter;
  QLabel *_statusLabel;

  // File
  QAction *_actOpen;
  QAction *_actOpenInside;
  QAction *_actOpenInsideOne;     // "Open Inside *"
  QAction *_actOpenInsideParser;  // "Open Inside #"
  QAction *_actOpenOutside;
  QAction *_actView;
  QAction *_actEdit;
  QAction *_actRename;
  QAction *_actCopyTo;
  QAction *_actMoveTo;
  QAction *_actDelete;
  QAction *_actSplit;
  QAction *_actCombine;
  QAction *_actProperties;
  QAction *_actCreateFolder;
  QAction *_actCreateFile;
  QAction *_actExit;
  // Edit
  QAction *_actSelectAll;
  QAction *_actDeselectAll;
  QAction *_actInvertSelection;
  QAction *_actSelect;
  QAction *_actDeselect;
  QAction *_actSelectByType;
  QAction *_actDeselectByType;
  // View
  QAction *_actFlatView;
  QAction *_actTwoPanels;
  QAction *_actOpenRootFolder;
  QAction *_actOpenParentFolder;
  QAction *_actRefresh;
  QAction *_actSortName;
  QAction *_actSortType;
  QAction *_actSortDate;
  QAction *_actSortSize;
  QAction *_actSortNone;
  QAction *_actBack;
  QAction *_actForward;
  // Tools / Help
  QAction *_actOptions;
  QAction *_actBenchmark;
  QAction *_actAbout;
  // Archive toolbar
  QAction *_actAdd;
  QAction *_actExtract;
  QAction *_actTest;
  QAction *_actInfo;
  QAction *_actExtractHere;
  QAction *_actExtractTo;
};

#endif
