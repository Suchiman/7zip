// Qt/Panel.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/StringConvert.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"

#include "../Agent/Agent.h"
#include "../Common/ArchiveName.h"

#include "Panel.h"
#include "Callbacks.h"
#include "Codecs.h"
#include "CompressDialog.h"
#include "ExtractDialog.h"
#include "FsFolder.h"
#include "IconUtils.h"
#include "Operations.h"
#include "ProgressDialog.h"
#include "RootFolder.h"
#include "OptionsDialog.h"
#include "SmallDialogs.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

using namespace NWindows;
using namespace NWindows::NFile;

CPanel::CPanel(QWidget *parent):
    QWidget(parent),
    _historyPos(-1),
    _inHistoryNavigation(false)
{
  BuildUi();
}

CPanel::~CPanel() {}


void CPanel::BuildUi()
{
  _model = new CPanelModel(this);

  _address = new QComboBox;
  _address->setEditable(true);
  _address->setInsertPolicy(QComboBox::NoInsert);
  _address->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  _upButton = new QToolButton;
  _upButton->setIcon(GetCommandIcon(QStringLiteral("Up")));
  _upButton->setToolTip(tr("Up one level"));
  _upButton->setAutoRaise(true);

  QHBoxLayout *addressRow = new QHBoxLayout;
  addressRow->setContentsMargins(0, 0, 0, 0);
  addressRow->addWidget(_upButton);
  addressRow->addWidget(_address, 1);

  _view = new QTreeView;
  _view->setModel(_model);
  _view->setRootIsDecorated(false);
  _view->setUniformRowHeights(true);
  _view->setAlternatingRowColors(true);
  _view->setSelectionBehavior(QAbstractItemView::SelectRows);
  _view->setSelectionMode(QAbstractItemView::ExtendedSelection);
  _view->setSortingEnabled(false);       // we sort through the folder interface
  _view->setContextMenuPolicy(Qt::CustomContextMenu);
  _view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  _view->header()->setSectionsClickable(true);
  _view->header()->setStretchLastSection(false);
  _view->header()->setContextMenuPolicy(Qt::CustomContextMenu);
  _view->installEventFilter(this);
  _view->viewport()->installEventFilter(this);
  // The side buttons should work wherever the pointer is inside the panel.
  installEventFilter(this);
  _address->installEventFilter(this);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->setContentsMargins(0, 0, 0, 0);
  main->addLayout(addressRow);
  main->addWidget(_view, 1);

  connect(_view, &QTreeView::activated, this, &CPanel::OnItemActivated);
  connect(_view->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &CPanel::OnSelectionChanged);
  connect(_view->selectionModel(), &QItemSelectionModel::currentChanged,
          this, &CPanel::OnSelectionChanged);
  connect(_view->header(), &QHeaderView::sectionClicked, this, &CPanel::OnHeaderClicked);
  connect(_view->header(), &QWidget::customContextMenuRequested,
          this, &CPanel::OnHeaderContextMenu);
  connect(_view, &QWidget::customContextMenuRequested, this, &CPanel::OnContextMenu);
  connect(_address->lineEdit(), &QLineEdit::returnPressed, this, &CPanel::OnAddressActivated);
  connect(_address, QOverload<int>::of(&QComboBox::activated), this, &CPanel::OnAddressActivated);
  connect(_upButton, &QToolButton::clicked, this, &CPanel::OnUpClicked);
}


// ---------------------------------------------------------------------------
// paths
// ---------------------------------------------------------------------------

QString CPanel::CurrentFsPath() const
{
  if (!_model->IsFsFolder())
    return QString();
  return _model->FolderPath();
}

QString CPanel::CurrentArchivePath() const
{
  for (int i = _parentFolders.size() - 1; i >= 0; i--)
    if (_parentFolders[i].IsArchive)
      return Us2Q(_parentFolders[i].ParentFolderPath + _parentFolders[i].ItemName);
  return QString();
}

QString CPanel::CurrentPath() const
{
  const QString arc = CurrentArchivePath();
  if (arc.isEmpty())
    return _model->FolderPath();
  // inside an archive: "<archive>/<path inside>"
  QString inside = _model->FolderPath();
  QString s = arc;
  if (!s.endsWith(QLatin1Char('/')))
    s += QLatin1Char('/');
  s += inside;
  return s;
}

void CPanel::UpdateAddressBar()
{
  const QString path = CurrentPath();
  _address->setEditText(path);
  _upButton->setEnabled(true);
}


// ---------------------------------------------------------------------------
// navigation
// ---------------------------------------------------------------------------

HRESULT CPanel::SetFolder(IFolderFolder *folder, bool keepHistory)
{
  const HRESULT res = _model->SetFolder(folder);
  ApplySettings();
  RestoreColumnWidths();
  UpdateAddressBar();
  if (keepHistory && !_inHistoryNavigation)
    PushHistory(CurrentPath());
  if (_model->rowCount() > 0)
    _view->setCurrentIndex(_model->index(0, 0));
  emit FolderChanged();
  emit SelectionChanged();
  return res;
}

void CPanel::PushHistory(const QString &path)
{
  if (_historyPos >= 0 && _historyPos < _history.size() && _history[_historyPos] == path)
    return;
  while (_history.size() > _historyPos + 1)
    _history.removeLast();
  _history.append(path);
  _historyPos = _history.size() - 1;
}

void CPanel::HistoryBack()
{
  if (!CanGoBack())
    return;
  _historyPos--;
  _inHistoryNavigation = true;
  NavigateTo(_history[_historyPos]);
  _inHistoryNavigation = false;
}

void CPanel::HistoryForward()
{
  if (!CanGoForward())
    return;
  _historyPos++;
  _inHistoryNavigation = true;
  NavigateTo(_history[_historyPos]);
  _inHistoryNavigation = false;
}

HRESULT CPanel::OpenRootFolder()
{
  _parentFolders.clear();
  CMyComPtr<IFolderFolder> folder;
  RINOK(CreateRootFolder(&folder))
  return SetFolder(folder);
}

HRESULT CPanel::BindToPathAndRefresh(const QString &path)
{
  COM_TRY_BEGIN
  if (path.isEmpty())
    return OpenRootFolder();

  // Walk down from the longest existing file-system prefix: everything after
  // it must be a path inside an archive, which is how 7-Zip resolves a typed
  // path like "/tmp/a.7z/dir/".
  QString fsPart = QDir::cleanPath(path);
  QStringList insideParts;
  NFile::NFind::CFileInfo fi;
  for (;;)
  {
    QString probe = fsPart;
    while (probe.length() > 1 && probe.endsWith(QLatin1Char('/')))
      probe.chop(1);
    if (probe.isEmpty())
      break;
    if (fi.Find(Q2Fs(probe)))
    {
      fsPart = probe;
      break;
    }
    const int slash = fsPart.lastIndexOf(QLatin1Char('/'));
    if (slash < 0)
      return E_INVALIDARG;
    insideParts.prepend(fsPart.mid(slash + 1));
    fsPart = fsPart.left(slash);
    if (fsPart.isEmpty())
      fsPart = QStringLiteral("/");
  }
  if (!fi.Find(Q2Fs(fsPart)))
    return E_INVALIDARG;

  _parentFolders.clear();

  if (fi.IsDir())
  {
    CMyComPtr<IFolderFolder> folder;
    RINOK(NQtFsFolder::CreateFsFolder(Q2Fs(fsPart), &folder))
    RINOK(SetFolder(folder))
  }
  else
  {
    // The path points at a file: open its folder, then the file as archive.
    const QFileInfo qfi(fsPart);
    CMyComPtr<IFolderFolder> folder;
    RINOK(NQtFsFolder::CreateFsFolder(Q2Fs(qfi.absolutePath()), &folder))
    RINOK(SetFolder(folder))
    const int index = FindItemIndexByName(qfi.fileName());
    if (index < 0)
      return E_INVALIDARG;
    RINOK(OpenAsArc(index, QString()))
  }

  // Descend into the remaining path components. A component is normally a
  // sub-folder, but it can also be another archive -- "a.7z/inner.zip/x" is a
  // valid path -- so a failed BindToFolder falls back to opening the item as
  // an archive.
  for (const QString &part : insideParts)
  {
    if (part.isEmpty())
      continue;
    CMyComPtr<IFolderFolder> sub;
    if (_model->Folder()->BindToFolder(Q2Us(part).Ptr(), &sub) == S_OK && sub)
    {
      RINOK(SetFolder(sub))
      continue;
    }
    const int index = FindItemIndexByName(part);
    if (index < 0)
      break;
    if (OpenAsArc(index, QString()) != S_OK)
      break;
  }
  return S_OK;
  COM_TRY_END
}


int CPanel::FindItemIndexByName(const QString &name) const
{
  if (!_model->Folder())
    return -1;
  UInt32 num = 0;
  _model->Folder()->GetNumberOfItems(&num);
  for (UInt32 i = 0; i < num; i++)
    if (Us2Q(_model->ItemName((int)i)) == name)
      return (int)i;
  return -1;
}


HRESULT CPanel::OpenAsArc(int itemIndex, const QString &arcFormat)
{
  COM_TRY_BEGIN
  const QString name = Us2Q(_model->ItemName(itemIndex));

  // Inside an archive 7-Zip first asks the folder for a sub-stream, which is
  // what lets it walk into a .tar inside a .tar.gz without touching the disk.
  // Not every handler can produce one -- a solid 7z block has to be decoded --
  // so the fallback is to unpack the item into a temporary folder and open
  // that, which is what CPanel::OpenItemInArchive() does as well.
  CMyComPtr<IInStream> subStream;
  QString tempPath;
  if (!_model->IsFsFolder())
  {
    CMyComPtr<IInArchiveGetStream> getStream;
    _model->Folder()->QueryInterface(IID_IInArchiveGetStream, (void **)&getStream);
    if (getStream)
    {
      CMyComPtr<ISequentialInStream> seqStream;
      getStream->GetStream((UInt32)itemIndex, &seqStream);
      if (seqStream)
        seqStream.QueryInterface(IID_IInStream, &subStream);
    }
    if (!subStream)
    {
      const HRESULT res = ExtractToTemp(itemIndex, tempPath);
      if (res != S_OK || tempPath.isEmpty())
        return res == S_OK ? S_FALSE : res;
    }
  }

  QString fullPath;
  if (_model->IsFsFolder())
    fullPath = _model->FolderPath() + name;
  else if (!tempPath.isEmpty())
    fullPath = tempPath;
  else
    fullPath = CurrentPath() + name;

  CMyComPtr<IFolderManager> folderManager = new CArchiveFolderManager;

  CProgressDialog dialog(this);
  dialog.SetTitleFileName(name);
  dialog.WaitMode = false;

  CExtractCallbackQt extractUi;
  extractUi.ProgressDialog = &dialog;
  extractUi.Init();

  COpenCallbackImp *openCallbackSpec = new COpenCallbackImp;
  CMyComPtr<IProgress> openCallback = openCallbackSpec;
  openCallbackSpec->Callback = &extractUi;
  if (_model->IsFsFolder())
    openCallbackSpec->Init2(Q2Fs(_model->FolderPath()), Q2Fs(name));
  else if (!tempPath.isEmpty())
    openCallbackSpec->Init2(Q2Fs(QFileInfo(tempPath).absolutePath()), Q2Fs(QFileInfo(tempPath).fileName()));

  CMyComPtr<IFolderFolder> newFolder;
  const UString path = Q2Us(fullPath);
  const UString type = Q2Us(arcFormat);
  IInStream *inStreamPtr = subStream;

  const HRESULT res = dialog.Execute([&]() -> HRESULT
  {
    // (arcFormat) must be an empty string, not NULL: CAgent::Open() builds a
    // UString from it without a null check.
    return folderManager->OpenFolderFile(inStreamPtr, path.Ptr(), type.Ptr(),
        &newFolder, openCallback);
  });

  if (res != S_OK || !newFolder)
    return res == S_OK ? S_FALSE : res;

  CFolderLink link;
  link.ParentFolder = _model->Folder();
  link.ItemName = Q2Us(name);
  link.ParentFolderPath = Q2Us(_model->IsFsFolder() ? _model->FolderPath() : CurrentPath());
  link.IsArchive = true;
  _parentFolders.append(link);

  return SetFolder(newFolder);
  COM_TRY_END
}


HRESULT CPanel::ExtractToTemp(int itemIndex, QString &resultPath)
{
  COM_TRY_BEGIN
  resultPath.clear();
  CMyComPtr<IFolderOperations> operations = _model->Operations();
  if (!operations)
    return E_NOTIMPL;

  QSharedPointer<QTemporaryDir> tempDir(new QTemporaryDir(
      QDir::tempPath() + QStringLiteral("/7zQ-XXXXXX")));
  if (!tempDir->isValid())
    return E_FAIL;

  const QString name = Us2Q(_model->ItemName(itemIndex));
  const UString destPath = Q2Us(tempDir->path());

  CProgressDialog dialog(this);
  dialog.WaitMode = false;
  dialog.SetTitleFileName(name);

  CExtractCallbackQt *callbackSpec = new CExtractCallbackQt;
  CMyComPtr<IFolderOperationsExtractCallback> callback = callbackSpec;
  callbackSpec->ProgressDialog = &dialog;
  callbackSpec->Init();

  const UInt32 index = (UInt32)itemIndex;
  const HRESULT res = dialog.Execute([&]() -> HRESULT
  {
    return operations->CopyTo(BoolToInt(false), &index, 1,
        BoolToInt(false), 0, destPath.Ptr(), callback);
  });
  if (res != S_OK)
    return res;

  const QString full = tempDir->path() + QLatin1Char('/') + name;
  if (!QFileInfo::exists(full))
    return E_FAIL;

  // The copy is read-only, because changes to it are not written back into the
  // archive. Only a plain file is marked: making the directory itself
  // read-only would stop QTemporaryDir from cleaning up afterwards.
  if (QFileInfo(full).isFile())
    QFile::setPermissions(full, QFileDevice::ReadOwner | QFileDevice::ReadGroup);
  _tempDirs.append(tempDir);
  resultPath = full;
  return S_OK;
  COM_TRY_END
}


HRESULT CPanel::CloseOneLevel()
{
  if (_parentFolders.isEmpty())
    return S_FALSE;
  const CFolderLink link = _parentFolders.takeLast();
  CMyComPtr<IFolderFolder> parent = link.ParentFolder;
  return SetFolder(parent);
}


HRESULT CPanel::GoUpOneLevel()
{
  COM_TRY_BEGIN
  if (!_model->Folder())
    return S_FALSE;
  CMyComPtr<IFolderFolder> parent;
  const HRESULT res = _model->Folder()->BindToParentFolder(&parent);
  if (res == S_OK && parent)
    return SetFolder(parent);
  // No parent inside the current folder tree: leave the archive, if any.
  if (CloseOneLevel() == S_OK)
    return S_OK;
  return OpenRootFolder();
  COM_TRY_END
}


HRESULT CPanel::DescendPath(const QStringList &parts)
{
  COM_TRY_BEGIN
  for (const QString &part : parts)
  {
    if (part.isEmpty())
      continue;
    CMyComPtr<IFolderFolder> sub;
    if (_model->Folder()->BindToFolder(Q2Us(part).Ptr(), &sub) == S_OK && sub)
    {
      RINOK(SetFolder(sub))
      continue;
    }
    // Not a sub-folder: it can still be an archive, as in "a.7z/inner.zip".
    const int index = FindItemIndexByName(part);
    if (index < 0)
      return S_FALSE;
    RINOK(OpenAsArc(index, QString()))
  }
  return S_OK;
  COM_TRY_END
}


// Reuses the open folder chain wherever it already matches (path).
HRESULT CPanel::NavigateTo(const QString &path)
{
  COM_TRY_BEGIN
  if (path.isEmpty())
    return OpenRootFolder();

  QString target = path;
  if (!target.endsWith(QLatin1Char('/')))
    target += QLatin1Char('/');

  if (CurrentPath() == target)
    return S_OK;

  // (1) The target is above us: closing levels is free, it is what ".." does.
  if (CurrentPath().startsWith(target))
  {
    for (;;)
    {
      const QString before = CurrentPath();
      if (before == target || !before.startsWith(target))
        break;
      RINOK(GoUpOneLevel())
      if (CurrentPath() == before)
        break;                       // cannot go any higher
    }
    if (CurrentPath() == target)
      return S_OK;
  }

  // (2) The target is below us: open only the components we are missing,
  //     keeping every archive on the way already open.
  {
    const QString cur = CurrentPath();
    if (target.startsWith(cur))
    {
      const QStringList parts =
          target.mid(cur.length()).split(QLatin1Char('/'), Qt::SkipEmptyParts);
      if (DescendPath(parts) == S_OK && CurrentPath() == target)
        return S_OK;
    }
  }

  // (3) Somewhere else entirely: resolve it from the file system.
  return BindToPathAndRefresh(path);
  COM_TRY_END
}


HRESULT CPanel::OpenParentFolder()
{
  COM_TRY_BEGIN
  if (!_model->Folder())
    return S_FALSE;

  const UString curName = Q2Us(QFileInfo(_model->FolderPath()).fileName());

  RINOK(GoUpOneLevel())

  // Put the cursor back on the folder we came from, as 7-Zip does.
  if (!curName.IsEmpty())
  {
    UInt32 num = 0;
    _model->Folder()->GetNumberOfItems(&num);
    for (UInt32 i = 0; i < num; i++)
      if (_model->ItemName((int)i) == curName)
      {
        const int row = _model->RowForItem((int)i);
        if (row >= 0)
          _view->setCurrentIndex(_model->index(row, 0));
        break;
      }
  }
  return S_OK;
  COM_TRY_END
}


void CPanel::RefreshListing()
{
  const int itemIndex = FocusedItemIndex();
  _model->Reload();
  if (itemIndex >= 0)
  {
    const int row = _model->RowForItem(itemIndex);
    if (row >= 0)
      _view->setCurrentIndex(_model->index(row, 0));
  }
  emit SelectionChanged();
}


void CPanel::RestoreColumnWidths()
{
  QHeaderView *h = _view->header();
  // 7-Zip's widths are pixels at 96 dpi; scale them with the current font so
  // the columns stay readable on a high-dpi screen.
  const double scale = qMax(1.0, fontMetrics().height() / 16.0);
  for (int i = 0; i < _model->columnCount(); i++)
  {
    const CPanelColumn &c = _model->Columns()[i];
    h->setSectionHidden(i, !c.Visible);
    h->resizeSection(i, qRound(c.Width * scale));
  }
}


// Right-clicking the header offers the full property list, which is how a
// hidden column is brought back.
void CPanel::OnHeaderContextMenu(const QPoint &pos)
{
  QMenu menu(this);
  const QVector<CPanelColumn> &columns = _model->Columns();
  for (int i = 0; i < columns.size(); i++)
  {
    QAction *a = menu.addAction(columns[i].Name);
    a->setCheckable(true);
    a->setChecked(columns[i].Visible);
    a->setData(i);
    if (columns[i].PropID == kpidName)
      a->setEnabled(false);      // the name column always stays
  }
  QAction *chosen = menu.exec(_view->header()->mapToGlobal(pos));
  if (!chosen)
    return;
  const int column = chosen->data().toInt();
  _model->SetColumnVisible(column, chosen->isChecked());
  _view->header()->setSectionHidden(column, !chosen->isChecked());
}


// ---------------------------------------------------------------------------
// opening items
// ---------------------------------------------------------------------------

void CPanel::OpenSelectedItem()
{
  const int row = _view->currentIndex().isValid() ? _view->currentIndex().row() : -1;
  if (row < 0)
    return;
  OnItemActivated(_model->index(row, 0));
}

void CPanel::OnItemActivated(const QModelIndex &index)
{
  if (!index.isValid())
    return;
  if (_model->IsParentRow(index.row()))
  {
    OpenParentFolder();
    return;
  }
  const int itemIndex = _model->ItemIndex(index.row());
  if (itemIndex < 0)
    return;
  OpenItem(itemIndex, true, true);   // tryInternal, tryExternal
}


// Verbatim from UI/FileManager/PanelItemOpen.cpp: extensions that a double
// click always hands to the system instead of trying to open as an archive.
static const char * const kStartExtensions =
  " exe bat ps1 com lnk"
  " chm"
  " msi doc dot xls ppt pps wps wpt wks xlr wdb vsd pub"
  " docx docm dotx dotm xlsx xlsm xltx xltm xlsb xps"
  " xlam pptx pptm potx potm ppam ppsx ppsm vsdx xsn"
  " mpp"
  " msg"
  " dwf"
  " flv swf"
  " odt ods"
  " wb3"
  " pdf"
  " ";

static bool DoItemAlwaysStart(const QString &name)
{
  const int dot = name.lastIndexOf(QLatin1Char('.'));
  if (dot < 0)
    return false;
  const QString ext = QStringLiteral(" ") + name.mid(dot + 1).toLower() + QStringLiteral(" ");
  return QString::fromLatin1(kStartExtensions).contains(ext);
}


void CPanel::OpenItem(int itemIndex, bool tryInternal, bool tryExternal,
    const QString &arcFormat)
{
  if (itemIndex < 0)
    return;

  // A folder is always just entered. Without this, "Open Inside" on a folder
  // inside an archive would fall through to the unpack-to-a-temp-folder path
  // and extract the whole subtree.
  if (_model->IsItemFolder(itemIndex))
  {
    CMyComPtr<IFolderFolder> sub;
    if (_model->Folder()->BindToFolder((UInt32)itemIndex, &sub) == S_OK && sub)
      SetFolder(sub);
    return;
  }

  const QString name = Us2Q(_model->ItemName(itemIndex));

  if (tryInternal)
    if (!tryExternal || !DoItemAlwaysStart(name))
    {
      const HRESULT res = OpenAsArc(itemIndex, arcFormat);
      if (res == S_OK || res == E_ABORT)
        return;
      if (res != S_FALSE)
      {
        QMessageBox::warning(this, tr("7-Zip"), HResultToMessage(res));
        return;
      }
      // S_FALSE: not an archive, fall through to the external open
    }

  if (!tryExternal)
  {
    QMessageBox::warning(this, tr("7-Zip"),
        tr("Cannot open the file as an archive:\n%1").arg(name));
    return;
  }

  QString fullPath;
  if (_model->IsFsFolder())
    fullPath = _model->FolderPath() + name;
  else
  {
    // Inside an archive the item has to be unpacked before anything else can
    // read it; 7-Zip unpacks it into a temporary folder in the same way.
    const HRESULT res = ExtractToTemp(itemIndex, fullPath);
    if (res == E_ABORT)
      return;
    if (res != S_OK || fullPath.isEmpty())
    {
      QMessageBox::warning(this, tr("7-Zip"), HResultToMessage(res));
      return;
    }
  }
  QDesktopServices::openUrl(QUrl::fromLocalFile(fullPath));
}


void CPanel::OpenItemInside(int itemIndex, const QString &arcFormat)
{
  OpenItem(itemIndex, true, false, arcFormat);   // archive only
}

void CPanel::OpenItemOutside(int itemIndex)
{
  OpenItem(itemIndex, false, true);   // desktop only
}


void CPanel::ViewItem(int itemIndex, bool useEditor)
{
  if (itemIndex < 0 || _model->IsItemFolder(itemIndex))
    return;

  QString fullPath;
  if (_model->IsFsFolder())
    fullPath = _model->FolderPath() + Us2Q(_model->ItemRelPath(itemIndex));
  else
  {
    const HRESULT res = ExtractToTemp(itemIndex, fullPath);
    if (res == E_ABORT)
      return;
    if (res != S_OK || fullPath.isEmpty())
    {
      QMessageBox::warning(this, tr("7-Zip"), HResultToMessage(res));
      return;
    }
  }

  const CFmSettings &settings = GlobalFmSettings();
  const QString program = useEditor ? settings.Editor : settings.Viewer;
  if (program.isEmpty())
  {
    QDesktopServices::openUrl(QUrl::fromLocalFile(fullPath));
    return;
  }
  if (!QProcess::startDetached(program, QStringList() << fullPath))
    QMessageBox::warning(this, tr("7-Zip"),
        tr("Cannot start the program:\n%1").arg(program));
}


void CPanel::OnAddressActivated()
{
  const QString path = _address->currentText();
  if (BindToPathAndRefresh(path) != S_OK)
  {
    QMessageBox::warning(this, tr("7-Zip"), tr("The path does not exist:\n%1").arg(path));
    UpdateAddressBar();
  }
}

void CPanel::OnUpClicked()
{
  OpenParentFolder();
}

void CPanel::OnHeaderClicked(int section)
{
  if (section < 0 || section >= _model->Columns().size())
    return;
  _model->SortByPropID(_model->Columns()[section].PropID);
  _view->header()->setSortIndicatorShown(true);
  _view->header()->setSortIndicator(section,
      _model->SortAscending() ? Qt::AscendingOrder : Qt::DescendingOrder);
}

void CPanel::OnSelectionChanged()
{
  emit SelectionChanged();
}


// ---------------------------------------------------------------------------
// selection
// ---------------------------------------------------------------------------

QVector<int> CPanel::SelectedItemIndices() const
{
  QVector<int> res;
  const QModelIndexList rows = _view->selectionModel()->selectedRows();
  for (const QModelIndex &idx : rows)
  {
    const int itemIndex = _model->ItemIndex(idx.row());
    if (itemIndex >= 0)
      res.append(itemIndex);
  }
  return res;
}

int CPanel::FocusedItemIndex() const
{
  const QModelIndex idx = _view->currentIndex();
  if (!idx.isValid())
    return -1;
  return _model->ItemIndex(idx.row());
}

QVector<int> CPanel::OperatedItemIndices() const
{
  QVector<int> res = SelectedItemIndices();
  if (res.isEmpty())
  {
    const int f = FocusedItemIndex();
    if (f >= 0)
      res.append(f);
  }
  return res;
}

void CPanel::SelectAll(bool select)
{
  if (select)
  {
    _view->selectAll();
    if (_model->ShowParentItem() && _model->rowCount() > 0)
      _view->selectionModel()->select(_model->index(0, 0),
          QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
  }
  else
    _view->clearSelection();
  emit SelectionChanged();
}

void CPanel::InvertSelection()
{
  QItemSelection sel;
  for (int row = _model->ShowParentItem() ? 1 : 0; row < _model->rowCount(); row++)
  {
    if (!_view->selectionModel()->isSelected(_model->index(row, 0)))
      sel.select(_model->index(row, 0), _model->index(row, _model->columnCount() - 1));
  }
  _view->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
  emit SelectionChanged();
}

void CPanel::SelectByMask(bool select)
{
  bool ok = false;
  const QString mask = QInputDialog::getText(this,
      select ? tr("Select") : tr("Deselect"), tr("Mask:"),
      QLineEdit::Normal, QStringLiteral("*"), &ok);
  if (!ok)
    return;
  const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(mask));
  QItemSelection sel;
  for (int row = _model->ShowParentItem() ? 1 : 0; row < _model->rowCount(); row++)
  {
    const int itemIndex = _model->ItemIndex(row);
    if (itemIndex < 0)
      continue;
    if (re.match(Us2Q(_model->ItemName(itemIndex))).hasMatch())
      sel.select(_model->index(row, 0), _model->index(row, _model->columnCount() - 1));
  }
  _view->selectionModel()->select(sel,
      select ? QItemSelectionModel::Select : QItemSelectionModel::Deselect);
  emit SelectionChanged();
}

void CPanel::SelectSameExtension(bool select)
{
  const int focused = FocusedItemIndex();
  if (focused < 0)
    return;
  const QString name = Us2Q(_model->ItemName(focused));
  const int dot = name.lastIndexOf(QLatin1Char('.'));
  const QString ext = dot < 0 ? QString() : name.mid(dot);

  QItemSelection sel;
  for (int row = _model->ShowParentItem() ? 1 : 0; row < _model->rowCount(); row++)
  {
    const int itemIndex = _model->ItemIndex(row);
    if (itemIndex < 0)
      continue;
    const QString n = Us2Q(_model->ItemName(itemIndex));
    const int d = n.lastIndexOf(QLatin1Char('.'));
    const QString e = d < 0 ? QString() : n.mid(d);
    if (e.compare(ext, Qt::CaseInsensitive) == 0)
      sel.select(_model->index(row, 0), _model->index(row, _model->columnCount() - 1));
  }
  _view->selectionModel()->select(sel,
      select ? QItemSelectionModel::Select : QItemSelectionModel::Deselect);
  emit SelectionChanged();
}


void CPanel::SetFlatMode(bool flat)
{
  _model->SetFlatMode(flat);
  RestoreColumnWidths();
}

void CPanel::SetShowParentItem(bool show)
{
  _model->SetShowParentItem(show);
}

void CPanel::ApplySettings()
{
  const CFmSettings &s = GlobalFmSettings();
  _model->SetShowParentItem(s.ShowDots);
  _view->setAllColumnsShowFocus(s.FullRow);
  _view->setSelectionBehavior(QAbstractItemView::SelectRows);
  _view->setAlternatingRowColors(!s.ShowGrid);
  _view->setStyleSheet(s.ShowGrid
      ? QStringLiteral("QTreeView::item { border-right: 1px solid palette(mid); }")
      : QString());
  _view->setSelectionMode(s.AlternativeSelection
      ? QAbstractItemView::MultiSelection
      : QAbstractItemView::ExtendedSelection);
  _view->setExpandsOnDoubleClick(false);
  // "Single-click to open an item" is the same option Dolphin/Explorer have.
  disconnect(_view, &QTreeView::clicked, this, &CPanel::OnItemActivated);
  if (s.SingleClick)
    connect(_view, &QTreeView::clicked, this, &CPanel::OnItemActivated);
}


QString CPanel::StatusText() const
{
  const QVector<int> sel = SelectedItemIndices();
  UInt64 totalSize = 0;
  for (int i : sel)
    totalSize += _model->ItemSize(i);

  int numItems = _model->rowCount() - (_model->ShowParentItem() ? 1 : 0);
  if (sel.isEmpty())
  {
    const int focused = FocusedItemIndex();
    QString s = tr("%n object(s)", "", numItems);
    if (focused >= 0 && !_model->IsItemFolder(focused))
      s += QStringLiteral("    ") + NumberToStringGrouped(_model->ItemSize(focused));
    return s;
  }
  return tr("%1 / %2 object(s) selected    %3")
      .arg(sel.size()).arg(numItems).arg(NumberToStringGrouped(totalSize));
}


// ---------------------------------------------------------------------------
// keyboard
// ---------------------------------------------------------------------------

bool CPanel::eventFilter(QObject *obj, QEvent *event)
{
  if (event->type() == QEvent::FocusIn)
    emit PanelActivated();

  // The side buttons of a mouse navigate the folder history, as they do in
  // every other file browser. 7-Zip itself has no Back/Forward at all, so
  // there is no original behaviour to copy here.
  switch (event->type())
  {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    {
      const QMouseEvent *me = static_cast<QMouseEvent *>(event);
      const Qt::MouseButton button = me->button();
      if (button != Qt::BackButton && button != Qt::ForwardButton)
        break;
      emit PanelActivated();
      // Act once, on the press, and swallow the rest so that the list does
      // not see a stray click.
      if (event->type() == QEvent::MouseButtonPress)
      {
        if (button == Qt::BackButton)
          HistoryBack();
        else
          HistoryForward();
      }
      return true;
    }
    default:
      break;
  }

  if (obj == _view && event->type() == QEvent::KeyPress)
  {
    QKeyEvent *ke = static_cast<QKeyEvent *>(event);
    switch (ke->key())
    {
      case Qt::Key_Backspace:
        OpenParentFolder();
        return true;
      case Qt::Key_Return:
      case Qt::Key_Enter:
        OpenSelectedItem();
        return true;
      case Qt::Key_Space:
      {
        const QModelIndex idx = _view->currentIndex();
        if (idx.isValid() && !_model->IsParentRow(idx.row()))
        {
          _view->selectionModel()->select(idx,
              QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
          emit SelectionChanged();
        }
        return true;
      }
      case Qt::Key_Insert:
      {
        const QModelIndex idx = _view->currentIndex();
        if (idx.isValid() && !_model->IsParentRow(idx.row()))
        {
          _view->selectionModel()->select(idx,
              QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
          if (idx.row() + 1 < _model->rowCount())
            _view->setCurrentIndex(_model->index(idx.row() + 1, 0));
          emit SelectionChanged();
        }
        return true;
      }
      default:
        break;
    }
  }
  return QWidget::eventFilter(obj, event);
}


// ---------------------------------------------------------------------------
// context menu
// ---------------------------------------------------------------------------

void CPanel::OnContextMenu(const QPoint &pos)
{
  // 7-Zip's CPanel::CreateFileMenu() fills the context menu from the same
  // File menu template the menu bar uses, so the frame -- which owns the
  // command actions -- builds it.
  emit PanelActivated();
  emit ContextMenuRequested(_view->viewport()->mapToGlobal(pos));
}


// ---------------------------------------------------------------------------
// operations
// ---------------------------------------------------------------------------

void CPanel::CopyToDialogAndRun(bool moveMode)
{
  const QVector<int> indices = OperatedItemIndices();
  if (indices.isEmpty())
    return;
  IFolderOperations *ops = _model->Operations();

  UInt64 totalSize = 0;
  for (int i : indices)
    totalSize += _model->ItemSize(i);

  QString info = tr("%n file(s)", "", indices.size());
  info += QStringLiteral("    ") + NumberToStringGrouped(totalSize) + tr(" bytes");

  CCopyDialog dlg(this,
      moveMode ? tr("Move") : tr("Copy"),
      moveMode ? tr("Move to:") : tr("Copy to:"),
      CurrentFsPath().isEmpty() ? QDir::homePath() : CurrentFsPath(),
      info);
  if (dlg.exec() != QDialog::Accepted)
    return;

  QString dest = dlg.Value();
  if (dest.isEmpty())
    return;
  if (!dest.endsWith(QLatin1Char('/')))
    dest += QLatin1Char('/');

  if (moveMode && !ops)
  {
    QMessageBox::warning(this, tr("7-Zip"), tr("This folder does not support moving."));
    return;
  }

  // The archive folder implements CopyTo as "extract these items".
  CMyComPtr<IFolderOperations> operations = ops;
  if (!operations)
  {
    QMessageBox::warning(this, tr("7-Zip"), tr("This operation is not supported."));
    return;
  }

  CRecordVector<UInt32> realIndices;
  for (int i : indices)
    realIndices.Add((UInt32)i);

  CProgressDialog dialog(this);
  dialog.WaitMode = true;
  dialog.SetTitleFileName(moveMode ? tr("Move") : tr("Copy"));

  CExtractCallbackQt *callbackSpec = new CExtractCallbackQt;
  CMyComPtr<IFolderOperationsExtractCallback> callback = callbackSpec;
  callbackSpec->ProgressDialog = &dialog;
  callbackSpec->Init();

  const UString destPath = Q2Us(dest);
  const HRESULT res = dialog.Execute([&]() -> HRESULT
  {
    return operations->CopyTo(BoolToInt(moveMode),
        realIndices.ConstData(), realIndices.Size(),
        BoolToInt(false), 0, destPath.Ptr(), callback);
  });

  if (res != S_OK && res != E_ABORT)
    QMessageBox::warning(this, tr("7-Zip"), HResultToMessage(res));
  RefreshListing();
}

void CPanel::CopyTo() { CopyToDialogAndRun(false); }
void CPanel::MoveTo() { CopyToDialogAndRun(true); }


void CPanel::DeleteItems(bool /* toRecycleBin */)
{
  const QVector<int> indices = OperatedItemIndices();
  if (indices.isEmpty())
    return;
  IFolderOperations *ops = _model->Operations();
  if (!ops)
  {
    QMessageBox::warning(this, tr("7-Zip"), tr("This operation is not supported."));
    return;
  }

  QString question;
  if (indices.size() == 1)
  {
    const int i = indices[0];
    question = _model->IsItemFolder(i)
        ? tr("Are you sure you want to delete the folder \"%1\"?")
        : tr("Are you sure you want to delete the file \"%1\"?");
    question = question.arg(Us2Q(_model->ItemName(i)));
  }
  else
    question = tr("Are you sure you want to delete %n item(s)?", "", indices.size());

  if (QMessageBox::question(this, tr("Confirm File Delete"), question,
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    return;

  CRecordVector<UInt32> realIndices;
  for (int i : indices)
    realIndices.Add((UInt32)i);

  CMyComPtr<IFolderOperations> operations = ops;
  CProgressDialog dialog(this);
  dialog.WaitMode = true;
  dialog.SetTitleFileName(tr("Deleting"));

  CExtractCallbackQt *callbackSpec = new CExtractCallbackQt;
  CMyComPtr<IFolderOperationsExtractCallback> callbackRef = callbackSpec;
  callbackSpec->ProgressDialog = &dialog;
  callbackSpec->Init();
  IProgress *progress = (IFolderOperationsExtractCallback *)callbackSpec;

  const HRESULT res = dialog.Execute([&]() -> HRESULT
  {
    return operations->Delete(realIndices.ConstData(), realIndices.Size(), progress);
  });

  if (res != S_OK && res != E_ABORT)
    QMessageBox::warning(this, tr("7-Zip"), HResultToMessage(res));
  RefreshListing();
}


void CPanel::CreateFolderCommand()
{
  IFolderOperations *ops = _model->Operations();
  if (!ops)
  {
    QMessageBox::warning(this, tr("7-Zip"), tr("This operation is not supported."));
    return;
  }
  CComboDialog dlg(this, tr("Create Folder"), tr("Folder name:"), tr("New Folder"));
  if (dlg.exec() != QDialog::Accepted || dlg.Value().isEmpty())
    return;
  const UString name = Q2Us(dlg.Value());
  const HRESULT res = ops->CreateFolder(name.Ptr(), NULL);
  if (res != S_OK)
    QMessageBox::warning(this, tr("7-Zip"), HResultToMessage(res));
  RefreshListing();
}

void CPanel::CreateFileCommand()
{
  IFolderOperations *ops = _model->Operations();
  if (!ops)
    return;
  CComboDialog dlg(this, tr("Create File"), tr("File name:"), tr("New File"));
  if (dlg.exec() != QDialog::Accepted || dlg.Value().isEmpty())
    return;
  const UString name = Q2Us(dlg.Value());
  const HRESULT res = ops->CreateFile(name.Ptr(), NULL);
  if (res != S_OK)
    QMessageBox::warning(this, tr("7-Zip"), HResultToMessage(res));
  RefreshListing();
}

void CPanel::RenameCommand()
{
  const int itemIndex = FocusedItemIndex();
  IFolderOperations *ops = _model->Operations();
  if (itemIndex < 0 || !ops)
    return;
  const QString oldName = Us2Q(_model->ItemName(itemIndex));
  CComboDialog dlg(this, tr("Rename"), tr("New file name:"), oldName);
  if (dlg.exec() != QDialog::Accepted || dlg.Value().isEmpty() || dlg.Value() == oldName)
    return;
  const UString newName = Q2Us(dlg.Value());
  const HRESULT res = ops->Rename((UInt32)itemIndex, newName.Ptr(), NULL);
  if (res != S_OK)
    QMessageBox::warning(this, tr("7-Zip"), HResultToMessage(res));
  RefreshListing();
}


void CPanel::CompressCommand()
{
  if (!_model->IsFsFolder())
  {
    QMessageBox::warning(this, tr("7-Zip"),
        tr("Adding to an archive is only possible from the file system."));
    return;
  }
  const QVector<int> indices = OperatedItemIndices();
  if (indices.isEmpty())
    return;

  QStringList paths;
  for (int i : indices)
    paths.append(_model->FolderPath() + Us2Q(_model->ItemRelPath(i)));

  // The default archive name, produced by UI/Common/ArchiveName.cpp.
  UString archiveName;
  {
    UStringVector pathsVec;
    for (const QString &p : paths)
      pathsVec.Add(Q2Us(p));
    NFile::NFind::CFileInfo fi;
    const bool oneItem = (pathsVec.Size() == 1);
    if (oneItem)
      fi.Find(us2fs(pathsVec[0]));
    UString baseName;
    archiveName = CreateArchiveName(pathsVec, false,
        (oneItem && !fi.Name.IsEmpty()) ? &fi : NULL, baseName);
  }

  CCompressDialog dlg(this);
  dlg.SetCurDirPrefix(_model->FolderPath());
  dlg.SetArchiveBaseName(_model->FolderPath() + Us2Q(archiveName));
  if (dlg.exec() != QDialog::Accepted)
    return;

  QString errorMessage;
  const HRESULT res = CompressFiles(this, _model->FolderPath(), paths, dlg.Info(), &errorMessage);
  if (res != S_OK && res != E_ABORT)
    QMessageBox::warning(this, tr("7-Zip"),
        errorMessage.isEmpty() ? HResultToMessage(res) : errorMessage);
  RefreshListing();
}


void CPanel::ExtractCommand(bool useDialog, bool toSeparateFolders)
{
  const QVector<int> indices = OperatedItemIndices();
  if (indices.isEmpty())
    return;

  if (!_model->IsFsFolder())
  {
    // We are inside an archive: extracting means "copy these items out".
    CopyTo();
    return;
  }

  QStringList archives;
  for (int i : indices)
  {
    if (_model->IsItemFolder(i))
      continue;
    archives.append(_model->FolderPath() + Us2Q(_model->ItemName(i)));
  }
  if (archives.isEmpty())
    return;

  CQtExtractInfo info;
  info.OutputDir = _model->FolderPath();
  info.ElimDup = GlobalFmSettings().ElimDup;

  if (useDialog)
  {
    CExtractDialog dlg(this);
    dlg.SetPath(_model->FolderPath());
    dlg.SetArchiveName(QFileInfo(archives.first()).completeBaseName());
    if (dlg.exec() != QDialog::Accepted)
      return;
    info = dlg.Info();
  }
  else if (toSeparateFolders)
  {
    // "Extract to <name>\": each archive into a folder of its own.
    for (const QString &arc : archives)
    {
      CQtExtractInfo one = info;
      one.OutputDir = _model->FolderPath() + QFileInfo(arc).completeBaseName();
      QString errorMessage;
      const HRESULT res = ExtractArchives(this, QStringList() << arc, one, &errorMessage);
      if (res != S_OK && res != E_ABORT)
      {
        QMessageBox::warning(this, tr("7-Zip"),
            errorMessage.isEmpty() ? HResultToMessage(res) : errorMessage);
        break;
      }
    }
    RefreshListing();
    return;
  }

  QString errorMessage;
  const HRESULT res = ExtractArchives(this, archives, info, &errorMessage);
  if (res != S_OK && res != E_ABORT)
    QMessageBox::warning(this, tr("7-Zip"),
        errorMessage.isEmpty() ? HResultToMessage(res) : errorMessage);
  RefreshListing();
}


void CPanel::TestArchiveCommand()
{
  const QVector<int> indices = OperatedItemIndices();
  if (indices.isEmpty())
    return;

  if (!_model->IsFsFolder())
  {
    // Inside an archive 7-Zip tests by extracting to nowhere; the agent's
    // CopyTo with an empty path does exactly that.
    CMyComPtr<IFolderOperations> operations = _model->Operations();
    if (!operations)
      return;
    CRecordVector<UInt32> realIndices;
    for (int i : indices)
      realIndices.Add((UInt32)i);

    CProgressDialog dialog(this);
    dialog.WaitMode = true;
    dialog.SetTitleFileName(tr("Testing"));
    CExtractCallbackQt *callbackSpec = new CExtractCallbackQt;
    CMyComPtr<IFolderOperationsExtractCallback> callback = callbackSpec;
    callbackSpec->ProgressDialog = &dialog;
    callbackSpec->Init();

    const HRESULT res = dialog.Execute([&]() -> HRESULT
    {
      return operations->CopyTo(BoolToInt(false),
          realIndices.ConstData(), realIndices.Size(),
          BoolToInt(false), 0, NULL, callback);
    });
    if (res != S_OK && res != E_ABORT)
      QMessageBox::warning(this, tr("7-Zip"), HResultToMessage(res));
    return;
  }

  QStringList archives;
  for (int i : indices)
    if (!_model->IsItemFolder(i))
      archives.append(_model->FolderPath() + Us2Q(_model->ItemName(i)));
  if (archives.isEmpty())
    return;

  CQtExtractInfo info;
  info.TestMode = true;
  QString errorMessage;
  const HRESULT res = ExtractArchives(this, archives, info, &errorMessage);
  if (res != S_OK && res != E_ABORT)
    QMessageBox::warning(this, tr("7-Zip"),
        errorMessage.isEmpty() ? HResultToMessage(res) : errorMessage);
}


void CPanel::CalculateCrcCommand(const QString &methodName)
{
  if (!_model->IsFsFolder())
  {
    QMessageBox::warning(this, tr("7-Zip"),
        tr("Checksums can only be calculated for files on disk."));
    return;
  }
  const QVector<int> indices = OperatedItemIndices();
  if (indices.isEmpty())
    return;

  QStringList paths;
  for (int i : indices)
    paths.append(_model->FolderPath() + Us2Q(_model->ItemRelPath(i)));

  QVector<CQtHashResult> results;
  QString errorMessage;
  const HRESULT res = CalcChecksum(this, _model->FolderPath(), paths,
      methodName, &results, &errorMessage);
  if (res == E_ABORT)
    return;
  if (res != S_OK)
  {
    QMessageBox::warning(this, tr("7-Zip"),
        errorMessage.isEmpty() ? HResultToMessage(res) : errorMessage);
    return;
  }

  QList<QPair<QString, QString> > rows;
  for (const CQtHashResult &r : results)
    rows.append(qMakePair(r.Name, r.Value));
  CListViewDialog dlg(this, tr("Checksum information"), rows);
  dlg.exec();
}


void CPanel::ShowPropertiesCommand()
{
  const int itemIndex = FocusedItemIndex();
  QList<QPair<QString, QString> > rows;

  if (itemIndex >= 0)
  {
    for (const CPanelColumn &c : _model->Columns())
    {
      const QString value = PropToString(_model->ItemProp(itemIndex, c.PropID), c.PropID);
      if (!value.isEmpty())
        rows.append(qMakePair(c.Name, value));
    }
  }

  // The archive's own properties, when we are inside one.
  CMyComPtr<IGetFolderArcProps> getProps;
  _model->Folder()->QueryInterface(IID_IGetFolderArcProps, (void **)&getProps);
  if (getProps)
  {
    CMyComPtr<IFolderArcProps> arcProps;
    getProps->GetFolderArcProps(&arcProps);
    if (arcProps)
    {
      UInt32 numLevels = 0;
      arcProps->GetArcNumLevels(&numLevels);
      for (UInt32 level = 0; level < numLevels; level++)
      {
        rows.append(qMakePair(QString(), QString()));
        UInt32 numProps = 0;
        arcProps->GetArcNumProps(level, &numProps);
        for (UInt32 i = 0; i < numProps; i++)
        {
          CMyComBSTR name;
          PROPID propID;
          VARTYPE varType;
          if (arcProps->GetArcPropInfo(level, i, &name, &propID, &varType) != S_OK)
            continue;
          NWindows::NCOM::CPropVariant prop;
          if (arcProps->GetArcProp(level, propID, &prop) != S_OK)
            continue;
          const QString value = PropToString(prop, propID);
          if (!value.isEmpty())
            rows.append(qMakePair(GetPropName(propID, (const wchar_t *)name), value));
        }
      }
    }
  }

  CListViewDialog dlg(this, tr("Properties"), rows);
  dlg.exec();
}


void CPanel::SplitFileCommand()
{
  const int itemIndex = FocusedItemIndex();
  if (itemIndex < 0 || !_model->IsFsFolder() || _model->IsItemFolder(itemIndex))
    return;

  CSplitDialog dlg(this, _model->FolderPath());
  if (dlg.exec() != QDialog::Accepted)
    return;
  const quint64 volSize = dlg.VolumeSize();
  if (volSize == 0)
  {
    QMessageBox::warning(this, tr("7-Zip"), tr("Incorrect volume size"));
    return;
  }

  QString dest = dlg.Path();
  if (!dest.endsWith(QLatin1Char(0x2F)))
    dest += QLatin1Char(0x2F);

  const QString name = Us2Q(_model->ItemName(itemIndex));
  QString errorMessage;
  const HRESULT res = SplitFile(this,
      _model->FolderPath() + name, dest + name, volSize, &errorMessage);
  if (res != S_OK && res != E_ABORT)
    QMessageBox::warning(this, tr("7-Zip"),
        errorMessage.isEmpty() ? HResultToMessage(res) : errorMessage);
  RefreshListing();
}


void CPanel::CombineFilesCommand()
{
  const int itemIndex = FocusedItemIndex();
  if (itemIndex < 0 || !_model->IsFsFolder())
    return;
  // A ".001" volume opens as a "split" archive; extracting it rebuilds the
  // original file, which is exactly what 7-Zip's Combine does.
  CQtExtractInfo info;
  info.OutputDir = _model->FolderPath();
  info.PathMode = NExtract::NPathMode::kNoPaths;

  QStringList archives;
  archives.append(_model->FolderPath() + Us2Q(_model->ItemName(itemIndex)));

  QString errorMessage;
  const HRESULT res = ExtractArchives(this, archives, info, &errorMessage);
  if (res != S_OK && res != E_ABORT)
    QMessageBox::warning(this, tr("7-Zip"),
        errorMessage.isEmpty() ? HResultToMessage(res) : errorMessage);
  RefreshListing();
}
