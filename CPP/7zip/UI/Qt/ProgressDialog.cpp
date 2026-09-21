// Qt/ProgressDialog.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../PropID.h"

#include "ProgressDialog.h"
#include "OverwriteDialog.h"
#include "PasswordDialog.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

using namespace NWindows;

// ---------------------------------------------------------------------------
// CProgressSync (port of ProgressDialog2.cpp)
// ---------------------------------------------------------------------------

CProgressSync::CProgressSync():
    _stopped(false), _paused(false),
    _filesProgressMode(true), _isDir(false),
    _totalBytes((UInt64)(Int64)-1), _completedBytes(0),
    _totalFiles((UInt64)(Int64)-1), _curFiles(0),
    _inSize((UInt64)(Int64)-1), _outSize((UInt64)(Int64)-1)
{}

#define PROGRESS_CHECK_STOP \
  { QMutexLocker l(&_cs); if (_stopped) return E_ABORT; }

HRESULT CProgressSync::CheckStop()
{
  for (;;)
  {
    {
      QMutexLocker l(&_cs);
      if (_stopped)
        return E_ABORT;
      if (!_paused)
        return S_OK;
    }
    QThread::msleep(100);
  }
}

void CProgressSync::Clear_Stop_Status()
{
  QMutexLocker l(&_cs);
  _stopped = false;
}

HRESULT CProgressSync::ScanProgress(UInt64 numFiles, UInt64 totalSize, const FString &fileName, bool isDir)
{
  {
    QMutexLocker l(&_cs);
    _totalFiles = numFiles;
    _totalBytes = totalSize;
    _filePath = fs2us(fileName);
    _isDir = isDir;
    _filesProgressMode = false;
  }
  return CheckStop();
}

HRESULT CProgressSync::Set_NumFilesTotal(UInt64 val)
{
  {
    QMutexLocker l(&_cs);
    _totalFiles = val;
  }
  return CheckStop();
}

void CProgressSync::Set_NumBytesTotal(UInt64 val)
{
  QMutexLocker l(&_cs);
  _totalBytes = val;
}

void CProgressSync::Set_NumFilesCur(UInt64 val)
{
  QMutexLocker l(&_cs);
  _curFiles = val;
}

HRESULT CProgressSync::Set_NumBytesCur(const UInt64 *val)
{
  {
    QMutexLocker l(&_cs);
    if (val)
      _completedBytes = *val;
  }
  return CheckStop();
}

HRESULT CProgressSync::Set_NumBytesCur(UInt64 val)
{
  {
    QMutexLocker l(&_cs);
    _completedBytes = val;
  }
  return CheckStop();
}

void CProgressSync::Set_Ratio(const UInt64 *inSize, const UInt64 *outSize)
{
  QMutexLocker l(&_cs);
  if (inSize)
    _inSize = *inSize;
  if (outSize)
    _outSize = *outSize;
}

void CProgressSync::Set_TitleFileName(const UString &fileName)
{
  QMutexLocker l(&_cs);
  _titleFileName = fileName;
}

void CProgressSync::Set_Status(const UString &s)
{
  QMutexLocker l(&_cs);
  _status = s;
}

HRESULT CProgressSync::Set_Status2(const UString &s, const wchar_t *path, bool isDir)
{
  {
    QMutexLocker l(&_cs);
    _status = s;
    _filePath = path ? path : L"";
    _isDir = isDir;
  }
  return CheckStop();
}

void CProgressSync::Set_FilePath(const wchar_t *path, bool isDir)
{
  QMutexLocker l(&_cs);
  _filePath = path ? path : L"";
  _isDir = isDir;
}

void CProgressSync::AddError_Message(const wchar_t *message)
{
  QMutexLocker l(&_cs);
  Messages.Add(message);
}

void CProgressSync::AddError_Message_Name(const wchar_t *message, const wchar_t *name)
{
  UString s;
  if (name && *name)
    s += name;
  if (message && *message)
  {
    if (!s.IsEmpty())
      s.Add_LF();
    s += message;
  }
  AddError_Message(s);
}

void CProgressSync::AddError_Code_Name(HRESULT systemError, const wchar_t *name)
{
  UString s = NError::MyFormatMessage(systemError);
  if (systemError == 0)
    s = "Error";
  AddError_Message_Name(s, name);
}


// ---------------------------------------------------------------------------
// CWorkerThread
// ---------------------------------------------------------------------------

void CWorkerThread::run()
{
  Result = E_FAIL;
  if (Func)
  {
    try { Result = Func(); }
    catch (...) { Result = E_FAIL; }
  }
}


// ---------------------------------------------------------------------------
// CProgressDialog
// ---------------------------------------------------------------------------

static QLabel *MakeValueLabel()
{
  QLabel *l = new QLabel;
  l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  l->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return l;
}

CProgressDialog::CProgressDialog(QWidget *parent):
    QDialog(parent),
    CompressingMode(false),
    WaitMode(false),
    MessagesDisplayed(false),
    _elapsedPausedMs(0),
    _pauseStartMs(0),
    _thread(nullptr),
    _numShownMessages(0),
    _cancelWasPressed(false),
    _finished(false),
    _prevPercent((UInt64)(Int64)-1)
{
  setWindowTitle(tr("Progress"));
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  resize(560, 400);

  // The two stat columns of 7-Zip's progress window, in the same order.
  QGridLayout *grid = new QGridLayout;
  int r = 0;
  grid->addWidget(new QLabel(tr("Elapsed time:")), r, 0);
  grid->addWidget(_elapsedVal = MakeValueLabel(), r, 1);
  grid->addWidget(new QLabel(tr("Total size:")), r, 2);
  grid->addWidget(_totalVal = MakeValueLabel(), r, 3);
  r++;
  grid->addWidget(new QLabel(tr("Remaining time:")), r, 0);
  grid->addWidget(_remainingVal = MakeValueLabel(), r, 1);
  grid->addWidget(new QLabel(tr("Speed:")), r, 2);
  grid->addWidget(_speedVal = MakeValueLabel(), r, 3);
  r++;
  grid->addWidget(new QLabel(tr("Files:")), r, 0);
  grid->addWidget(_filesVal = MakeValueLabel(), r, 1);
  grid->addWidget(new QLabel(tr("Processed:")), r, 2);
  grid->addWidget(_processedVal = MakeValueLabel(), r, 3);
  r++;
  grid->addWidget(_filesTotalVal = MakeValueLabel(), r, 1);
  grid->addWidget(_packedLabel = new QLabel(tr("Compressed size:")), r, 2);
  grid->addWidget(_packedVal = MakeValueLabel(), r, 3);
  r++;
  grid->addWidget(new QLabel(tr("Errors:")), r, 0);
  grid->addWidget(_errorsVal = MakeValueLabel(), r, 1);
  grid->addWidget(_ratioLabel = new QLabel(tr("Compression ratio:")), r, 2);
  grid->addWidget(_ratioVal = MakeValueLabel(), r, 3);
  grid->setColumnStretch(1, 1);
  grid->setColumnStretch(3, 1);

  _statusLabel = new QLabel;
  _statusLabel->setTextFormat(Qt::PlainText);

  _fileNameLabel = new QLabel;
  _fileNameLabel->setTextFormat(Qt::PlainText);
  _fileNameLabel->setWordWrap(true);
  _fileNameLabel->setMinimumHeight(32);
  _fileNameLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  _fileNameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

  _progressBar = new QProgressBar;
  _progressBar->setRange(0, 1000);
  _progressBar->setValue(0);

  _messages = new QListWidget;
  _messages->setSelectionMode(QAbstractItemView::ExtendedSelection);

  _backgroundButton = new QPushButton(tr("&Background"));
  _pauseButton = new QPushButton(tr("&Pause"));
  _cancelButton = new QPushButton(tr("Cancel"));
  _backgroundButton->setDefault(true);

  QHBoxLayout *buttons = new QHBoxLayout;
  buttons->addStretch(1);
  buttons->addWidget(_backgroundButton);
  buttons->addWidget(_pauseButton);
  buttons->addWidget(_cancelButton);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addLayout(grid);
  main->addWidget(_statusLabel);
  main->addWidget(_fileNameLabel);
  main->addWidget(_progressBar);
  main->addWidget(_messages, 1);
  main->addLayout(buttons);

  connect(_backgroundButton, &QPushButton::clicked, this, &CProgressDialog::OnBackground);
  connect(_pauseButton, &QPushButton::clicked, this, &CProgressDialog::OnPause);
  connect(_cancelButton, &QPushButton::clicked, this, &CProgressDialog::OnCancel);

  _timer = new QTimer(this);
  _timer->setInterval(100);
  connect(_timer, &QTimer::timeout, this, &CProgressDialog::OnTimer);
}

CProgressDialog::~CProgressDialog()
{
  if (_thread)
  {
    Sync.Set_Stopped(true);
    _thread->wait();
  }
}

void CProgressDialog::SetTitleFileName(const QString &name)
{
  _titleFileName = name;
}


static QString FormatTime(qint64 ms)
{
  if (ms < 0)
    return QString();
  const qint64 sec = ms / 1000;
  return QString::asprintf("%02d:%02d:%02d",
      (int)(sec / 3600), (int)((sec / 60) % 60), (int)(sec % 60));
}


HRESULT CProgressDialog::Execute(const std::function<HRESULT()> &func)
{
  _packedLabel->setVisible(CompressingMode);
  _packedVal->setVisible(CompressingMode);
  _ratioLabel->setVisible(CompressingMode);
  _ratioVal->setVisible(CompressingMode);

  _thread = new CWorkerThread(this);
  _thread->Func = func;
  connect(_thread, &QThread::finished, this, &CProgressDialog::OnFinished);

  _elapsed.start();
  _timer->start();
  _thread->start();

  // Modal until the worker is done (or the user sends it to the background).
  exec();

  _timer->stop();
  if (_thread)
  {
    Sync.Set_Stopped(true);
    _thread->wait();
  }
  const HRESULT res = _thread ? _thread->Result : E_FAIL;
  return res;
}


void CProgressDialog::OnFinished()
{
  _finished = true;
  OnTimer();          // one last refresh with the final numbers
  AddMessages();

  if (WaitMode && Sync.ThereIsMessage())
  {
    // Keep the window up so the error list can be read, like 7-Zip does.
    _backgroundButton->setEnabled(false);
    _pauseButton->setEnabled(false);
    _cancelButton->setText(tr("&Close"));
    _progressBar->setValue(_progressBar->maximum());
    return;
  }
  MessagesDisplayed = true;
  accept();
}


void CProgressDialog::UpdateStatInfo()
{
  UInt64 total, completed, totalFiles, curFiles, inSize, outSize;
  UString status, filePath;
  unsigned numMessages;
  bool paused;
  {
    QMutexLocker l(&Sync._cs);
    total = Sync._totalBytes;
    completed = Sync._completedBytes;
    totalFiles = Sync._totalFiles;
    curFiles = Sync._curFiles;
    inSize = Sync._inSize;
    outSize = Sync._outSize;
    status = Sync._status;
    filePath = Sync._filePath;
    numMessages = Sync.Messages.Size();
  }
  paused = Sync.Get_Paused();

  const qint64 elapsedMs = _elapsed.elapsed() - _elapsedPausedMs
      - (paused && _pauseStartMs ? _elapsed.elapsed() - _pauseStartMs : 0);

  _elapsedVal->setText(FormatTime(elapsedMs));

  const bool totalDefined = (total != (UInt64)(Int64)-1);
  _totalVal->setText(totalDefined ? NumberToStringGrouped(total) : QString());
  _processedVal->setText(NumberToStringGrouped(completed));

  if (totalFiles != (UInt64)(Int64)-1)
    _filesTotalVal->setText(QStringLiteral("/ ") + NumberToStringGrouped(totalFiles));
  else
    _filesTotalVal->clear();
  _filesVal->setText(NumberToStringGrouped(curFiles));
  _errorsVal->setText(numMessages ? NumberToStringGrouped(numMessages) : QString());

  if (elapsedMs > 500)
  {
    const UInt64 speed = (UInt64)((double)completed * 1000.0 / (double)elapsedMs);
    _speedVal->setText(SizeToStringShort(speed) + QStringLiteral("/s"));
    if (totalDefined && completed && total >= completed)
    {
      const qint64 remain = (qint64)((double)elapsedMs *
          ((double)(total - completed) / (double)completed));
      _remainingVal->setText(FormatTime(remain));
    }
  }

  if (CompressingMode)
  {
    if (outSize != (UInt64)(Int64)-1)
      _packedVal->setText(NumberToStringGrouped(outSize));
    if (inSize != (UInt64)(Int64)-1 && inSize != 0 && outSize != (UInt64)(Int64)-1)
      _ratioVal->setText(QString::number((qulonglong)(outSize * 100 / inSize)) + QLatin1Char('%'));
  }

  _statusLabel->setText(Us2Q(status));
  _fileNameLabel->setText(Us2Q(filePath));

  if (totalDefined && total != 0)
  {
    const UInt64 permille = completed >= total ? 1000 : completed * 1000 / total;
    if (permille != _prevPercent)
    {
      _prevPercent = permille;
      _progressBar->setRange(0, 1000);
      _progressBar->setValue((int)permille);
    }
  }
  else
    _progressBar->setRange(0, 0);   // busy indicator

  // Title, the way 7-Zip shows "37% file.7z".
  QString title;
  if (totalDefined && total != 0)
    title = QString::number((qulonglong)(completed * 100 / total)) + QStringLiteral("% ");
  if (!_titleFileName.isEmpty())
    title += _titleFileName + QStringLiteral(" ");
  title += tr("Progress");
  if (paused)
    title = tr("Paused") + QStringLiteral(" ") + title;
  setWindowTitle(title);
}

void CProgressDialog::AddMessages()
{
  UStringVector messages;
  {
    QMutexLocker l(&Sync._cs);
    for (unsigned i = _numShownMessages; i < Sync.Messages.Size(); i++)
      messages.Add(Sync.Messages[i]);
    _numShownMessages = Sync.Messages.Size();
  }
  FOR_VECTOR (i, messages)
    _messages->addItem(Us2Q(messages[i]));
  if (!messages.IsEmpty())
    _messages->scrollToBottom();
}

void CProgressDialog::OnTimer()
{
  UpdateStatInfo();
  AddMessages();
}

void CProgressDialog::OnBackground()
{
  // 7-Zip hides the window and lets the operation run on; closing the dialog
  // here has the same effect because the worker keeps running.
  hide();
}

void CProgressDialog::OnPause()
{
  const bool paused = !Sync.Get_Paused();
  Sync.Set_Paused(paused);
  if (paused)
    _pauseStartMs = _elapsed.elapsed();
  else if (_pauseStartMs)
  {
    _elapsedPausedMs += _elapsed.elapsed() - _pauseStartMs;
    _pauseStartMs = 0;
  }
  _pauseButton->setText(paused ? tr("&Continue") : tr("&Pause"));
}

void CProgressDialog::OnCancel()
{
  if (_finished)
  {
    MessagesDisplayed = true;
    accept();
    return;
  }
  _cancelWasPressed = true;
  Sync.Set_Stopped(true);
}

void CProgressDialog::reject()
{
  OnCancel();
}

void CProgressDialog::closeEvent(QCloseEvent *e)
{
  if (_finished)
  {
    MessagesDisplayed = true;
    e->accept();
    return;
  }
  OnCancel();
  e->ignore();
}


// ---------------------------------------------------------------------------
// prompts
// ---------------------------------------------------------------------------

static QString FileInfoText(const FILETIME *ft, const UInt64 *size)
{
  QString s;
  if (size)
    s += QObject::tr("%1 bytes").arg(NumberToStringGrouped(*size));
  if (ft)
  {
    NWindows::NCOM::CPropVariant prop;
    prop = *ft;
    const QString t = PropToString(prop, kpidMTime);
    if (!t.isEmpty())
    {
      if (!s.isEmpty())
        s += QStringLiteral("\n");
      s += t;
    }
  }
  return s;
}

int CProgressDialog::AskOverwrite(
    const QString &existName, const FILETIME *existTime, const UInt64 *existSize,
    const QString &newName, const FILETIME *newTime, const UInt64 *newSize)
{
  const QString existInfo = FileInfoText(existTime, existSize);
  const QString newInfo = FileInfoText(newTime, newSize);
  int answer = NOverwriteAnswer::kCancel;
  QMetaObject::invokeMethod(this, "DoAskOverwrite", Qt::BlockingQueuedConnection,
      Q_RETURN_ARG(int, answer),
      Q_ARG(QString, existName), Q_ARG(QString, existInfo),
      Q_ARG(QString, newName), Q_ARG(QString, newInfo));
  return answer;
}

int CProgressDialog::DoAskOverwrite(const QString &existName, QString existInfo,
    const QString &newName, QString newInfo)
{
  COverwriteDialog dlg(this);
  dlg.SetFiles(existName, existInfo, newName, newInfo);
  const bool wasVisible = isVisible();
  if (!wasVisible)
    show();
  dlg.exec();
  return dlg.Answer();
}

bool CProgressDialog::AskPassword(UString &password)
{
  QString pw;
  int ok = 0;
  QMetaObject::invokeMethod(this, "DoAskPassword", Qt::BlockingQueuedConnection,
      Q_RETURN_ARG(int, ok), Q_ARG(QString *, &pw));
  if (!ok)
    return false;
  password = Q2Us(pw);
  return true;
}

bool CProgressDialog::AskMemoryUse(const QString &path, quint64 requiredGb, quint64 limitGb)
{
  int answer = 0;
  QMetaObject::invokeMethod(this, "DoAskMemoryUse", Qt::BlockingQueuedConnection,
      Q_RETURN_ARG(int, answer), Q_ARG(QString, path),
      Q_ARG(qulonglong, (qulonglong)requiredGb), Q_ARG(qulonglong, (qulonglong)limitGb));
  return answer != 0;
}

int CProgressDialog::DoAskMemoryUse(const QString &path, qulonglong requiredGb,
    qulonglong limitGb)
{
  if (!isVisible())
    show();
  QString text = tr("The archive needs %1 GB of RAM to unpack, but the limit "
      "is set to %2 GB.").arg(requiredGb).arg(limitGb);
  if (!path.isEmpty())
    text += QStringLiteral("\n\n") + path;
  text += QStringLiteral("\n\n") + tr("Allow the larger amount of memory?");
  const QMessageBox::StandardButton res = QMessageBox::question(this,
      tr("7-Zip"), text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  return res == QMessageBox::Yes ? 1 : 0;
}

int CProgressDialog::DoAskPassword(QString *password)
{
  CPasswordDialog dlg(this);
  if (!isVisible())
    show();
  if (dlg.exec() != QDialog::Accepted)
    return 0;
  *password = dlg.Password();
  return 1;
}
