// Qt/Callbacks.h
//
// The Qt counterparts of
//   UI/FileManager/ExtractCallback.{h,cpp}  (CExtractCallbackImp)
//   UI/GUI/UpdateCallbackGUI.{h,cpp}        (CUpdateCallbackGUI)
// Both simply forward progress into CProgressSync and route the questions
// (overwrite / password) to the progress dialog on the GUI thread.

#ifndef ZIP7_INC_QT_CALLBACKS_H
#define ZIP7_INC_QT_CALLBACKS_H

#include "../../../Common/MyCom.h"
#include "../../IPassword.h"

#include "../Common/ArchiveExtractCallback.h"
#include "../Common/ArchiveOpenCallback.h"
#include "../Common/IFileExtractCallback.h"
#include "../Common/Update.h"
#include "../../Archive/IArchive.h"
#include "../FileManager/IFolder.h"

#include "ProgressDialog.h"

// ---------------------------------------------------------------------------
// extract / test
// ---------------------------------------------------------------------------

class CExtractCallbackQt Z7_final:
  public IFolderArchiveExtractCallback,
  public IFolderArchiveExtractCallback2,
  public IExtractCallbackUI,
  public IOpenCallbackUI,
  public IFolderOperationsExtractCallback,
  public ICompressProgressInfo,
  public IArchiveRequestMemoryUseCallback,
  public ICryptoGetTextPassword,
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IFolderArchiveExtractCallback)
    Z7_COM_QI_ENTRY(IFolderArchiveExtractCallback2)
    Z7_COM_QI_ENTRY(IFolderOperationsExtractCallback)
    Z7_COM_QI_ENTRY(ICompressProgressInfo)
    Z7_COM_QI_ENTRY(IArchiveRequestMemoryUseCallback)
    Z7_COM_QI_ENTRY(ICryptoGetTextPassword)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_IMP(IExtractCallbackUI)
  Z7_IFACE_IMP(IOpenCallbackUI)
  Z7_IFACE_COM7_IMP(IProgress)
  Z7_IFACE_COM7_IMP(IFolderArchiveExtractCallback)
  Z7_IFACE_COM7_IMP(IFolderArchiveExtractCallback2)
  Z7_IFACE_COM7_IMP(IFolderOperationsExtractCallback)
  Z7_IFACE_COM7_IMP(ICompressProgressInfo)
  Z7_IFACE_COM7_IMP(IArchiveRequestMemoryUseCallback)
  Z7_IFACE_COM7_IMP(ICryptoGetTextPassword)

public:
  CProgressDialog *ProgressDialog;
  bool PasswordIsDefined;
  bool PasswordWasAsked;
  bool MultiArcMode;
  bool ThereAreMessageErrors;
  bool TestMode;
  UString Password;

  CExtractCallbackQt():
      ProgressDialog(NULL),
      PasswordIsDefined(false),
      PasswordWasAsked(false),
      MultiArcMode(false),
      ThereAreMessageErrors(false),
      TestMode(false),
      _isFolder(false),
      _totalFilesDefined(false),
      _numFiles(0)
      {}

  void Init() { PasswordIsDefined = false; PasswordWasAsked = false; ThereAreMessageErrors = false; }

private:
  UString _currentFilePath;
  bool _isFolder;
  bool _totalFilesDefined;
  UInt64 _numFiles;

  HRESULT AddErrorMessage(const wchar_t *message);
};


// ---------------------------------------------------------------------------
// add / update
// ---------------------------------------------------------------------------

class CUpdateCallbackQt Z7_final:
  public IOpenCallbackUI,
  public IUpdateCallbackUI2
{
  Z7_IFACE_IMP(IOpenCallbackUI)
  Z7_IFACE_IMP(IUpdateCallbackUI)
  Z7_IFACE_IMP(IDirItemsCallback)
  Z7_IFACE_IMP(IUpdateCallbackUI2)

public:
  CProgressDialog *ProgressDialog;
  bool AskPassword;
  bool PasswordIsDefined;
  bool PasswordWasAsked;
  UString Password;
  UInt64 NumFiles;
  FStringVector FailedFiles;

  CUpdateCallbackQt():
      ProgressDialog(NULL),
      AskPassword(false),
      PasswordIsDefined(false),
      PasswordWasAsked(false),
      NumFiles(0)
      {}

  void Init() { NumFiles = 0; FailedFiles.Clear(); }
};

#endif
