// Qt/Panel.h
//
// Port of UI/FileManager/Panel*.cpp: one browsing pane, i.e. the address bar,
// the item list and everything that can be done to the selected items.
// Browsing into an archive works exactly as in 7-Zip: the archive is opened
// through IFolderManager::OpenFolderFile and the resulting IFolderFolder
// replaces the current one, so the list code does not know the difference.

#ifndef ZIP7_INC_QT_PANEL_H
#define ZIP7_INC_QT_PANEL_H

#include "../../../Common/MyCom.h"
#include "../FileManager/IFolder.h"

#include "PanelModel.h"

#include <QWidget>
#include <QVector>
#include <QSharedPointer>
#include <QTemporaryDir>

class QComboBox;
class QLabel;
class QToolButton;
class QTreeView;

// One entry of the "folders we came from" stack. An archive adds a link here,
// the same way CPanel::_parentFolders does in 7-Zip.
struct CFolderLink
{
  CMyComPtr<IFolderFolder> ParentFolder;
  UString ItemName;         // name of the item we opened (the archive file)
  UString ParentFolderPath;
  bool IsArchive;

  CFolderLink(): IsArchive(false) {}
};


class CPanel: public QWidget
{
  Q_OBJECT
public:
  explicit CPanel(QWidget *parent = nullptr);
  ~CPanel() override;

  CPanelModel *Model() const { return _model; }
  QTreeView *View() const { return _view; }

  // ---- navigation ----
  HRESULT BindToPathAndRefresh(const QString &path);
  // Goes to (path) reusing whatever is already open. Only the part of the
  // chain that actually differs is closed or opened, so stepping out of a
  // folder inside an archive and back in costs nothing -- re-opening a big
  // image just to walk one level is what made the history feel slow.
  HRESULT NavigateTo(const QString &path);
  HRESULT SetFolder(IFolderFolder *folder, bool keepHistory = true);
  HRESULT OpenParentFolder();
  HRESULT OpenRootFolder();
  void OpenSelectedItem();
  void RefreshListing();
  void HistoryBack();
  void HistoryForward();
  bool CanGoBack() const { return _historyPos > 0; }
  bool CanGoForward() const { return _historyPos + 1 < _history.size(); }

  // The path shown in the address bar, e.g. "/home/me/a.7z/sub/".
  QString CurrentPath() const;
  // The plain file-system path of the current folder, empty inside an archive.
  QString CurrentFsPath() const;
  // The file-system path of the archive we are inside, empty otherwise.
  QString CurrentArchivePath() const;

  // ---- selection ----
  QVector<int> SelectedItemIndices() const;   // folder item indices
  QVector<int> OperatedItemIndices() const;   // selection, or the focused item
  int FocusedItemIndex() const;
  void SelectAll(bool select);
  void InvertSelection();
  void SelectByMask(bool select);
  void SelectSameExtension(bool select);

  // ---- operations (the toolbar / menu commands) ----
  void CopyTo();
  void MoveTo();
  void DeleteItems(bool toRecycleBin);
  void CreateFolderCommand();
  void CreateFileCommand();
  void RenameCommand();
  void SplitFileCommand();
  void CombineFilesCommand();
  void CalculateCrcCommand(const QString &methodName);
  void ShowPropertiesCommand();
  void CompressCommand();
  void ExtractCommand(bool useDialog, bool toSeparateFolders);
  void TestArchiveCommand();
  // 7-Zip's CPanel::OpenItem(): (tryInternal) means "open as an archive",
  // (tryExternal) means "hand it to the desktop". Both are true for a plain
  // double click, which is what makes a .7z open inside and a .txt open in an
  // editor.
  // (arcFormat) is 7-Zip's open-type string: "" = by extension and
  // signature, "*" = try every format, "#" = parser mode (scan for embedded
  // archives). It is what the "Open Inside *" and "Open Inside #" commands pass.
  void OpenItem(int itemIndex, bool tryInternal, bool tryExternal,
                const QString &arcFormat = QString());
  void OpenItemInside(int itemIndex, const QString &arcFormat = QString());
  void OpenItemOutside(int itemIndex);  // Shift+Enter
  void ViewItem(int itemIndex, bool useEditor);   // F3 / F4

  void SetFlatMode(bool flat);
  void SetShowParentItem(bool show);
  // Re-reads the Options dialog settings and applies them to this panel.
  void ApplySettings();

  QString StatusText() const;

signals:
  void FolderChanged();
  void SelectionChanged();
  void PanelActivated();
  // The frame owns the command actions, so it builds the context menu too,
  // exactly as CPanel::CreateFileMenu() reuses the File menu template.
  void ContextMenuRequested(const QPoint &globalPos);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void OnItemActivated(const QModelIndex &index);
  void OnHeaderClicked(int section);
  void OnHeaderContextMenu(const QPoint &pos);
  void OnAddressActivated();
  void OnUpClicked();
  void OnContextMenu(const QPoint &pos);
  void OnSelectionChanged();

private:
  void BuildUi();
  void UpdateAddressBar();
  void RestoreColumnWidths();
  // Opens the item as an archive. In a file-system folder that opens the file
  // by path; inside an archive it asks the current folder for a sub-stream
  // (IInArchiveGetStream) so that nested archives do not have to be unpacked.
  // Index of the item with this name in the current folder, -1 if absent.
  int FindItemIndexByName(const QString &name) const;
  HRESULT OpenAsArc(int itemIndex, const QString &arcFormat);
  // Unpacks one item into a temporary folder and returns its path.
  HRESULT ExtractToTemp(int itemIndex, QString &resultPath);
  HRESULT CloseOneLevel();
  // One level up, without touching the list cursor. OpenParentFolder() is
  // this plus restoring the cursor on the folder we came from.
  HRESULT GoUpOneLevel();
  // Opens (parts) one after another below the current folder, entering
  // archives where a component is not a sub-folder.
  HRESULT DescendPath(const QStringList &parts);
  void PushHistory(const QString &path);
  // Copies or moves the operated items to (destPath).
  void CopyToDialogAndRun(bool moveMode);

  CPanelModel *_model;
  QTreeView *_view;
  QComboBox *_address;
  QToolButton *_upButton;

  QVector<CFolderLink> _parentFolders;
  // Temporary folders for items opened out of an archive; they live as long
  // as the panel does, like 7-Zip's temp dirs live until the program exits.
  QVector<QSharedPointer<QTemporaryDir> > _tempDirs;
  QStringList _history;
  int _historyPos;
  bool _inHistoryNavigation;
};

#endif
