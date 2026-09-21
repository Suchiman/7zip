// Qt/PanelModel.h
//
// The Qt item model over one IFolderFolder. This is the data half of
// UI/FileManager/Panel*.cpp: it owns the column set, the sort order and the
// ".." entry, exactly as CPanel does for its Win32 list view.

#ifndef ZIP7_INC_QT_PANEL_MODEL_H
#define ZIP7_INC_QT_PANEL_MODEL_H

#include "../../../Common/MyCom.h"
#include "../../Archive/IArchive.h"
#include "../FileManager/IFolder.h"

#include "Z7Qt.h"

#include <QAbstractTableModel>
#include <QVector>
#include <QIcon>

struct CPanelColumn
{
  PROPID PropID;
  VARTYPE VarType;
  QString Name;
  bool IsRawProp;
  bool Visible;
  unsigned Width;      // 7-Zip's default width, in pixels at 96 dpi

  CPanelColumn(): PropID(0), VarType(VT_EMPTY), IsRawProp(false),
      Visible(true), Width(100) {}
};


class CPanelModel: public QAbstractTableModel
{
  Q_OBJECT
public:
  explicit CPanelModel(QObject *parent = nullptr);
  ~CPanelModel() override;

  // ---- folder ----
  HRESULT SetFolder(IFolderFolder *folder);
  IFolderFolder *Folder() const { return _folder; }
  IFolderOperations *Operations() const { return _folderOperations; }
  bool IsFsFolder() const { return _isFsFolder; }
  bool IsArchiveFolder() const { return _isArcFolder; }
  // The folder's own path, as the address bar shows it.
  QString FolderPath() const;
  QString FolderType() const;

  HRESULT Reload();

  // ---- columns ----
  const QVector<CPanelColumn> &Columns() const { return _columns; }
  int ColumnForPropID(PROPID propID) const;
  void SetColumnVisible(int column, bool visible);

  // ---- rows ----
  // A row is either the ".." entry (item index -1) or a folder item.
  int ItemIndex(int row) const;
  int RowForItem(int itemIndex) const;
  bool IsParentRow(int row) const { return _showParentItem && row == 0; }
  bool IsItemFolder(int itemIndex) const;
  bool IsItemAltStream(int itemIndex) const;
  UInt64 ItemSize(int itemIndex) const;
  UString ItemName(int itemIndex) const;
  UString ItemPrefix(int itemIndex) const;
  UString ItemRelPath(int itemIndex) const;   // prefix + name
  NWindows::NCOM::CPropVariant ItemProp(int itemIndex, PROPID propID) const;

  void SetShowParentItem(bool show);
  bool ShowParentItem() const { return _showParentItem; }
  void SetFlatMode(bool flat);
  bool FlatMode() const { return _flatMode; }

  // ---- sorting, 7-Zip semantics ----
  void SortByPropID(PROPID propID);
  void SetSort(PROPID propID, bool ascending);
  PROPID SortPropID() const { return _sortID; }
  bool SortAscending() const { return _ascending; }

  // ---- QAbstractTableModel ----
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QVariant headerData(int section, Qt::Orientation o, int role) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
  void LoadColumns();
  void BuildOrder();
  int CompareItems(int i1, int i2) const;
  int CompareItems2(int i1, int i2, PROPID propID, bool isRawProp) const;
  QIcon IconForItem(int itemIndex) const;

  CMyComPtr<IFolderFolder> _folder;
  CMyComPtr<IFolderCompare> _folderCompare;
  CMyComPtr<IFolderGetItemName> _folderGetItemName;
  CMyComPtr<IFolderOperations> _folderOperations;
  CMyComPtr<IFolderSetFlatMode> _folderSetFlatMode;
  CMyComPtr<IFolderCalcItemFullSize> _folderCalcItemFullSize;
  CMyComPtr<IArchiveGetRawProps> _folderRawProps;

  QVector<CPanelColumn> _columns;
  QVector<int> _order;          // row (without ".." ) -> folder item index
  mutable QVector<QIcon> _iconCache;

  PROPID _sortID;
  bool _ascending;
  bool _isRawSortProp;
  bool _showParentItem;
  bool _flatMode;
  bool _isFsFolder;
  bool _isArcFolder;
  UInt32 _numItems;
};

#endif
