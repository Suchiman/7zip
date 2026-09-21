// Qt/PanelModel.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/Defs.h"
#include "../../../Windows/PropVariant.h"
#include "../../../Windows/PropVariantConv.h"
#include "../../PropID.h"

#include "PanelModel.h"
#include "IconUtils.h"

#include <QCollator>
#include <algorithm>

using namespace NWindows;
using namespace NWindows::NCOM;

// Which columns a fresh panel shows. Ported from GetColumnVisible() in
// UI/FileManager/PanelItems.cpp, with the posix properties this port adds
// folded into the same groups: kpidPosixAttrib is the local spelling of
// kpidAttrib, and kpidSymLink of kpidNtReparse.
static bool GetColumnVisible(PROPID propID, bool isFsFolder)
{
  if (isFsFolder)
  {
    switch (propID)
    {
      case kpidATime:
      case kpidChangeTime:
      case kpidAttrib:
      case kpidPosixAttrib:
      case kpidPackSize:
      case kpidINode:
      case kpidLinks:
      case kpidNtReparse:
      case kpidSymLink:
        return false;
      default: break;
    }
  }
  return true;
}

// GetColumnWidth() from the same file. The values are pixels at the original
// 96 dpi; CPanel scales them by the current font size.
static unsigned GetColumnWidth(PROPID propID)
{
  switch (propID)
  {
    case kpidName: return 160;
    default: break;
  }
  return 100;
}

CPanelModel::CPanelModel(QObject *parent):
    QAbstractTableModel(parent),
    _sortID(kpidName),
    _ascending(true),
    _isRawSortProp(false),
    _showParentItem(true),
    _flatMode(false),
    _isFsFolder(false),
    _isArcFolder(false),
    _numItems(0)
{
}

CPanelModel::~CPanelModel() {}


HRESULT CPanelModel::SetFolder(IFolderFolder *folder)
{
  beginResetModel();

  _folder = folder;
  _folderCompare.Release();
  _folderGetItemName.Release();
  _folderOperations.Release();
  _folderSetFlatMode.Release();
  _folderCalcItemFullSize.Release();
  _folderRawProps.Release();
  _order.clear();
  _iconCache.clear();
  _numItems = 0;

  if (_folder)
  {
    _folder.QueryInterface(IID_IFolderCompare, &_folderCompare);
    _folder.QueryInterface(IID_IFolderGetItemName, &_folderGetItemName);
    _folder.QueryInterface(IID_IFolderOperations, &_folderOperations);
    _folder.QueryInterface(IID_IFolderSetFlatMode, &_folderSetFlatMode);
    _folder.QueryInterface(IID_IFolderCalcItemFullSize, &_folderCalcItemFullSize);
    _folder.QueryInterface(IID_IArchiveGetRawProps, &_folderRawProps);
  }

  const QString type = FolderType();
  _isFsFolder = (type == QLatin1String("FSFolder"));
  _isArcFolder = !type.isEmpty() && !_isFsFolder && type != QLatin1String("RootFolder");

  LoadColumns();
  endResetModel();

  return Reload();
}

QString CPanelModel::FolderType() const
{
  if (!_folder)
    return QString();
  CPropVariant prop;
  if (_folder->GetFolderProperty(kpidType, &prop) != S_OK)
    return QString();
  if (prop.vt != VT_BSTR)
    return QString();
  return QString::fromWCharArray(prop.bstrVal);
}

QString CPanelModel::FolderPath() const
{
  if (!_folder)
    return QString();
  CPropVariant prop;
  if (_folder->GetFolderProperty(kpidPath, &prop) != S_OK)
    return QString();
  if (prop.vt != VT_BSTR)
    return QString();
  return QString::fromWCharArray(prop.bstrVal);
}


void CPanelModel::LoadColumns()
{
  _columns.clear();
  if (!_folder)
    return;

  UInt32 numProps = 0;
  if (_folder->GetNumberOfProperties(&numProps) != S_OK)
    return;

  for (UInt32 i = 0; i < numProps; i++)
  {
    CMyComBSTR name;
    PROPID propID;
    VARTYPE varType;
    if (_folder->GetPropertyInfo(i, &name, &propID, &varType) != S_OK)
      continue;
    if (propID == kpidIsDir)
      continue;
    CPanelColumn c;
    c.PropID = propID;
    c.VarType = varType;
    c.Name = GetPropName(propID, (const wchar_t *)name);
    c.IsRawProp = false;
    c.Visible = GetColumnVisible(propID, _isFsFolder);
    c.Width = GetColumnWidth(propID);
    _columns.append(c);
  }

  // The raw properties (SHA-1, NT security, ...) an archive can expose.
  if (_folderRawProps)
  {
    UInt32 numRaw = 0;
    if (_folderRawProps->GetNumRawProps(&numRaw) == S_OK)
    {
      for (UInt32 i = 0; i < numRaw; i++)
      {
        CMyComBSTR name;
        PROPID propID;
        if (_folderRawProps->GetRawPropInfo(i, &name, &propID) != S_OK)
          continue;
        CPanelColumn c;
        c.PropID = propID;
        c.VarType = VT_BSTR;
        c.Name = GetPropName(propID, (const wchar_t *)name);
        c.IsRawProp = true;
        c.Visible = false;   // off by default, like in 7-Zip
        c.Width = GetColumnWidth(propID);
        _columns.append(c);
      }
    }
  }

  // 7-Zip always keeps Name as the first column.
  for (int i = 1; i < _columns.size(); i++)
    if (_columns[i].PropID == kpidName)
    {
      _columns.move(i, 0);
      break;
    }
}

int CPanelModel::ColumnForPropID(PROPID propID) const
{
  for (int i = 0; i < _columns.size(); i++)
    if (_columns[i].PropID == propID)
      return i;
  return -1;
}

void CPanelModel::SetColumnVisible(int column, bool visible)
{
  if (column >= 0 && column < _columns.size())
    _columns[column].Visible = visible;
}


HRESULT CPanelModel::Reload()
{
  beginResetModel();
  _order.clear();
  _iconCache.clear();
  _numItems = 0;

  HRESULT res = S_OK;
  if (_folder)
  {
    res = _folder->LoadItems();
    if (res == S_OK)
      res = _folder->GetNumberOfItems(&_numItems);
  }
  if (res != S_OK)
    _numItems = 0;

  BuildOrder();
  endResetModel();
  return res;
}


void CPanelModel::SetShowParentItem(bool show)
{
  if (_showParentItem == show)
    return;
  beginResetModel();
  _showParentItem = show;
  endResetModel();
}

void CPanelModel::SetFlatMode(bool flat)
{
  if (_flatMode == flat)
    return;
  _flatMode = flat;
  if (_folderSetFlatMode)
    _folderSetFlatMode->SetFlatMode(BoolToInt(flat));
  Reload();
}


// ---------------------------------------------------------------------------
// item accessors
// ---------------------------------------------------------------------------

int CPanelModel::ItemIndex(int row) const
{
  if (_showParentItem)
  {
    if (row == 0)
      return -1;
    row--;
  }
  if (row < 0 || row >= _order.size())
    return -1;
  return _order[row];
}

int CPanelModel::RowForItem(int itemIndex) const
{
  for (int i = 0; i < _order.size(); i++)
    if (_order[i] == itemIndex)
      return _showParentItem ? i + 1 : i;
  return -1;
}

CPropVariant CPanelModel::ItemProp(int itemIndex, PROPID propID) const
{
  CPropVariant prop;
  if (_folder && itemIndex >= 0)
    _folder->GetProperty((UInt32)itemIndex, propID, &prop);
  return prop;
}

bool CPanelModel::IsItemFolder(int itemIndex) const
{
  const CPropVariant prop = ItemProp(itemIndex, kpidIsDir);
  return prop.vt == VT_BOOL && prop.boolVal != VARIANT_FALSE;
}

bool CPanelModel::IsItemAltStream(int itemIndex) const
{
  const CPropVariant prop = ItemProp(itemIndex, kpidIsAltStream);
  return prop.vt == VT_BOOL && prop.boolVal != VARIANT_FALSE;
}

UInt64 CPanelModel::ItemSize(int itemIndex) const
{
  if (_folderGetItemName)
    return _folderGetItemName->GetItemSize((UInt32)itemIndex);
  const CPropVariant prop = ItemProp(itemIndex, kpidSize);
  UInt64 v = 0;
  ConvertPropVariantToUInt64(prop, v);
  return v;
}

UString CPanelModel::ItemName(int itemIndex) const
{
  if (_folderGetItemName)
  {
    const wchar_t *name;
    unsigned len;
    if (_folderGetItemName->GetItemName((UInt32)itemIndex, &name, &len) == S_OK && name)
      return UString(name);
  }
  const CPropVariant prop = ItemProp(itemIndex, kpidName);
  if (prop.vt == VT_BSTR)
    return UString(prop.bstrVal);
  return UString();
}

UString CPanelModel::ItemPrefix(int itemIndex) const
{
  if (_folderGetItemName)
  {
    const wchar_t *name;
    unsigned len;
    if (_folderGetItemName->GetItemPrefix((UInt32)itemIndex, &name, &len) == S_OK && name)
      return UString(name);
  }
  const CPropVariant prop = ItemProp(itemIndex, kpidPrefix);
  if (prop.vt == VT_BSTR)
    return UString(prop.bstrVal);
  return UString();
}

UString CPanelModel::ItemRelPath(int itemIndex) const
{
  UString s = ItemPrefix(itemIndex);
  s += ItemName(itemIndex);
  return s;
}


// ---------------------------------------------------------------------------
// sorting: same rules as UI/FileManager/PanelSort.cpp
// ---------------------------------------------------------------------------

int CPanelModel::CompareItems2(int i1, int i2, PROPID propID, bool isRawProp) const
{
  if (propID == kpidNoProperty)
    return MyCompare(i1, i2);

  if (isRawProp && _folderRawProps)
  {
    const void *data1; const void *data2;
    UInt32 dataSize1; UInt32 dataSize2;
    UInt32 propType1; UInt32 propType2;
    if (_folderRawProps->GetRawProp((UInt32)i1, propID, &data1, &dataSize1, &propType1) != S_OK) return 0;
    if (_folderRawProps->GetRawProp((UInt32)i2, propID, &data2, &dataSize2, &propType2) != S_OK) return 0;
    if (dataSize1 == 0) return (dataSize2 == 0) ? 0 : -1;
    if (dataSize2 == 0) return 1;
    if (propType1 != NPropDataType::kRaw) return 0;
    if (propType2 != NPropDataType::kRaw) return 0;
  }

  if (_folderCompare)
    return _folderCompare->CompareItems((UInt32)i1, (UInt32)i2, propID, BoolToInt(isRawProp));

  switch (propID)
  {
    case kpidName:
      return CompareFileNames_ForFolderList(ItemName(i1), ItemName(i2));
    case kpidExtension:
    {
      const UString n1 = ItemName(i1);
      const UString n2 = ItemName(i2);
      const int dot1 = n1.ReverseFind_Dot();
      const int dot2 = n2.ReverseFind_Dot();
      return CompareFileNames_ForFolderList(
          dot1 < 0 ? L"" : n1.Ptr((unsigned)dot1 + 1),
          dot2 < 0 ? L"" : n2.Ptr((unsigned)dot2 + 1));
    }
    default: break;
  }

  CPropVariant prop1 = ItemProp(i1, propID);
  CPropVariant prop2 = ItemProp(i2, propID);
  if (prop1.vt != prop2.vt)
    return MyCompare(prop1.vt, prop2.vt);
  if (prop1.vt == VT_BSTR)
    return MyStringCompareNoCase(prop1.bstrVal, prop2.bstrVal);
  return prop1.Compare(prop2);
}


int CPanelModel::CompareItems(int i1, int i2) const
{
  const bool isDir1 = IsItemFolder(i1);
  const bool isDir2 = IsItemFolder(i2);
  if (isDir1 != isDir2)
    return isDir1 ? -1 : 1;

  PROPID propID = _sortID;
  int res = 0;
  for (unsigned iter = 0; iter < 3; iter++)
  {
    res = CompareItems2(i1, i2, propID, iter ? false : _isRawSortProp);
    if (res)
      break;
    if (propID == kpidName)
    {
      propID = kpidPrefix;
      continue;
    }
    if (iter)
      break;
    propID = kpidName;
  }
  if (res == 0)
    res = MyCompare(i1, i2);   // keep the folder's own enumeration order
  return _ascending ? res : -res;
}


void CPanelModel::BuildOrder()
{
  _order.resize((int)_numItems);
  for (int i = 0; i < (int)_numItems; i++)
    _order[i] = i;

  if (!_folder)
    return;

  std::stable_sort(_order.begin(), _order.end(),
      [this](int a, int b) { return CompareItems(a, b) < 0; });
}


void CPanelModel::SortByPropID(PROPID propID)
{
  if (propID == _sortID)
    SetSort(propID, !_ascending);
  else
  {
    // 7-Zip starts sizes and timestamps in descending order.
    bool asc = true;
    switch (propID)
    {
      case kpidSize:
      case kpidPackSize:
      case kpidCTime:
      case kpidATime:
      case kpidMTime:
        asc = false;
        break;
      default: break;
    }
    SetSort(propID, asc);
  }
}

void CPanelModel::SetSort(PROPID propID, bool ascending)
{
  beginResetModel();
  _sortID = propID;
  _ascending = ascending;
  _isRawSortProp = false;
  const int col = ColumnForPropID(propID);
  if (col >= 0)
    _isRawSortProp = _columns[col].IsRawProp;
  BuildOrder();
  endResetModel();
}


// ---------------------------------------------------------------------------
// QAbstractTableModel
// ---------------------------------------------------------------------------

int CPanelModel::rowCount(const QModelIndex &parent) const
{
  if (parent.isValid())
    return 0;
  return _order.size() + (_showParentItem ? 1 : 0);
}

int CPanelModel::columnCount(const QModelIndex &parent) const
{
  if (parent.isValid())
    return 0;
  return _columns.size();
}

QIcon CPanelModel::IconForItem(int itemIndex) const
{
  if (itemIndex < 0)
    return GetParentFolderIcon();
  if (_iconCache.size() != (int)_numItems)
    _iconCache.resize((int)_numItems);
  if (!_iconCache[itemIndex].isNull())
    return _iconCache[itemIndex];

  const bool isDir = IsItemFolder(itemIndex);
  const QString name = Us2Q(ItemName(itemIndex));
  QIcon icon;
  if (_isFsFolder)
    icon = GetFileSystemIcon(FolderPath() + name, isDir);
  else
    icon = isDir ? GetFolderIcon() : GetFileIconForName(name);
  _iconCache[itemIndex] = icon;
  return icon;
}

QVariant CPanelModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || !_folder)
    return QVariant();

  const int itemIndex = ItemIndex(index.row());
  const int col = index.column();
  if (col < 0 || col >= _columns.size())
    return QVariant();
  const CPanelColumn &column = _columns[col];

  if (itemIndex < 0)
  {
    // the ".." entry
    if (col != 0)
      return QVariant();
    if (role == Qt::DisplayRole)
      return QStringLiteral("..");
    if (role == Qt::DecorationRole)
      return GetParentFolderIcon();
    return QVariant();
  }

  switch (role)
  {
    case Qt::DisplayRole:
    {
      if (column.IsRawProp && _folderRawProps)
      {
        const void *data2;
        UInt32 dataSize;
        UInt32 propType;
        if (_folderRawProps->GetRawProp((UInt32)itemIndex, column.PropID,
            &data2, &dataSize, &propType) != S_OK || dataSize == 0)
          return QVariant();
        QString hex;
        const Byte *p = (const Byte *)data2;
        for (UInt32 i = 0; i < dataSize && i < 64; i++)
          hex += QString::asprintf("%02X", p[i]);
        return hex;
      }
      const CPropVariant prop = ItemProp(itemIndex, column.PropID);
      return PropToString(prop, column.PropID);
    }
    case Qt::DecorationRole:
      return col == 0 ? QVariant(IconForItem(itemIndex)) : QVariant();
    case Qt::TextAlignmentRole:
      return IsPropRightAligned(column.PropID)
          ? QVariant(int(Qt::AlignRight | Qt::AlignVCenter))
          : QVariant(int(Qt::AlignLeft | Qt::AlignVCenter));
    default:
      break;
  }
  return QVariant();
}

QVariant CPanelModel::headerData(int section, Qt::Orientation o, int role) const
{
  if (o != Qt::Horizontal)
    return QVariant();
  if (section < 0 || section >= _columns.size())
    return QVariant();
  if (role == Qt::DisplayRole)
    return _columns[section].Name;
  if (role == Qt::TextAlignmentRole)
    return IsPropRightAligned(_columns[section].PropID)
        ? QVariant(int(Qt::AlignRight | Qt::AlignVCenter))
        : QVariant(int(Qt::AlignLeft | Qt::AlignVCenter));
  return QVariant();
}

Qt::ItemFlags CPanelModel::flags(const QModelIndex &index) const
{
  if (!index.isValid())
    return Qt::NoItemFlags;
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}
