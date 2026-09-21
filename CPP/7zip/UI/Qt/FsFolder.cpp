// Qt/FsFolder.cpp

#include "StdAfx.h"

#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/MyBuffer.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"
#include "../../../Windows/TimeUtils.h"

#include "../../PropID.h"

#include "FsFolder.h"
#include "RootFolder.h"
#include "Z7Qt.h"

using namespace NWindows;
using namespace NFile;
using namespace NFind;
using namespace NDir;

namespace NQtFsFolder {

// The columns a file-system folder offers. This mirrors FSFolder.cpp's kProps,
// with the NTFS-only entries replaced by their POSIX counterparts.
static const Byte kProps[] =
{
  kpidName,
  kpidSize,
  kpidMTime,
  kpidCTime,
  kpidATime,
  kpidAttrib,
  kpidPosixAttrib,
  kpidUser,
  kpidGroup,
  kpidINode,
  kpidLinks,
  kpidSymLink,
  kpidNumSubDirs,
  kpidNumSubFiles,
  kpidPrefix
};

static void FiTimeToProp(const CFiTime &ft, NCOM::CPropVariant &prop)
{
  FILETIME ft2;
  FiTime_To_FILETIME(ft, ft2);
  prop.SetAsTimeFrom_FT_Prec(ft2, k_PropVar_TimePrec_Base);
}

static UString GetUserName_ById(uid_t uid)
{
  const struct passwd *pw = getpwuid(uid);
  if (pw && pw->pw_name)
    return GetUnicodeString(pw->pw_name);
  char temp[16];
  ConvertUInt32ToString((UInt32)uid, temp);
  return GetUnicodeString(temp);
}

static UString GetGroupName_ById(gid_t gid)
{
  const struct group *gr = getgrgid(gid);
  if (gr && gr->gr_name)
    return GetUnicodeString(gr->gr_name);
  char temp[16];
  ConvertUInt32ToString((UInt32)gid, temp);
  return GetUnicodeString(temp);
}

// NDir::RemoveDirWithSubItems() exists only in the _WIN32 part of FileDir.cpp,
// so the posix port needs its own recursive delete.
static bool RemoveDirWithSubItems_Posix(const FString &path)
{
  {
    CEnumerator enumerator;
    FString prefix = path;
    prefix.Add_PathSepar();
    enumerator.SetDirPrefix(prefix);
    CDirEntry de;
    for (;;)
    {
      bool found;
      if (!enumerator.Next(de, found))
        return false;
      if (!found)
        break;
      if (de.IsDots())
        continue;
      const FString full = prefix + de.Name;
      CFileInfo fi;
      const bool isDir = enumerator.Fill_FileInfo(de, fi, false) // (followLink = false)
          && fi.IsDir() && !fi.IsPosixLink();
      if (isDir)
      {
        if (!RemoveDirWithSubItems_Posix(full))
          return false;
      }
      else if (!DeleteFileAlways(full))
        return false;
    }
  }
  return RemoveDir(path);
}

static bool ReadSymLink(const FString &path, UString &res)
{
  char buf[4096];
  const ssize_t len = readlink(path.Ptr(), buf, sizeof(buf) - 1);
  if (len < 0)
    return false;
  buf[len] = 0;
  res = GetUnicodeString(buf);
  return true;
}


// ---------------------------------------------------------------------------
// CFsFolderStat
// ---------------------------------------------------------------------------

HRESULT CFsFolderStat::Enumerate()
{
  if (Progress)
  {
    RINOK(Progress->SetCompleted(NULL))
  }
  FString pathPrefix = Path;
  NName::NormalizeDirPathPrefix(pathPrefix);

  CEnumerator enumerator;
  enumerator.SetDirPrefix(pathPrefix);
  CDirEntry de;
  for (;;)
  {
    bool found;
    if (!enumerator.Next(de, found))
      return S_OK;   // unreadable directory: report what we have so far
    if (!found)
      return S_OK;
    if (de.IsDots())
      continue;
    CFileInfo fi;
    if (!enumerator.Fill_FileInfo(de, fi, false)) // (followLink = false)
      continue;
    if (fi.IsDir() && !fi.IsPosixLink())
    {
      CFsFolderStat stat(pathPrefix + de.Name, Progress);
      RINOK(stat.Enumerate())
      NumFolders += stat.NumFolders + 1;
      NumFiles += stat.NumFiles;
      Size += stat.Size;
    }
    else
    {
      NumFiles++;
      Size += fi.Size;
    }
  }
}


// ---------------------------------------------------------------------------
// CFsFolder
// ---------------------------------------------------------------------------

HRESULT CFsFolder::Init(const FString &path)
{
  _path = path;
  NName::NormalizeDirPathPrefix(_path);
  return S_OK;
}

HRESULT CreateFsFolder(const FString &path, IFolderFolder **resultFolder)
{
  *resultFolder = NULL;
  CFsFolder *folderSpec = new CFsFolder;
  CMyComPtr<IFolderFolder> folder = folderSpec;
  RINOK(folderSpec->Init(path))
  *resultFolder = folder.Detach();
  return S_OK;
}

void CFsFolder::GetPrefix(const CDirItem &item, FString &prefix) const
{
  prefix.Empty();
  if (item.Parent >= 0 && (unsigned)item.Parent < _prefixes.Size())
    prefix = _prefixes[(unsigned)item.Parent];
}

FString CFsFolder::GetRelPath(const CDirItem &item) const
{
  FString prefix;
  GetPrefix(item, prefix);
  return prefix + item.Name;
}


HRESULT CFsFolder::LoadSubItems(int dirItem, const FString &relPrefix)
{
  int prefixIndex = -1;
  if (dirItem >= 0)
  {
    _prefixes.Add(relPrefix);
    prefixIndex = (int)_prefixes.Size() - 1;
  }

  const unsigned startIndex = Files.Size();
  {
    CEnumerator enumerator;
    enumerator.SetDirPrefix(_path + relPrefix);
    CDirEntry de;
    for (;;)
    {
      bool found;
      if (!enumerator.Next(de, found))
        break;
      if (!found)
        break;
      if (de.IsDots())
        continue;
      CDirItem &item = Files.AddNew();
      CFileInfo fi;
      if (enumerator.Fill_FileInfo(de, fi, false)) // (followLink = false)
        (CFileInfo &)item = fi;
      // An entry that cannot be stat()ed still stays visible, like in 7-Zip.
      item.Name = de.Name;
      item.Parent = prefixIndex;
    }
  }

  if (_flatMode)
  {
    const unsigned endIndex = Files.Size();
    for (unsigned i = startIndex; i < endIndex; i++)
    {
      if (!Files[i].IsDir() || Files[i].IsPosixLink())
        continue;
      FString sub = relPrefix;
      sub += Files[i].Name;
      sub.Add_PathSepar();
      RINOK(LoadSubItems(0, sub))
    }
  }
  return S_OK;
}


Z7_COM7F_IMF(CFsFolder::LoadItems())
{
  COM_TRY_BEGIN
  Files.Clear();
  _prefixes.Clear();

  RINOK(LoadSubItems(_flatMode ? 0 : -1, FString()))

  {
    // remember the directory mtime, so that WasChanged() can answer cheaply
    CFileInfo fi;
    _scanTimeIsValid = fi.Find(_path);
    if (_scanTimeIsValid)
      _scanMTime = fi.MTime;
  }
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CFsFolder::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = Files.Size();
  return S_OK;
}

Z7_COM7F_IMF(CFsFolder::GetItemName(UInt32 index, const wchar_t **name, unsigned *len))
{
  UNUSED_VAR(index)
  *name = NULL;
  *len = 0;
  // FString is a byte string here, so there is no wchar_t buffer to hand out.
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CFsFolder::GetItemPrefix(UInt32 index, const wchar_t **name, unsigned *len))
{
  UNUSED_VAR(index)
  *name = NULL;
  *len = 0;
  return E_NOTIMPL;
}

Z7_COM7F_IMF2(UInt64, CFsFolder::GetItemSize(UInt32 index))
{
  const CDirItem &fi = Files[index];
  return fi.IsDir() ? 0 : fi.Size;
}


Z7_COM7F_IMF(CFsFolder::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  const CDirItem &fi = Files[index];
  switch (propID)
  {
    case kpidIsDir:  prop = fi.IsDir(); break;
    case kpidName:   prop = fs2us(fi.Name); break;
    case kpidSize:   if (!fi.IsDir()) prop = fi.Size; break;
    case kpidMTime:  FiTimeToProp(fi.MTime, prop); break;
    case kpidCTime:  FiTimeToProp(fi.CTime, prop); break;
    case kpidATime:  FiTimeToProp(fi.ATime, prop); break;
    case kpidAttrib: prop = (UInt32)fi.GetWinAttrib(); break;
    case kpidPosixAttrib: prop = (UInt32)fi.mode; break;
    case kpidUser:   prop = GetUserName_ById(fi.uid); break;
    case kpidGroup:  prop = GetGroupName_ById(fi.gid); break;
    case kpidINode:  prop = (UInt64)fi.ino; break;
    case kpidLinks:  prop = (UInt64)fi.nlink; break;
    case kpidSymLink:
    {
      if (fi.IsPosixLink())
      {
        UString target;
        if (ReadSymLink(_path + GetRelPath(fi), target))
          prop = target;
      }
      break;
    }
    case kpidNumSubDirs:  if (fi.IsDir() && fi.FolderStat_Defined) prop = fi.NumFolders; break;
    case kpidNumSubFiles: if (fi.IsDir() && fi.FolderStat_Defined) prop = fi.NumFiles; break;
    case kpidPrefix:
    {
      if (fi.Parent >= 0)
      {
        FString prefix;
        GetPrefix(fi, prefix);
        prop = fs2us(prefix);
      }
      break;
    }
    default: break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


HRESULT CFsFolder::BindToFolderPath(const FString &path, IFolderFolder **resultFolder)
{
  *resultFolder = NULL;
  CFsFolder *folderSpec = new CFsFolder;
  CMyComPtr<IFolderFolder> folder = folderSpec;
  RINOK(folderSpec->Init(path))
  *resultFolder = folder.Detach();
  return S_OK;
}

Z7_COM7F_IMF(CFsFolder::BindToFolder(UInt32 index, IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN
  *resultFolder = NULL;
  if (index >= Files.Size())
    return E_INVALIDARG;
  const CDirItem &fi = Files[index];
  if (!fi.IsDir())
    return E_INVALIDARG;
  return BindToFolderPath(_path + GetRelPath(fi), resultFolder);
  COM_TRY_END
}

Z7_COM7F_IMF(CFsFolder::BindToFolder(const wchar_t *name, IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN
  return BindToFolderPath(_path + us2fs(name), resultFolder);
  COM_TRY_END
}

Z7_COM7F_IMF(CFsFolder::BindToParentFolder(IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN
  *resultFolder = NULL;
  if (_parentFolder)
  {
    CMyComPtr<IFolderFolder> parent = _parentFolder;
    *resultFolder = parent.Detach();
    return S_OK;
  }
  FString parentPath = _path;
  while (!parentPath.IsEmpty() && parentPath.Back() == '/')
    parentPath.DeleteBack();
  if (parentPath.IsEmpty())
  {
    // Above "/" there is only the root listing.
    return CreateRootFolder(resultFolder);
  }
  const int slash = parentPath.ReverseFind_PathSepar();
  if (slash < 0)
    return CreateRootFolder(resultFolder);
  parentPath.DeleteFrom((unsigned)slash + 1);
  return BindToFolderPath(parentPath, resultFolder);
  COM_TRY_END
}

IMP_IFolderFolder_Props(CFsFolder)

Z7_COM7F_IMF(CFsFolder::GetFolderProperty(PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidType: prop = "FSFolder"; break;
    case kpidPath: prop = fs2us(_path); break;
    default: break;
  }
  prop.Detach(value);
  return S_OK;
}


Z7_COM7F_IMF(CFsFolder::WasChanged(Int32 *wasChanged))
{
  bool changed;
  CFileInfo fi;
  if (fi.Find(_path))
    changed = !_scanTimeIsValid || Compare_FiTime(&fi.MTime, &_scanMTime) != 0;
  else
    changed = _scanTimeIsValid;
  *wasChanged = BoolToInt(changed);
  return S_OK;
}

Z7_COM7F_IMF(CFsFolder::Clone(IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN
  CFsFolder *folderSpec = new CFsFolder;
  CMyComPtr<IFolderFolder> folder = folderSpec;
  folderSpec->_path = _path;
  folderSpec->_flatMode = _flatMode;
  folderSpec->_parentFolder = _parentFolder;
  *resultFolder = folder.Detach();
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CFsFolder::SetFlatMode(Int32 flatMode))
{
  _flatMode = IntToBool(flatMode);
  return S_OK;
}


Z7_COM7F_IMF2(Int32, CFsFolder::CompareItems(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw))
{
  UNUSED_VAR(propIsRaw)
  const CDirItem &fi1 = Files[index1];
  const CDirItem &fi2 = Files[index2];
  switch (propID)
  {
    case kpidName:
      return CompareFileNames_ForFolderList(fs2us(fi1.Name), fs2us(fi2.Name));
    case kpidSize:
      return MyCompare(fi1.IsDir() ? (UInt64)0 : fi1.Size,
                       fi2.IsDir() ? (UInt64)0 : fi2.Size);
    case kpidAttrib:
      return MyCompare(fi1.GetWinAttrib(), fi2.GetWinAttrib());
    case kpidPosixAttrib:
      return MyCompare((UInt32)fi1.mode, (UInt32)fi2.mode);
    case kpidMTime:
      return Compare_FiTime(&fi1.MTime, &fi2.MTime);
    case kpidCTime:
      return Compare_FiTime(&fi1.CTime, &fi2.CTime);
    case kpidATime:
      return Compare_FiTime(&fi1.ATime, &fi2.ATime);
    case kpidINode:
      return MyCompare((UInt64)fi1.ino, (UInt64)fi2.ino);
    case kpidLinks:
      return MyCompare((UInt64)fi1.nlink, (UInt64)fi2.nlink);
    case kpidPrefix:
    {
      FString p1, p2;
      GetPrefix(fi1, p1);
      GetPrefix(fi2, p2);
      return CompareFileNames_ForFolderList(fs2us(p1), fs2us(p2));
    }
    default: break;
  }
  return 0;
}


HRESULT CFsFolder::GetItemFullSize(unsigned index, UInt64 &size, IProgress *progress)
{
  const CDirItem &fi = Files[index];
  if (!fi.IsDir())
  {
    size = fi.Size;
    return S_OK;
  }
  CFsFolderStat stat(_path + GetRelPath(fi), progress);
  RINOK(stat.Enumerate())
  Files[index].NumFolders = stat.NumFolders;
  Files[index].NumFiles = stat.NumFiles;
  Files[index].FolderStat_Defined = true;
  size = stat.Size;
  return S_OK;
}

Z7_COM7F_IMF(CFsFolder::CalcItemFullSize(UInt32 index, IProgress *progress))
{
  COM_TRY_BEGIN
  if (index >= Files.Size())
    return E_INVALIDARG;
  UInt64 size;
  return GetItemFullSize(index, size, progress);
  COM_TRY_END
}


// ---------------------------------------------------------------------------
// IFolderOperations
// ---------------------------------------------------------------------------

Z7_COM7F_IMF(CFsFolder::CreateFolder(const wchar_t *name, IProgress * /* progress */))
{
  COM_TRY_BEGIN
  if (!CreateDir(_path + us2fs(name)))
    return GetLastError_noZero_HRESULT();
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CFsFolder::CreateFile(const wchar_t *name, IProgress * /* progress */))
{
  COM_TRY_BEGIN
  NIO::COutFile outFile;
  if (!outFile.Create_NEW(_path + us2fs(name)))
    return GetLastError_noZero_HRESULT();
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CFsFolder::Rename(UInt32 index, const wchar_t *newName, IProgress * /* progress */))
{
  COM_TRY_BEGIN
  if (index >= Files.Size())
    return E_INVALIDARG;
  const CDirItem &fi = Files[index];
  FString fullPrefix = _path;
  {
    FString prefix;
    GetPrefix(fi, prefix);
    fullPrefix += prefix;
  }
  if (!MyMoveFile(fullPrefix + fi.Name, fullPrefix + us2fs(newName)))
    return GetLastError_noZero_HRESULT();
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CFsFolder::Delete(const UInt32 *indices, UInt32 numItems, IProgress *progress))
{
  COM_TRY_BEGIN
  if (progress)
  {
    RINOK(progress->SetTotal(numItems))
  }
  for (UInt32 i = 0; i < numItems; i++)
  {
    const UInt32 index = indices[i];
    if (index >= Files.Size())
      return E_INVALIDARG;
    const CDirItem &fi = Files[index];
    const FString fullPath = _path + GetRelPath(fi);
    bool result;
    if (fi.IsDir() && !fi.IsPosixLink())
      result = RemoveDirWithSubItems_Posix(fullPath);
    else
      result = DeleteFileAlways(fullPath);
    if (!result)
      return GetLastError_noZero_HRESULT();
    if (progress)
    {
      const UInt64 completed = (UInt64)i + 1;
      RINOK(progress->SetCompleted(&completed))
    }
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CFsFolder::SetProperty(UInt32 index, PROPID propID,
    const PROPVARIANT *value, IProgress * /* progress */))
{
  COM_TRY_BEGIN
  if (index >= Files.Size())
    return E_INVALIDARG;
  if (propID != kpidPosixAttrib && propID != kpidAttrib)
    return E_NOTIMPL;
  if (value->vt != VT_UI4)
    return E_INVALIDARG;
  const FString path = _path + GetRelPath(Files[index]);
  UInt32 mode = value->ulVal;
  if (propID == kpidAttrib)
  {
    // Only the read-only bit carries over; map it onto the write permissions.
    CFileInfo fi;
    if (!fi.Find(path))
      return GetLastError_noZero_HRESULT();
    mode = (UInt32)fi.mode;
    if (value->ulVal & 1) // FILE_ATTRIBUTE_READONLY
      mode &= ~(UInt32)0222;
    else
      mode |= (UInt32)0200;
  }
  if (chmod(path.Ptr(), (mode_t)(mode & 07777)) != 0)
    return GetLastError_noZero_HRESULT();
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CFsFolder::CopyFromFile(UInt32 /* index */,
    const wchar_t * /* fullFilePath */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}


// ---------- copy / move engine ----------

namespace {

struct CCopyState
{
  IFolderOperationsExtractCallback *Callback;
  bool MoveMode;
  UInt64 TotalSize;
  UInt64 CompletedSize;

  CCopyState(): Callback(NULL), MoveMode(false), TotalSize(0), CompletedSize(0) {}

  HRESULT Progress()
  {
    return Callback ? Callback->SetCompleted(&CompletedSize) : S_OK;
  }
  HRESULT ShowError(const FString &path, const char *what)
  {
    if (!Callback)
      return E_FAIL;
    UString m (GetUnicodeString(what));
    m += ": ";
    m += fs2us(path);
    return Callback->ShowMessage(m);
  }
  HRESULT CopyOneFile(const FString &src, const FString &dest, const CFileInfo &fi);
  HRESULT CopyItem(const FString &src, const FString &dest, const CFileInfo &fi);
};


HRESULT CCopyState::CopyOneFile(const FString &src, const FString &dest, const CFileInfo &fi)
{
  if (MoveMode)
  {
    // A plain rename works whenever source and destination share a mount
    // point; otherwise we fall through to copy + delete.
    if (MyMoveFile(src, dest))
    {
      CompletedSize += fi.Size;
      return Progress();
    }
  }

  NIO::CInFile inFile;
  if (!inFile.Open(src))
  {
    RINOK(ShowError(src, "Cannot open file"))
    return S_FALSE;
  }
  NIO::COutFile outFile;
  if (!outFile.Create_ALWAYS(dest))
  {
    RINOK(ShowError(dest, "Cannot create file"))
    return S_FALSE;
  }

  const size_t kBufSize = (size_t)1 << 20;
  CByteBuffer buf(kBufSize);
  for (;;)
  {
    size_t processed;
    if (!inFile.ReadFull((Byte *)buf, kBufSize, processed))
    {
      RINOK(ShowError(src, "Read error"))
      return S_FALSE;
    }
    if (processed == 0)
      break;
    if (!outFile.WriteFull((const Byte *)buf, processed))
    {
      RINOK(ShowError(dest, "Write error"))
      return S_FALSE;
    }
    CompletedSize += processed;
    RINOK(Progress())
  }
  outFile.SetMTime(&fi.MTime);
  outFile.Close();
  chmod(dest.Ptr(), (mode_t)(fi.mode & 07777));

  if (MoveMode)
  {
    if (!DeleteFileAlways(src))
    {
      RINOK(ShowError(src, "Cannot delete file"))
      return S_FALSE;
    }
  }
  return S_OK;
}


HRESULT CCopyState::CopyItem(const FString &src, const FString &dest, const CFileInfo &fi)
{
  if (Callback)
  {
    RINOK(Callback->SetCurrentFilePath(fs2us(src)))
  }

  if (fi.IsDir() && !fi.IsPosixLink())
  {
    if (!CreateComplexDir(dest))
    {
      RINOK(ShowError(dest, "Cannot create folder"))
      return S_FALSE;
    }
    FString srcPrefix = src;   srcPrefix.Add_PathSepar();
    FString destPrefix = dest; destPrefix.Add_PathSepar();

    CEnumerator enumerator;
    enumerator.SetDirPrefix(srcPrefix);
    CDirEntry de;
    for (;;)
    {
      bool found;
      if (!enumerator.Next(de, found) || !found)
        break;
      if (de.IsDots())
        continue;
      CFileInfo sub;
      if (!enumerator.Fill_FileInfo(de, sub, false)) // (followLink = false)
        continue;
      sub.Name = de.Name;
      RINOK(CopyItem(srcPrefix + de.Name, destPrefix + de.Name, sub))
    }
    if (MoveMode)
      RemoveDir(src);
    return S_OK;
  }

  if (fi.IsPosixLink())
  {
    UString target;
    if (ReadSymLink(src, target))
    {
      DeleteFileAlways(dest);
      if (symlink(us2fs(target).Ptr(), dest.Ptr()) == 0)
      {
        if (MoveMode)
          DeleteFileAlways(src);
        return S_OK;
      }
    }
    RINOK(ShowError(dest, "Cannot create symbolic link"))
    return S_FALSE;
  }

  // Ask the UI what to do when the destination already exists.
  if (Callback && DoesFileExist_Raw(dest))
  {
    FILETIME ft;
    FiTime_To_FILETIME(fi.MTime, ft);
    Int32 writeAnswer = BoolToInt(true);
    CMyComBSTR destPathResult;
    RINOK(Callback->AskWrite(
        fs2us(src),
        BoolToInt(false),
        &ft, &fi.Size,
        fs2us(dest),
        &destPathResult,
        &writeAnswer))
    if (!IntToBool(writeAnswer))
    {
      CompletedSize += fi.Size;
      return Progress();
    }
    if (destPathResult)
      return CopyOneFile(src, us2fs((LPCOLESTR)destPathResult), fi);
  }
  return CopyOneFile(src, dest, fi);
}

} // namespace


Z7_COM7F_IMF(CFsFolder::CopyTo(Int32 moveMode, const UInt32 *indices, UInt32 numItems,
    Int32 /* includeAltStreams */, Int32 /* replaceAltStreamCharsMode */,
    const wchar_t *path, IFolderOperationsExtractCallback *callback))
{
  COM_TRY_BEGIN
  if (numItems == 0)
    return S_OK;

  FString destPrefix = us2fs(path);
  NName::NormalizeDirPathPrefix(destPrefix);
  if (!CreateComplexDir(destPrefix))
    return GetLastError_noZero_HRESULT();

  CCopyState state;
  state.Callback = callback;
  state.MoveMode = IntToBool(moveMode);

  // total size first, so that the progress bar means something
  for (UInt32 i = 0; i < numItems; i++)
  {
    const UInt32 index = indices[i];
    if (index >= Files.Size())
      return E_INVALIDARG;
    const CDirItem &fi = Files[index];
    if (fi.IsDir() && !fi.IsPosixLink())
    {
      CFsFolderStat stat(_path + GetRelPath(fi), callback);
      RINOK(stat.Enumerate())
      state.TotalSize += stat.Size;
    }
    else
      state.TotalSize += fi.Size;
  }
  if (callback)
  {
    RINOK(callback->SetNumFiles(numItems))
    RINOK(callback->SetTotal(state.TotalSize))
  }

  for (UInt32 i = 0; i < numItems; i++)
  {
    const CDirItem &fi = Files[indices[i]];
    RINOK(state.CopyItem(_path + GetRelPath(fi), destPrefix + fi.Name, fi))
  }
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CFsFolder::CopyFrom(Int32 moveMode, const wchar_t *fromFolderPath,
    const wchar_t * const *itemsPaths, UInt32 numItems, IProgress *progress))
{
  COM_TRY_BEGIN
  FString srcPrefix = us2fs(fromFolderPath);
  NName::NormalizeDirPathPrefix(srcPrefix);

  CCopyState state;
  state.MoveMode = IntToBool(moveMode);

  UInt64 total = 0;
  for (UInt32 i = 0; i < numItems; i++)
  {
    CFileInfo fi;
    if (fi.Find(srcPrefix + us2fs(itemsPaths[i])))
      total += fi.Size;
  }
  if (progress)
  {
    RINOK(progress->SetTotal(total))
  }

  for (UInt32 i = 0; i < numItems; i++)
  {
    const FString name = us2fs(itemsPaths[i]);
    CFileInfo fi;
    if (!fi.Find(srcPrefix + name))
      return GetLastError_noZero_HRESULT();
    fi.Name = name;
    RINOK(state.CopyItem(srcPrefix + name, _path + name, fi))
    if (progress)
    {
      RINOK(progress->SetCompleted(&state.CompletedSize))
    }
  }
  return S_OK;
  COM_TRY_END
}

}
