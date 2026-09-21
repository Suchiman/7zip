// Qt/Operations.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"
#include "../../../Common/Wildcard.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileName.h"

#include "../Common/ArchiveName.h"
#include "../Common/Extract.h"
#include "../Common/HashCalc.h"
#include "../Common/LoadCodecs.h"
#include "../Common/OpenArchive.h"
#include "../Common/Update.h"

#include "Callbacks.h"
#include "Codecs.h"
#include "Operations.h"
#include "ProgressDialog.h"

#include <QFileInfo>

using namespace NWindows;
using namespace NWindows::NFile;

// ---------------------------------------------------------------------------
// property helpers, ported verbatim from UI/GUI/UpdateGUI.cpp
// ---------------------------------------------------------------------------

static void AddProp_UString(CObjectVector<CProperty> &properties, const char *name, const UString &value)
{
  CProperty prop;
  prop.Name = name;
  prop.Value = value;
  properties.Add(prop);
}

static void AddProp_UInt32(CObjectVector<CProperty> &properties, const char *name, UInt32 value)
{
  UString s;
  s.Add_UInt32(value);
  AddProp_UString(properties, name, s);
}

static void AddProp_bool(CObjectVector<CProperty> &properties, const char *name, bool value)
{
  AddProp_UString(properties, name, value ? UString("on") : UString("off"));
}

static void AddProp_Size(CObjectVector<CProperty> &properties, const char *name, const UInt64 size)
{
  UString s;
  s.Add_UInt64(size);
  s.Add_Char('b');
  AddProp_UString(properties, name, s);
}

static void AddProp_BoolPair(CObjectVector<CProperty> &properties, const char *name, const CBoolPair &bp)
{
  if (bp.Def)
    AddProp_bool(properties, name, bp.Val);
}

static void SetOutProperties(CObjectVector<CProperty> &properties,
    const NQtCompress::CInfo &di, bool is7z, bool setMethod)
{
  if (di.Level != (UInt32)(Int32)-1)
    AddProp_UInt32(properties, "x", di.Level);
  if (setMethod)
  {
    if (!di.Method.IsEmpty())
      AddProp_UString(properties, is7z ? "0" : "m", di.Method);
    if (di.Dict64 != (UInt64)(Int64)-1)
    {
      AString name;
      if (is7z)
        name = "0";
      name += (di.OrderMode ? "mem" : "d");
      AddProp_Size(properties, name, di.Dict64);
    }
    if (di.Order != (UInt32)(Int32)-1)
    {
      AString name;
      if (is7z)
        name = "0";
      name += (di.OrderMode ? "o" : "fb");
      AddProp_UInt32(properties, name, di.Order);
    }
  }

  if (!di.EncryptionMethod.IsEmpty())
    AddProp_UString(properties, "em", di.EncryptionMethod);

  if (di.EncryptHeadersIsAllowed)
    AddProp_bool(properties, "he", di.EncryptHeaders);

  if (di.SolidIsSpecified)
    AddProp_Size(properties, "s", di.SolidBlockSize);

  if (di.NumThreads != (UInt32)(Int32)-1)
    AddProp_UInt32(properties, "mt", di.NumThreads);

  AddProp_BoolPair(properties, "tm", di.MTime);
  AddProp_BoolPair(properties, "tc", di.CTime);
  AddProp_BoolPair(properties, "ta", di.ATime);

  if (di.TimePrec != (UInt32)(Int32)-1)
    AddProp_UInt32(properties, "tp", di.TimePrec);
}

// "-mx=9 -mfb=64" style free-form options from the dialog's Parameters field.
static void ParseAndAddProperties(CObjectVector<CProperty> &properties, const UString &s)
{
  UStringVector strings;
  SplitString(s, strings);
  FOR_VECTOR (i, strings)
  {
    const UString &sv = strings[i];
    if (sv.IsEmpty())
      continue;
    CProperty property;
    const int index = sv.Find(L'=');
    if (index < 0)
      property.Name = sv;
    else
    {
      property.Name = sv.Left((unsigned)index);
      property.Value = sv.Ptr((unsigned)index + 1);
    }
    properties.Add(property);
  }
}

static const NUpdateArchive::CActionSet *ActionSetForMode(NQtCompress::NUpdateMode::EEnum mode)
{
  switch (mode)
  {
    case NQtCompress::NUpdateMode::kAdd:    return &NUpdateArchive::k_ActionSet_Add;
    case NQtCompress::NUpdateMode::kUpdate: return &NUpdateArchive::k_ActionSet_Update;
    case NQtCompress::NUpdateMode::kFresh:  return &NUpdateArchive::k_ActionSet_Fresh;
    case NQtCompress::NUpdateMode::kSync:   return &NUpdateArchive::k_ActionSet_Sync;
  }
  return &NUpdateArchive::k_ActionSet_Add;
}


// ---------------------------------------------------------------------------
// CompressFiles
// ---------------------------------------------------------------------------

HRESULT CompressFiles(QWidget *parent,
    const QString &curDirPrefix,
    const QStringList &paths,
    const NQtCompress::CInfo &di,
    QString *errorMessage)
{
  CCodecs *codecs = GetCodecs();
  if (!codecs)
    return E_FAIL;
  if (di.FormatIndex < 0 || (unsigned)di.FormatIndex >= codecs->Formats.Size())
    return E_INVALIDARG;
  const CArcInfoEx &arcInfo = codecs->Formats[(unsigned)di.FormatIndex];

  // The list of things to pack, as a wildcard censor, exactly like the
  // command line builds it.
  NWildcard::CCensor censor;
  {
    FString prefix = Q2Fs(curDirPrefix);
    NName::NormalizeDirPathPrefix(prefix);
    for (const QString &p : paths)
      censor.AddPreItem_NoWildcard(Q2Us(p));
    censor.AddPathsToCensor((NWildcard::ECensorPathMode)di.PathMode);
    censor.ExtendExclude();
  }

  CUpdateOptions options;
  options.SetActionCommand_Add();
  options.Commands.Front().ActionSet = *ActionSetForMode(di.UpdateMode);
  options.PathMode = (NWildcard::ECensorPathMode)di.PathMode;
  options.DeleteAfterCompressing = di.DeleteAfterCompressing;
  options.SymLinks = di.SymLinks;
  options.HardLinks = di.HardLinks;
  options.StoreOwnerId = di.StoreOwnerId;
  options.StoreOwnerName = di.StoreOwnerName;
  options.SetArcMTime = di.SetArcMTime.Val;
  options.VolumesSizes = di.VolumeSizes;

  const bool is7z = arcInfo.Is_7z();
  UString optionStringsAll = di.Options;
  SetOutProperties(options.MethodMode.Properties, di, is7z, true);
  ParseAndAddProperties(options.MethodMode.Properties, optionStringsAll);

  options.MethodMode.Type = COpenType();
  options.MethodMode.Type_Defined = true;
  options.MethodMode.Type.FormatIndex = di.FormatIndex;

  options.ArchivePath.VolExtension = arcInfo.GetMainExt();
  options.ArchivePath.BaseExtension = options.ArchivePath.VolExtension;
  options.ArchivePath.ParseFromPath(di.ArcPath, k_ArcNameMode_Exact);

  CObjectVector<COpenType> formatIndices;
  formatIndices.Add(options.MethodMode.Type);

  CProgressDialog dialog(parent);
  dialog.CompressingMode = true;
  dialog.WaitMode = true;
  dialog.SetTitleFileName(QFileInfo(Us2Q(di.ArcPath)).fileName());

  CUpdateCallbackQt callback;
  callback.ProgressDialog = &dialog;
  callback.Init();
  callback.AskPassword = false;
  if (!di.Password.IsEmpty())
  {
    callback.PasswordIsDefined = true;
    callback.Password = di.Password;
  }

  CUpdateErrorInfo errorInfo;
  const UString cmdArcPath = di.ArcPath;

  const HRESULT res = dialog.Execute([&]() -> HRESULT
  {
    return UpdateArchive(codecs, formatIndices, cmdArcPath,
        censor, options, errorInfo, &callback, &callback,
        true); // needSetPath
  });

  if (errorMessage)
  {
    *errorMessage = QString::fromUtf8(errorInfo.Message.Ptr());
    if (errorMessage->isEmpty() && errorInfo.SystemError != 0)
      *errorMessage = HResultToMessage(HRESULT_FROM_WIN32(errorInfo.SystemError));
  }
  if (res != S_OK)
    return res;
  return errorInfo.SystemError == 0 ? S_OK : HRESULT_FROM_WIN32(errorInfo.SystemError);
}


// ---------------------------------------------------------------------------
// ExtractArchives
// ---------------------------------------------------------------------------

HRESULT ExtractArchives(QWidget *parent,
    const QStringList &archivePaths,
    const CQtExtractInfo &info,
    QString *errorMessage)
{
  CCodecs *codecs = GetCodecs();
  if (!codecs)
    return E_FAIL;

  UStringVector paths, pathsFull;
  for (const QString &p : archivePaths)
  {
    const UString u = Q2Us(p);
    paths.Add(u);
    FString full;
    if (NDir::MyGetFullPathName(us2fs(u), full))
      pathsFull.Add(fs2us(full));
    else
      pathsFull.Add(u);
  }

  // "extract everything" censor
  NWildcard::CCensor censor;
  censor.AddPreItem_Wildcard();
  censor.AddPathsToCensor(NWildcard::k_RelatPath);
  censor.ExtendExclude();

  CExtractOptions options;
  options.TestMode = info.TestMode;
  options.PathMode = info.PathMode;
  options.PathMode_Force = true;
  options.OverwriteMode = info.OverwriteMode;
  options.OverwriteMode_Force = true;
  options.ElimDup.Val = info.ElimDup;
  options.ElimDup.Def = true;
  options.OutDirMode = NExtractOutDirMode::k_Direct;
  if (!info.TestMode)
  {
    FString outDir = Q2Fs(info.OutputDir);
    NName::NormalizeDirPathPrefix(outDir);
    options.OutputDir = outDir;
  }

  CObjectVector<COpenType> types;
  CIntVector excludedFormats;

  CProgressDialog dialog(parent);
  dialog.WaitMode = true;

  CExtractCallbackQt *callbackSpec = new CExtractCallbackQt;
  CMyComPtr<IFolderArchiveExtractCallback> callback = callbackSpec;
  callbackSpec->ProgressDialog = &dialog;
  callbackSpec->Init();
  callbackSpec->MultiArcMode = (archivePaths.size() > 1);
  if (info.PasswordIsDefined)
  {
    callbackSpec->PasswordIsDefined = true;
    callbackSpec->Password = info.Password;
  }

  UString engineErrorMessage;
  CDecompressStat stat;

  const HRESULT res = dialog.Execute([&]() -> HRESULT
  {
    return Extract(codecs, types, excludedFormats,
        paths, pathsFull, censor.Pairs.Front().Head,
        options, callbackSpec, callbackSpec, callback,
        NULL, // IHashCalc
        engineErrorMessage, stat);
  });

  if (errorMessage)
    *errorMessage = Us2Q(engineErrorMessage);
  return res;
}


// ---------------------------------------------------------------------------
// CalcChecksum
// ---------------------------------------------------------------------------

namespace {

class CHashCallbackQt Z7_final: public IHashCallbackUI
{
  Z7_IFACE_IMP(IDirItemsCallback)
  Z7_IFACE_IMP(IHashCallbackUI)
public:
  CProgressDialog *ProgressDialog;
  QVector<CQtHashResult> *Results;
  UInt64 NumFiles;
  UInt64 FilesSize;
  CHashCallbackQt(): ProgressDialog(NULL), Results(NULL), NumFiles(0), FilesSize(0) {}
};

HRESULT CHashCallbackQt::ScanError(const FString &path, DWORD systemError)
{
  ProgressDialog->Sync.AddError_Code_Name(HRESULT_FROM_WIN32(systemError), fs2us(path));
  return S_OK;
}

HRESULT CHashCallbackQt::ScanProgress(const CDirItemsStat &st, const FString &path, bool isDir)
{
  return ProgressDialog->Sync.ScanProgress(st.NumDirs + st.NumFiles,
      st.GetTotalBytes(), path, isDir);
}

HRESULT CHashCallbackQt::StartScanning()
{
  ProgressDialog->Sync.Set_Status(GetUnicodeString("Scanning"));
  return S_OK;
}

HRESULT CHashCallbackQt::FinishScanning(const CDirItemsStat & /* st */) { return S_OK; }

HRESULT CHashCallbackQt::SetNumFiles(UInt64 numFiles)
{
  return ProgressDialog->Sync.Set_NumFilesTotal(numFiles);
}

HRESULT CHashCallbackQt::SetTotal(UInt64 size)
{
  ProgressDialog->Sync.Set_NumBytesTotal(size);
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CHashCallbackQt::SetCompleted(const UInt64 *completeValue)
{
  return ProgressDialog->Sync.Set_NumBytesCur(completeValue);
}

HRESULT CHashCallbackQt::CheckBreak() { return ProgressDialog->Sync.CheckStop(); }

HRESULT CHashCallbackQt::BeforeFirstFile(const CHashBundle & /* hb */) { return S_OK; }

HRESULT CHashCallbackQt::GetStream(const wchar_t *name, bool isFolder)
{
  return ProgressDialog->Sync.Set_Status2(GetUnicodeString("Checking"), name, isFolder);
}

HRESULT CHashCallbackQt::OpenFileError(const FString &path, DWORD systemError)
{
  ProgressDialog->Sync.AddError_Code_Name(HRESULT_FROM_WIN32(systemError), fs2us(path));
  return S_FALSE;
}

HRESULT CHashCallbackQt::SetOperationResult(UInt64 /* fileSize */,
    const CHashBundle & /* hb */, bool /* showHash */)
{
  return S_OK;
}

HRESULT CHashCallbackQt::AfterLastFile(CHashBundle &hb)
{
  // This is the only place the engine hands us the finished digests, so the
  // results the dialog shows are collected here.
  if (!Results)
    return S_OK;
  FOR_VECTOR (i, hb.Hashers)
  {
    const CHasherState &h = hb.Hashers[i];
    CQtHashResult r;
    r.Name = QString::fromUtf8(h.Name.Ptr());
    char temp[k_HashCalc_DigestSize_Max * 2 + 8];
    h.WriteToString(k_HashCalc_Index_DataSum, temp);
    r.Value = QString::fromLatin1(temp);
    Results->append(r);
  }
  NumFiles = hb.NumFiles;
  FilesSize = hb.FilesSize;
  return S_OK;
}

} // namespace


HRESULT CalcChecksum(QWidget *parent,
    const QString &curDirPrefix,
    const QStringList &paths,
    const QString &methods,
    QVector<CQtHashResult> *results,
    QString *errorMessage)
{
  CCodecs *codecs = GetCodecs();
  if (!codecs)
    return E_FAIL;

  NWildcard::CCensor censor;
  {
    FString prefix = Q2Fs(curDirPrefix);
    NName::NormalizeDirPathPrefix(prefix);
    for (const QString &p : paths)
      censor.AddPreItem_NoWildcard(Q2Us(p));
    censor.AddPathsToCensor(NWildcard::k_RelatPath);
    censor.ExtendExclude();
  }

  CHashOptions options;
  if (methods != QLatin1String("*"))
    options.Methods.Add(Q2Us(methods));

  CProgressDialog dialog(parent);
  dialog.WaitMode = true;

  CHashCallbackQt callback;
  callback.ProgressDialog = &dialog;
  if (results)
    results->clear();
  callback.Results = results;

  AString errorInfo;
  const HRESULT res = dialog.Execute([&]() -> HRESULT
  {
    return HashCalc(EXTERNAL_CODECS_VARS_L censor, options, errorInfo, &callback);
  });

  if (errorMessage)
    *errorMessage = QString::fromUtf8(errorInfo.Ptr());
  return res;
}


// ---------------------------------------------------------------------------
// SplitFile -- port of UI/FileManager/PanelSplitFile.cpp
// ---------------------------------------------------------------------------

namespace {

// Verbatim from PanelSplitFile.cpp: ".001", ".002", ... with as many digits
// as the total number of volumes needs.
struct CVolSeqName
{
  UString UnchangedPart;
  UString ChangedPart;
  CVolSeqName(): ChangedPart("000") {}

  void SetNumDigits(UInt64 numVolumes)
  {
    ChangedPart = "000";
    while (numVolumes > 999)
    {
      numVolumes /= 10;
      ChangedPart.Add_Char('0');
    }
  }

  UString GetNextName()
  {
    for (int i = (int)ChangedPart.Len() - 1; i >= 0; i--)
    {
      const wchar_t c = ChangedPart[(unsigned)i];
      if (c != L'9')
      {
        ChangedPart.ReplaceOneCharAtPos((unsigned)i, (wchar_t)(c + 1));
        break;
      }
      ChangedPart.ReplaceOneCharAtPos((unsigned)i, L'0');
      if (i == 0)
        ChangedPart.InsertAtFront(L'1');
    }
    return UnchangedPart + ChangedPart;
  }
};

} // namespace


HRESULT SplitFile(QWidget *parent,
    const QString &filePath,
    const QString &volBasePath,
    UInt64 volumeSize,
    QString *errorMessage)
{
  if (volumeSize == 0)
    return E_INVALIDARG;

  const FString srcPath = Q2Fs(filePath);
  const FString basePath = Q2Fs(volBasePath);

  CProgressDialog dialog(parent);
  dialog.WaitMode = true;
  dialog.SetTitleFileName(QFileInfo(filePath).fileName());

  UString engineError;

  const HRESULT res = dialog.Execute([&]() -> HRESULT
  {
    NIO::CInFile inFile;
    if (!inFile.Open(srcPath))
      return GetLastError_noZero_HRESULT();

    UInt64 length;
    if (!inFile.GetLength(length))
      return GetLastError_noZero_HRESULT();

    dialog.Sync.Set_NumBytesTotal(length);
    const UInt64 numVolumes = (length + volumeSize - 1) / volumeSize;
    dialog.Sync.Set_NumFilesTotal(numVolumes);

    CVolSeqName seqName;
    seqName.SetNumDigits(numVolumes);

    const size_t kBufSize = (size_t)1 << 20;
    CByteBuffer buffer(kBufSize);

    NIO::COutFile outFile;
    UInt64 written = 0;
    UInt64 pos = 0;
    UInt64 numFiles = 0;
    bool fileIsOpen = false;

    for (;;)
    {
      size_t needSize = kBufSize;
      {
        const UInt64 rem = volumeSize - written;
        if (needSize > rem)
          needSize = (size_t)rem;
      }
      size_t processedSize;
      if (!inFile.ReadFull((Byte *)buffer, needSize, processedSize))
        return GetLastError_noZero_HRESULT();
      if (processedSize == 0)
        break;

      if (!fileIsOpen)
      {
        FString name = basePath;
        name.Add_Dot();
        name += us2fs(seqName.GetNextName());
        dialog.Sync.Set_FilePath(fs2us(name));
        if (!outFile.Create_NEW(name))
        {
          engineError = fs2us(name);
          return GetLastError_noZero_HRESULT();
        }
        fileIsOpen = true;
      }

      if (!outFile.WriteFull((const Byte *)buffer, processedSize))
        return GetLastError_noZero_HRESULT();

      written += processedSize;
      pos += processedSize;
      RINOK(dialog.Sync.Set_NumBytesCur(pos))

      if (written == volumeSize)
      {
        outFile.Close();
        fileIsOpen = false;
        written = 0;
        dialog.Sync.Set_NumFilesCur(++numFiles);
      }
    }

    if (fileIsOpen)
    {
      outFile.Close();
      dialog.Sync.Set_NumFilesCur(++numFiles);
    }
    return S_OK;
  });

  if (errorMessage)
    *errorMessage = Us2Q(engineError);
  return res;
}
