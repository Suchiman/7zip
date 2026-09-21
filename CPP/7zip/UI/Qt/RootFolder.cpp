// Qt/RootFolder.cpp

#include "StdAfx.h"

#include <sys/statvfs.h>
#include <stdlib.h>
#include <stdio.h>
#include <mntent.h>

#include "../../../Common/ComTry.h"
#include "../../../Common/StringConvert.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/PropVariant.h"

#include "../../PropID.h"

#include "RootFolder.h"
#include "FsFolder.h"
#include "Z7Qt.h"

using namespace NWindows;

namespace NQtRootFolder {

static const Byte kProps[] =
{
  kpidName,
  kpidTotalSize,
  kpidFreeSpace,
  kpidClusterSize,
  kpidFileSystem
};

// Pseudo file systems are noise in a file manager, so they are skipped, the
// same way the Windows panel only lists real drives.
static bool IsInterestingFs(const char *type, const char *dev)
{
  static const char * const kSkip[] =
  {
    "proc", "sysfs", "devtmpfs", "devpts", "tmpfs", "cgroup", "cgroup2",
    "securityfs", "pstore", "efivarfs", "bpf", "debugfs", "tracefs",
    "configfs", "fusectl", "hugetlbfs", "mqueue", "binfmt_misc", "autofs",
    "ramfs", "rpc_pipefs", "nsfs", "squashfs", "overlay", "fuse.portal"
  };
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(kSkip); i++)
    if (strcmp(type, kSkip[i]) == 0)
      return false;
  // a real backing device, or a network share
  return dev[0] == '/'
      || strncmp(type, "nfs", 3) == 0
      || strcmp(type, "cifs") == 0
      || strcmp(type, "smbfs") == 0;
}

static void FillSpace(CRootItem &item)
{
  struct statvfs vfs;
  if (statvfs(item.Path.Ptr(), &vfs) != 0)
    return;
  const UInt64 unit = vfs.f_frsize ? (UInt64)vfs.f_frsize : (UInt64)vfs.f_bsize;
  item.TotalSize = (UInt64)vfs.f_blocks * unit;
  item.FreeSpace = (UInt64)vfs.f_bavail * unit;
  item.ClusterSize = (UInt64)(vfs.f_bsize ? vfs.f_bsize : unit);
  item.SizeDefined = true;
}


Z7_COM7F_IMF(CRootFolder::LoadItems())
{
  COM_TRY_BEGIN
  Items.Clear();

  {
    CRootItem &item = Items.AddNew();
    item.Name = L"/";
    item.Path = "/";
    item.FileSystem = L"Root";
    FillSpace(item);
  }
  {
    const char *home = getenv("HOME");
    if (home && *home)
    {
      CRootItem &item = Items.AddNew();
      item.Name = GetUnicodeString(home);
      item.Path = home;
      item.FileSystem = L"Home";
      FillSpace(item);
    }
  }

  if (FILE *f = setmntent("/proc/self/mounts", "r"))
  {
    struct mntent *me;
    while ((me = getmntent(f)) != NULL)
    {
      if (!IsInterestingFs(me->mnt_type, me->mnt_fsname))
        continue;
      if (strcmp(me->mnt_dir, "/") == 0)
        continue;
      bool dup = false;
      FOR_VECTOR (i, Items)
        if (Items[i].Path == FString(me->mnt_dir))
          { dup = true; break; }
      if (dup)
        continue;
      CRootItem &item = Items.AddNew();
      item.Name = GetUnicodeString(me->mnt_dir);
      item.Path = me->mnt_dir;
      item.FileSystem = GetUnicodeString(me->mnt_type);
      FillSpace(item);
    }
    endmntent(f);
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CRootFolder::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = Items.Size();
  return S_OK;
}

Z7_COM7F_IMF(CRootFolder::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  if (index >= Items.Size())
    return E_INVALIDARG;
  const CRootItem &item = Items[index];
  switch (propID)
  {
    case kpidIsDir:      prop = true; break;
    case kpidName:       prop = item.Name; break;
    case kpidFileSystem: prop = item.FileSystem; break;
    case kpidTotalSize:   if (item.SizeDefined) prop = item.TotalSize; break;
    case kpidFreeSpace:   if (item.SizeDefined) prop = item.FreeSpace; break;
    case kpidClusterSize: if (item.SizeDefined) prop = item.ClusterSize; break;
    default: break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CRootFolder::BindToFolder(UInt32 index, IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN
  *resultFolder = NULL;
  if (index >= Items.Size())
    return E_INVALIDARG;
  return NQtFsFolder::CreateFsFolder(Items[index].Path, resultFolder);
  COM_TRY_END
}

Z7_COM7F_IMF(CRootFolder::BindToFolder(const wchar_t *name, IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN
  *resultFolder = NULL;
  const UString n (name);
  FOR_VECTOR (i, Items)
    if (Items[i].Name == n)
      return NQtFsFolder::CreateFsFolder(Items[i].Path, resultFolder);
  // Not one of the listed entries: treat it as an absolute path.
  return NQtFsFolder::CreateFsFolder(us2fs(name), resultFolder);
  COM_TRY_END
}

Z7_COM7F_IMF(CRootFolder::BindToParentFolder(IFolderFolder **resultFolder))
{
  *resultFolder = NULL;   // this is the top of the tree
  return S_OK;
}

IMP_IFolderFolder_Props(CRootFolder)

Z7_COM7F_IMF(CRootFolder::GetFolderProperty(PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidType: prop = "RootFolder"; break;
    case kpidPath: prop = UString(); break;
    default: break;
  }
  prop.Detach(value);
  return S_OK;
}

Z7_COM7F_IMF(CRootFolder::GetSystemIconIndex(UInt32 index, Int32 *iconIndex))
{
  UNUSED_VAR(index)
  *iconIndex = 0;
  return S_OK;
}

Z7_COM7F_IMF2(Int32, CRootFolder::CompareItems(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw))
{
  UNUSED_VAR(propIsRaw)
  const CRootItem &i1 = Items[index1];
  const CRootItem &i2 = Items[index2];
  switch (propID)
  {
    case kpidName:      return CompareFileNames_ForFolderList(i1.Name, i2.Name);
    case kpidTotalSize: return MyCompare(i1.TotalSize, i2.TotalSize);
    case kpidFreeSpace: return MyCompare(i1.FreeSpace, i2.FreeSpace);
    default: break;
  }
  return 0;
}

Z7_COM7F_IMF(CRootFolder::Clone(IFolderFolder **resultFolder))
{
  COM_TRY_BEGIN
  CRootFolder *folderSpec = new CRootFolder;
  CMyComPtr<IFolderFolder> folder = folderSpec;
  folderSpec->Items = Items;
  *resultFolder = folder.Detach();
  return S_OK;
  COM_TRY_END
}

}

HRESULT CreateRootFolder(IFolderFolder **resultFolder)
{
  *resultFolder = NULL;
  NQtRootFolder::CRootFolder *folderSpec = new NQtRootFolder::CRootFolder;
  CMyComPtr<IFolderFolder> folder = folderSpec;
  RINOK(folder->LoadItems())
  *resultFolder = folder.Detach();
  return S_OK;
}
