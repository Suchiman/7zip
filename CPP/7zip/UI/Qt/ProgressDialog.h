// Qt/ProgressDialog.h
//
// Port of UI/FileManager/ProgressDialog2.{h,cpp,rc}.
//
// CProgressSync is the same thread-shared state object as in 7-Zip: the
// worker thread writes into it under a lock, and the dialog reads from it on
// a timer. The prompts (overwrite / password) instead hop to the GUI thread
// with a blocking queued call, because they need an answer.

#ifndef ZIP7_INC_QT_PROGRESS_DIALOG_H
#define ZIP7_INC_QT_PROGRESS_DIALOG_H

#include "../../../Common/MyString.h"
#include "../Common/IFileExtractCallback.h"

#include "Z7Qt.h"

#include <QDialog>
#include <QElapsedTimer>
#include <QMutex>
#include <QThread>

#include <functional>

class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTimer;

struct CProgressMessageBoxPair
{
  UString Title;
  UString Message;
};

struct CProgressFinalMessage
{
  CProgressMessageBoxPair ErrorMessage;
  CProgressMessageBoxPair OkMessage;

  bool ThereIsMessage() const
    { return !ErrorMessage.Message.IsEmpty() || !OkMessage.Message.IsEmpty(); }
};


class CProgressSync
{
  bool _stopped;
  bool _paused;
public:
  bool _filesProgressMode;
  bool _isDir;
  UInt64 _totalBytes;
  UInt64 _completedBytes;
  UInt64 _totalFiles;
  UInt64 _curFiles;
  UInt64 _inSize;
  UInt64 _outSize;

  UString _titleFileName;
  UString _status;
  UString _filePath;

  UStringVector Messages;
  CProgressFinalMessage FinalMessage;

  mutable QMutex _cs;

  CProgressSync();

  bool Get_Stopped()          { QMutexLocker l(&_cs); return _stopped; }
  void Set_Stopped(bool val)  { QMutexLocker l(&_cs); _stopped = val; }
  bool Get_Paused()           { QMutexLocker l(&_cs); return _paused; }
  void Set_Paused(bool val)   { QMutexLocker l(&_cs); _paused = val; }
  void Set_FilesProgressMode(bool v) { QMutexLocker l(&_cs); _filesProgressMode = v; }

  // Blocks while paused, returns E_ABORT once the user cancelled.
  HRESULT CheckStop();
  void Clear_Stop_Status();

  HRESULT ScanProgress(UInt64 numFiles, UInt64 totalSize, const FString &fileName, bool isDir = false);

  HRESULT Set_NumFilesTotal(UInt64 val);
  void Set_NumBytesTotal(UInt64 val);
  void Set_NumFilesCur(UInt64 val);
  HRESULT Set_NumBytesCur(const UInt64 *val);
  HRESULT Set_NumBytesCur(UInt64 val);
  void Set_Ratio(const UInt64 *inSize, const UInt64 *outSize);

  void Set_TitleFileName(const UString &fileName);
  void Set_Status(const UString &s);
  HRESULT Set_Status2(const UString &s, const wchar_t *path, bool isDir = false);
  void Set_FilePath(const wchar_t *path, bool isDir = false);

  void AddError_Message(const wchar_t *message);
  void AddError_Message_Name(const wchar_t *message, const wchar_t *name);
  void AddError_Code_Name(HRESULT systemError, const wchar_t *name);

  bool ThereIsMessage() const { return !Messages.IsEmpty() || FinalMessage.ThereIsMessage(); }
};


// Runs one HRESULT-returning function off the GUI thread.
class CWorkerThread: public QThread
{
  Q_OBJECT
public:
  explicit CWorkerThread(QObject *parent = nullptr): QThread(parent), Result(S_OK) {}
  std::function<HRESULT()> Func;
  HRESULT Result;
protected:
  void run() override;
};


class CProgressDialog: public QDialog
{
  Q_OBJECT
public:
  explicit CProgressDialog(QWidget *parent = nullptr);
  ~CProgressDialog() override;

  CProgressSync Sync;

  // "Compressed size" and "Compression ratio" rows only make sense when
  // packing; 7-Zip hides them otherwise.
  bool CompressingMode;
  // Keep the window open after the operation so the messages stay readable.
  bool WaitMode;
  bool MessagesDisplayed;

  void SetTitleFileName(const QString &name);

  // Runs (func) in a worker thread and pumps the dialog until it finishes.
  // Returns the function's HRESULT.
  HRESULT Execute(const std::function<HRESULT()> &func);

  // ---- prompts, callable from the worker thread ----
  // Returns one of NOverwriteAnswer::EEnum.
  int AskOverwrite(const QString &existName, const FILETIME *existTime, const UInt64 *existSize,
                   const QString &newName, const FILETIME *newTime, const UInt64 *newSize);
  // Returns false when the user cancelled.
  bool AskPassword(UString &password);
  // "this archive needs N GB" -- true if the user allows it anyway.
  bool AskMemoryUse(const QString &path, quint64 requiredGb, quint64 limitGb);

public slots:
  int DoAskOverwrite(const QString &existName, QString existInfo,
                     const QString &newName, QString newInfo);
  int DoAskPassword(QString *password);
  int DoAskMemoryUse(const QString &path, qulonglong requiredGb, qulonglong limitGb);

private slots:
  void OnTimer();
  void OnBackground();
  void OnPause();
  void OnCancel();
  void OnFinished();

private:
  void UpdateStatInfo();
  void AddMessages();
  void closeEvent(QCloseEvent *e) override;
  void reject() override;

  QLabel *_elapsedVal;
  QLabel *_remainingVal;
  QLabel *_filesVal;
  QLabel *_filesTotalVal;
  QLabel *_errorsVal;
  QLabel *_totalVal;
  QLabel *_speedVal;
  QLabel *_processedVal;
  QLabel *_packedVal;
  QLabel *_ratioVal;
  QLabel *_packedLabel;
  QLabel *_ratioLabel;
  QLabel *_statusLabel;
  QLabel *_fileNameLabel;
  QProgressBar *_progressBar;
  QListWidget *_messages;
  QPushButton *_backgroundButton;
  QPushButton *_pauseButton;
  QPushButton *_cancelButton;

  QTimer *_timer;
  QElapsedTimer _elapsed;
  qint64 _elapsedPausedMs;
  qint64 _pauseStartMs;

  CWorkerThread *_thread;
  QString _titleFileName;
  unsigned _numShownMessages;
  bool _cancelWasPressed;
  bool _finished;
  UInt64 _prevPercent;
};

#endif
