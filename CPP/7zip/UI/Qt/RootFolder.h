// Qt/RootFolder.h
//
// The listing that sits above "/" — the port of UI/FileManager/RootFolder.cpp
// and FSDrives.cpp. On Windows those show "Computer" with its drive letters;
// here the equivalent is the set of mount points plus the user's home folder.

#ifndef ZIP7_INC_QT_ROOT_FOLDER_H
#define ZIP7_INC_QT_ROOT_FOLDER_H

#include "../../../Common/MyCom.h"
#include "../FileManager/IFolder.h"

namespace NQtRootFolder {

struct CRootItem
{
  UString Name;         // what the panel shows
  FString Path;         // where selecting it navigates to
  UString FileSystem;
  UInt64 TotalSize;
  UInt64 FreeSpace;
  UInt64 ClusterSize;
  bool SizeDefined;

  CRootItem(): TotalSize(0), FreeSpace(0), ClusterSize(0), SizeDefined(false) {}
};

class CRootFolder Z7_final:
  public IFolderFolder,
  public IFolderGetSystemIconIndex,
  public IFolderCompare,
  public IFolderClone,
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IFolderFolder)
    Z7_COM_QI_ENTRY(IFolderGetSystemIconIndex)
    Z7_COM_QI_ENTRY(IFolderCompare)
    Z7_COM_QI_ENTRY(IFolderClone)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(IFolderFolder)
  Z7_IFACE_COM7_IMP(IFolderGetSystemIconIndex)
  Z7_IFACE_COM7_IMP(IFolderCompare)
  Z7_IFACE_COM7_IMP(IFolderClone)

public:
  CObjectVector<CRootItem> Items;
};

}

HRESULT CreateRootFolder(IFolderFolder **resultFolder);

#endif
