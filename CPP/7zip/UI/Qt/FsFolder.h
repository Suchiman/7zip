// Qt/FsFolder.h
//
// IFolderFolder for the local (POSIX) file system.
//
// This is the port of UI/FileManager/FSFolder.cpp. That file is written
// against the Win32/NTFS model (alt streams, FILE_ATTRIBUTE_*, compressed
// sizes via GetCompressedFileSizeW, NtQueryInformationFile), none of which
// exists here, so the implementation is rewritten for POSIX while keeping
// the same interface set and the same property columns that the panel and
// the rest of the engine expect.

#ifndef ZIP7_INC_QT_FS_FOLDER_H
#define ZIP7_INC_QT_FS_FOLDER_H

#include "../../../Common/MyCom.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/TimeUtils.h"

#include "../../Archive/IArchive.h"
#include "../FileManager/IFolder.h"

namespace NQtFsFolder {

struct CDirItem: public NWindows::NFile::NFind::CFileInfo
{
  int Parent;            // index of the parent item in flat mode, -1 otherwise
  UInt64 NumFolders;
  UInt64 NumFiles;
  bool FolderStat_Defined;

  CDirItem(): Parent(-1), NumFolders(0), NumFiles(0), FolderStat_Defined(false) {}
};

// Recursive "how big is this directory" walk, used by the Size column for
// folders and by the panel's "Calculate occupied size" command.
struct CFsFolderStat
{
  UInt64 NumFolders;
  UInt64 NumFiles;
  UInt64 Size;
  IProgress *Progress;
  FString Path;

  CFsFolderStat(): NumFolders(0), NumFiles(0), Size(0), Progress(NULL) {}
  CFsFolderStat(const FString &path, IProgress *progress = NULL):
      NumFolders(0), NumFiles(0), Size(0), Progress(progress), Path(path) {}

  HRESULT Enumerate();
};


class CFsFolder Z7_final:
  public IFolderFolder,
  public IFolderCompare,
  public IFolderGetItemName,
  public IFolderWasChanged,
  public IFolderOperations,
  public IFolderCalcItemFullSize,
  public IFolderClone,
  public IFolderSetFlatMode,
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IFolderFolder)
    Z7_COM_QI_ENTRY(IFolderCompare)
    Z7_COM_QI_ENTRY(IFolderGetItemName)
    Z7_COM_QI_ENTRY(IFolderWasChanged)
    Z7_COM_QI_ENTRY(IFolderOperations)
    Z7_COM_QI_ENTRY(IFolderCalcItemFullSize)
    Z7_COM_QI_ENTRY(IFolderClone)
    Z7_COM_QI_ENTRY(IFolderSetFlatMode)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(IFolderFolder)
  Z7_IFACE_COM7_IMP(IFolderCompare)
  Z7_IFACE_COM7_IMP(IFolderGetItemName)
  Z7_IFACE_COM7_IMP(IFolderWasChanged)
  Z7_IFACE_COM7_IMP(IFolderOperations)
  Z7_IFACE_COM7_IMP(IFolderCalcItemFullSize)
  Z7_IFACE_COM7_IMP(IFolderClone)
  Z7_IFACE_COM7_IMP(IFolderSetFlatMode)

public:
  CObjectVector<CDirItem> Files;
  FString _path;
  CMyComPtr<IFolderFolder> _parentFolder;

  CFsFolder(): _flatMode(false), _scanTimeIsValid(false) { FiTime_Clear(_scanMTime); }

  HRESULT Init(const FString &path);
  // Reloads the directory listing from disk.
  HRESULT LoadSubItems(int dirItem, const FString &relPrefix);

  const FString &GetPath() const { return _path; }
  FString GetRelPath(const CDirItem &item) const;
  void GetPrefix(const CDirItem &item, FString &prefix) const;

private:
  bool _flatMode;
  bool _scanTimeIsValid;
  CFiTime _scanMTime;   // mtime of _path at load time, for IFolderWasChanged

  CObjectVector<FString> _prefixes;   // flat mode: relative dir prefixes

  HRESULT GetItemFullSize(unsigned index, UInt64 &size, IProgress *progress);
  HRESULT BindToFolderPath(const FString &path, IFolderFolder **resultFolder);
};

// Convenience used by the panel and by the "open folder" paths.
HRESULT CreateFsFolder(const FString &path, IFolderFolder **resultFolder);

}

#endif
