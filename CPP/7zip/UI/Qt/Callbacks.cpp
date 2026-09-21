// Qt/Callbacks.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/PropVariant.h"
#include "../../../Windows/PropVariantConv.h"

#include "../Common/OpenArchive.h"
#include "../../Common/FilePathAutoRename.h"

#include "Callbacks.h"
#include "OptionsDialog.h"

using namespace NWindows;

// The operation names 7-Zip shows in the progress status line.
static const char * const kOpNames[] =
{
    "Adding"
  , "Updating"
  , "Analyzing"
  , "Replicating"
  , "Repacking"
  , "Skipping"
  , "Deleting"
  , "Header"
};

static UString OpName(UInt32 op)
{
  if (op < Z7_ARRAY_SIZE(kOpNames))
    return GetUnicodeString(kOpNames[op]);
  return UString();
}


// ===========================================================================
// CExtractCallbackQt
// ===========================================================================

Z7_COM7F_IMF(CExtractCallbackQt::SetTotal(UInt64 total))
{
  ProgressDialog->Sync.Set_NumBytesTotal(total);
  return ProgressDialog->Sync.CheckStop();
}

Z7_COM7F_IMF(CExtractCallbackQt::SetCompleted(const UInt64 *completeValue))
{
  return ProgressDialog->Sync.Set_NumBytesCur(completeValue);
}

Z7_COM7F_IMF(CExtractCallbackQt::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize))
{
  ProgressDialog->Sync.Set_Ratio(inSize, outSize);
  return ProgressDialog->Sync.CheckStop();
}

Z7_COM7F_IMF(CExtractCallbackQt::SetNumFiles(UInt64 numFiles))
{
  return ProgressDialog->Sync.Set_NumFilesTotal(numFiles);
}

Z7_COM7F_IMF(CExtractCallbackQt::SetCurrentFilePath(const wchar_t *filePath))
{
  _currentFilePath = filePath ? filePath : L"";
  ProgressDialog->Sync.Set_FilePath(filePath);
  return ProgressDialog->Sync.CheckStop();
}

Z7_COM7F_IMF(CExtractCallbackQt::AskWrite(
    const wchar_t *srcPath, Int32 srcIsFolder,
    const FILETIME *srcTime, const UInt64 *srcSize,
    const wchar_t *destPathRequest,
    BSTR *destPathResult, Int32 *writeAnswer))
{
  COM_TRY_BEGIN
  *destPathResult = NULL;
  *writeAnswer = BoolToInt(false);

  const UString destPath (destPathRequest);
  NFile::NFind::CFileInfo destFileInfo;
  if (destFileInfo.Find(us2fs(destPath)))
  {
    if (IntToBool(srcIsFolder))
    {
      if (destFileInfo.IsDir())
      {
        *writeAnswer = BoolToInt(true);
        return StringToBstr(destPath, destPathResult);
      }
    }
    else
    {
      FILETIME destTime;
      FiTime_To_FILETIME(destFileInfo.MTime, destTime);
      const Int32 answer = ProgressDialog->AskOverwrite(
          Us2Q(destPath), &destTime, &destFileInfo.Size,
          Us2Q(UString(srcPath)), srcTime, srcSize);
      switch (answer)
      {
        case NOverwriteAnswer::kCancel:    return E_ABORT;
        case NOverwriteAnswer::kNo:        return S_OK;
        case NOverwriteAnswer::kNoToAll:   return S_OK;
        case NOverwriteAnswer::kAutoRename:
        {
          FString destPathNew = us2fs(destPath);
          if (!AutoRenamePath(destPathNew))
            return E_FAIL;
          *writeAnswer = BoolToInt(true);
          return StringToBstr(fs2us(destPathNew), destPathResult);
        }
        default: break;   // kYes / kYesToAll
      }
    }
  }
  *writeAnswer = BoolToInt(true);
  return StringToBstr(destPath, destPathResult);
  COM_TRY_END
}

Z7_COM7F_IMF(CExtractCallbackQt::ShowMessage(const wchar_t *message))
{
  return AddErrorMessage(message);
}

HRESULT CExtractCallbackQt::AddErrorMessage(const wchar_t *message)
{
  ThereAreMessageErrors = true;
  ProgressDialog->Sync.AddError_Message(message);
  return S_OK;
}

Z7_COM7F_IMF(CExtractCallbackQt::AskOverwrite(
    const wchar_t *existName, const FILETIME *existTime, const UInt64 *existSize,
    const wchar_t *newName, const FILETIME *newTime, const UInt64 *newSize,
    Int32 *answer))
{
  COM_TRY_BEGIN
  const int res = ProgressDialog->AskOverwrite(
      Us2Q(UString(existName)), existTime, existSize,
      Us2Q(UString(newName)), newTime, newSize);
  *answer = (Int32)res;
  return (res == NOverwriteAnswer::kCancel) ? E_ABORT : S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CExtractCallbackQt::PrepareOperation(const wchar_t *name, Int32 isFolder,
    Int32 askExtractMode, const UInt64 *position))
{
  UNUSED_VAR(position)
  _isFolder = IntToBool(isFolder);
  _currentFilePath = name ? name : L"";

  const char *s;
  switch (askExtractMode)
  {
    case NArchive::NExtract::NAskMode::kExtract: s = "Extracting"; break;
    case NArchive::NExtract::NAskMode::kTest:    s = "Testing"; break;
    case NArchive::NExtract::NAskMode::kSkip:    s = "Skipping"; break;
    case NArchive::NExtract::NAskMode::kReadExternal: s = "Reading"; break;
    default: s = ""; break;
  }
  return ProgressDialog->Sync.Set_Status2(GetUnicodeString(s), name, _isFolder);
}

Z7_COM7F_IMF(CExtractCallbackQt::SetOperationResult(Int32 opRes, Int32 encrypted))
{
  return ReportExtractResult(opRes, encrypted, _currentFilePath);
}

Z7_COM7F_IMF(CExtractCallbackQt::ReportExtractResult(Int32 opRes, Int32 encrypted, const wchar_t *name))
{
  if (opRes != NArchive::NExtract::NOperationResult::kOK)
  {
    UString s;
    switch (opRes)
    {
      case NArchive::NExtract::NOperationResult::kUnsupportedMethod:
        s = "Unsupported compression method"; break;
      case NArchive::NExtract::NOperationResult::kCRCError:
        s = IntToBool(encrypted) ? "CRC failed. Wrong password?" : "CRC failed"; break;
      case NArchive::NExtract::NOperationResult::kDataError:
        s = IntToBool(encrypted) ? "Data error. Wrong password?" : "Data error"; break;
      case NArchive::NExtract::NOperationResult::kUnavailable:
        s = "Unavailable data"; break;
      case NArchive::NExtract::NOperationResult::kUnexpectedEnd:
        s = "Unexpected end of data"; break;
      case NArchive::NExtract::NOperationResult::kDataAfterEnd:
        s = "There are some data after the end of the payload data"; break;
      case NArchive::NExtract::NOperationResult::kIsNotArc:
        s = "Is not archive"; break;
      case NArchive::NExtract::NOperationResult::kHeadersError:
        s = "Headers Error"; break;
      case NArchive::NExtract::NOperationResult::kWrongPassword:
        s = "Wrong password"; break;
      default:
      {
        char temp[16];
        ConvertUInt32ToString((UInt32)opRes, temp);
        s = "Error #";
        s += temp;
        break;
      }
    }
    ThereAreMessageErrors = true;
    ProgressDialog->Sync.AddError_Message_Name(s, name);
  }
  return S_OK;
}

Z7_COM7F_IMF(CExtractCallbackQt::MessageError(const wchar_t *message))
{
  return AddErrorMessage(message);
}

Z7_COM7F_IMF(CExtractCallbackQt::CryptoGetTextPassword(BSTR *password))
{
  COM_TRY_BEGIN
  if (!PasswordIsDefined)
  {
    if (!ProgressDialog->AskPassword(Password))
      return E_ABORT;
    PasswordIsDefined = true;
    PasswordWasAsked = true;
  }
  return StringToBstr(Password, password);
  COM_TRY_END
}

// The RAM budget for unpacking, from Options > Settings. 7-Zip asks before it
// allocates a dictionary larger than the limit (CMemDialog); this is the same
// question as a plain message box.
Z7_COM7F_IMF(CExtractCallbackQt::RequestMemoryUse(
    UInt32 flags, UInt32 /* indexType */, UInt32 /* index */, const wchar_t *path,
    UInt64 requiredSize, UInt64 *allowedSize, UInt32 *answerFlags))
{
  if (flags & NRequestMemoryUseFlags::k_IsReport)
    return S_OK;

  UInt64 limit = *allowedSize;
  {
    const CFmSettings &settings = GlobalFmSettings();
    if (settings.MemLimitDefined)
    {
      const UInt64 fromSettings = (UInt64)settings.MemLimitGb << 30;
      if ((flags & NRequestMemoryUseFlags::k_AllowedSize_WasForced) == 0
          || limit < fromSettings)
        limit = fromSettings;
    }
  }
  *allowedSize = limit;

  if (requiredSize <= limit)
  {
    *answerFlags = NRequestMemoryAnswerFlags::k_Allow;
    return S_OK;
  }

  *answerFlags = NRequestMemoryAnswerFlags::k_Limit_Exceeded;
  if (flags & NRequestMemoryUseFlags::k_SkipArc_IsExpected)
    *answerFlags |= NRequestMemoryAnswerFlags::k_SkipArc;

  const UInt64 requiredGb = (requiredSize + ((1u << 30) - 1)) >> 30;
  const UInt64 limitGb = (limit + ((1u << 30) - 1)) >> 30;
  if (ProgressDialog->AskMemoryUse(Us2Q(UString(path ? path : L"")), requiredGb, limitGb))
  {
    *allowedSize = requiredSize;
    *answerFlags = NRequestMemoryAnswerFlags::k_Allow;
  }
  return S_OK;
}


// ---- IExtractCallbackUI ----

HRESULT CExtractCallbackQt::BeforeOpen(const wchar_t *name, bool /* testMode */)
{
  ProgressDialog->Sync.Set_TitleFileName(name ? name : L"");
  return ProgressDialog->Sync.Set_Status2(GetUnicodeString("Opening"), name);
}

HRESULT CExtractCallbackQt::OpenResult(const CCodecs *codecs, const CArchiveLink &arcLink,
    const wchar_t *name, HRESULT result)
{
  FOR_VECTOR (level, arcLink.Arcs)
  {
    const CArcErrorInfo &error = arcLink.Arcs[level].ErrorInfo;
    if (error.ErrorFormatIndex >= 0)
      ProgressDialog->Sync.AddError_Message_Name(
          L"Can not open the file as archive", name);
    if (!error.ErrorMessage.IsEmpty())
      ProgressDialog->Sync.AddError_Message_Name(error.ErrorMessage, name);
    if (error.AreThereWarnings())
      ProgressDialog->Sync.AddError_Message_Name(
          error.WarningMessage.IsEmpty() ? UString("Warning") : error.WarningMessage, name);
  }
  UNUSED_VAR(codecs)

  if (result != S_OK)
  {
    ThereAreMessageErrors = true;
    if (result == S_FALSE)
      ProgressDialog->Sync.AddError_Message_Name(L"Can not open the file as archive", name);
    else
      ProgressDialog->Sync.AddError_Code_Name(result, name);
  }
  return S_OK;
}

HRESULT CExtractCallbackQt::ThereAreNoFiles()
{
  return ProgressDialog->Sync.Set_Status2(GetUnicodeString("There are no files"), NULL);
}

HRESULT CExtractCallbackQt::ExtractResult(HRESULT result)
{
  if (result == S_OK)
    return result;
  ThereAreMessageErrors = true;
  if (result == E_ABORT)
    return result;
  ProgressDialog->Sync.AddError_Code_Name(result, _currentFilePath);
  return S_OK;
}

HRESULT CExtractCallbackQt::SetPassword(const UString &password)
{
  PasswordIsDefined = true;
  Password = password;
  return S_OK;
}

// ---- IOpenCallbackUI ----

HRESULT CExtractCallbackQt::Open_CheckBreak()
{
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CExtractCallbackQt::Open_SetTotal(const UInt64 *files, const UInt64 *bytes)
{
  if (files)
    ProgressDialog->Sync.Set_NumFilesTotal(*files);
  if (bytes)
    ProgressDialog->Sync.Set_NumBytesTotal(*bytes);
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CExtractCallbackQt::Open_SetCompleted(const UInt64 *files, const UInt64 *bytes)
{
  if (files)
    ProgressDialog->Sync.Set_NumFilesCur(*files);
  if (bytes)
    return ProgressDialog->Sync.Set_NumBytesCur(bytes);
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CExtractCallbackQt::Open_Finished()
{
  return S_OK;
}

HRESULT CExtractCallbackQt::Open_CryptoGetTextPassword(BSTR *password)
{
  return CryptoGetTextPassword(password);
}



// ===========================================================================
// CUpdateCallbackQt
// ===========================================================================

HRESULT CUpdateCallbackQt::WriteSfx(const wchar_t *name, UInt64 /* size */)
{
  return ProgressDialog->Sync.Set_Status2(GetUnicodeString("Writing"), name);
}

HRESULT CUpdateCallbackQt::SetTotal(UInt64 size)
{
  ProgressDialog->Sync.Set_NumBytesTotal(size);
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CUpdateCallbackQt::SetCompleted(const UInt64 *completeValue)
{
  return ProgressDialog->Sync.Set_NumBytesCur(completeValue);
}

HRESULT CUpdateCallbackQt::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize)
{
  ProgressDialog->Sync.Set_Ratio(inSize, outSize);
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CUpdateCallbackQt::CheckBreak()
{
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CUpdateCallbackQt::SetNumItems(const CArcToDoStat &stat)
{
  return ProgressDialog->Sync.Set_NumFilesTotal(stat.Get_NumDataItems_Total());
}

HRESULT CUpdateCallbackQt::GetStream(const wchar_t *name, bool isDir, bool /* isAnti */, UInt32 mode)
{
  if (mode == NUpdateNotifyOp::kSkip)
    return S_OK;
  ProgressDialog->Sync.Set_NumFilesCur(NumFiles);
  return ProgressDialog->Sync.Set_Status2(OpName(mode), name, isDir);
}

HRESULT CUpdateCallbackQt::OpenFileError(const FString &path, DWORD systemError)
{
  FailedFiles.Add(path);
  ProgressDialog->Sync.AddError_Code_Name(HRESULT_FROM_WIN32(systemError), fs2us(path));
  return S_FALSE;
}

HRESULT CUpdateCallbackQt::ReadingFileError(const FString &path, DWORD systemError)
{
  ProgressDialog->Sync.AddError_Code_Name(HRESULT_FROM_WIN32(systemError), fs2us(path));
  return HRESULT_FROM_WIN32(systemError);
}

HRESULT CUpdateCallbackQt::SetOperationResult(Int32 /* opRes */)
{
  NumFiles++;
  ProgressDialog->Sync.Set_NumFilesCur(NumFiles);
  return S_OK;
}

HRESULT CUpdateCallbackQt::ReportExtractResult(Int32 opRes, Int32 isEncrypted, const wchar_t *name)
{
  if (opRes != NArchive::NExtract::NOperationResult::kOK)
    ProgressDialog->Sync.AddError_Message_Name(
        IntToBool(isEncrypted) ? L"Data error. Wrong password?" : L"Data error", name);
  return S_OK;
}

HRESULT CUpdateCallbackQt::ReportUpdateOperation(UInt32 op, const wchar_t *name, bool isDir)
{
  return ProgressDialog->Sync.Set_Status2(OpName(op), name, isDir);
}

HRESULT CUpdateCallbackQt::CryptoGetTextPassword2(Int32 *passwordIsDefined, BSTR *password)
{
  *password = NULL;
  if (!PasswordIsDefined)
  {
    if (AskPassword)
    {
      if (!ProgressDialog->AskPassword(Password))
        return E_ABORT;
      PasswordIsDefined = true;
      PasswordWasAsked = true;
    }
  }
  *passwordIsDefined = BoolToInt(PasswordIsDefined);
  return StringToBstr(Password, password);
}

HRESULT CUpdateCallbackQt::CryptoGetTextPassword(BSTR *password)
{
  if (!PasswordIsDefined)
  {
    if (!ProgressDialog->AskPassword(Password))
      return E_ABORT;
    PasswordIsDefined = true;
    PasswordWasAsked = true;
  }
  return StringToBstr(Password, password);
}

HRESULT CUpdateCallbackQt::ShowDeleteFile(const wchar_t *name, bool isDir)
{
  return ProgressDialog->Sync.Set_Status2(GetUnicodeString("Deleting"), name, isDir);
}

// ---- IDirItemsCallback ----

HRESULT CUpdateCallbackQt::ScanError(const FString &path, DWORD systemError)
{
  ProgressDialog->Sync.AddError_Code_Name(HRESULT_FROM_WIN32(systemError), fs2us(path));
  return S_OK;
}

HRESULT CUpdateCallbackQt::ScanProgress(const CDirItemsStat &st, const FString &path, bool isDir)
{
  return ProgressDialog->Sync.ScanProgress(st.NumDirs + st.NumFiles + st.NumAltStreams,
      st.GetTotalBytes(), path, isDir);
}

// ---- IUpdateCallbackUI2 ----

HRESULT CUpdateCallbackQt::OpenResult(const CCodecs * /* codecs */, const CArchiveLink & /* arcLink */,
    const wchar_t *name, HRESULT result)
{
  if (result != S_OK)
    ProgressDialog->Sync.AddError_Code_Name(result, name);
  return S_OK;
}

HRESULT CUpdateCallbackQt::StartScanning()
{
  ProgressDialog->Sync.Set_Status(GetUnicodeString("Scanning"));
  return S_OK;
}

HRESULT CUpdateCallbackQt::FinishScanning(const CDirItemsStat & /* st */)
{
  return S_OK;
}

HRESULT CUpdateCallbackQt::StartOpenArchive(const wchar_t *name)
{
  return ProgressDialog->Sync.Set_Status2(GetUnicodeString("Opening"), name);
}

HRESULT CUpdateCallbackQt::StartArchive(const wchar_t *name, bool /* updating */)
{
  ProgressDialog->Sync.Set_TitleFileName(name ? name : L"");
  return S_OK;
}

HRESULT CUpdateCallbackQt::FinishArchive(const CFinishArchiveStat & /* st */)
{
  return S_OK;
}

HRESULT CUpdateCallbackQt::DeletingAfterArchiving(const FString &path, bool isDir)
{
  return ProgressDialog->Sync.Set_Status2(GetUnicodeString("Removing"), fs2us(path), isDir);
}

HRESULT CUpdateCallbackQt::FinishDeletingAfterArchiving()
{
  return S_OK;
}

HRESULT CUpdateCallbackQt::MoveArc_Start(const wchar_t *srcTempPath, const wchar_t * /* destFinalPath */,
    UInt64 /* size */, Int32 /* updateMode */)
{
  return ProgressDialog->Sync.Set_Status2(GetUnicodeString("Moving"), srcTempPath);
}

HRESULT CUpdateCallbackQt::MoveArc_Progress(UInt64 total, UInt64 current)
{
  ProgressDialog->Sync.Set_NumBytesTotal(total);
  return ProgressDialog->Sync.Set_NumBytesCur(current);
}

HRESULT CUpdateCallbackQt::MoveArc_Finish()
{
  return S_OK;
}

// ---- IOpenCallbackUI (the update side reuses the same questions) ----

HRESULT CUpdateCallbackQt::Open_CheckBreak()
{
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CUpdateCallbackQt::Open_SetTotal(const UInt64 * /* files */, const UInt64 * /* bytes */)
{
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CUpdateCallbackQt::Open_SetCompleted(const UInt64 * /* files */, const UInt64 * /* bytes */)
{
  return ProgressDialog->Sync.CheckStop();
}

HRESULT CUpdateCallbackQt::Open_Finished()
{
  return S_OK;
}

HRESULT CUpdateCallbackQt::Open_CryptoGetTextPassword(BSTR *password)
{
  return CryptoGetTextPassword(password);
}

