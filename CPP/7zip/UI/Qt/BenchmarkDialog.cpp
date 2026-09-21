// Qt/BenchmarkDialog.cpp

#include "StdAfx.h"

#include "../../../Common/StringConvert.h"
#include "../../../Windows/System.h"

#include "../Common/Bench.h"

void GetCpuName_MultiLine(AString &s, AString &registers);

#include "BenchmarkDialog.h"
#include "Codecs.h"
#include "Z7Qt.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

using namespace NWindows;

namespace {

class CBenchPrintCallback Z7_final: public IBenchPrintCallback
{
public:
  CBenchmarkDialog *Dialog;
  AString Line;

  CBenchPrintCallback(): Dialog(NULL) {}

  void Print(const char *s) Z7_override { Line += s; }
  void NewLine() Z7_override
  {
    Dialog->AppendText(QString::fromUtf8(Line.Ptr()));
    Line.Empty();
  }
  HRESULT CheckBreak() Z7_override
  {
    return Dialog->StopRequested() ? E_ABORT : S_OK;
  }
};

class CBenchThread: public QThread
{
public:
  CBenchmarkDialog *Dialog;
  UInt32 NumThreads;
  UInt32 DictionaryLog;
  HRESULT Result;

  CBenchThread(QObject *parent): QThread(parent), Dialog(NULL),
      NumThreads(1), DictionaryLog(0), Result(S_OK) {}

protected:
  void run() override
  {
    CBenchPrintCallback printer;
    printer.Dialog = Dialog;

    CObjectVector<CProperty> props;
    {
      CProperty p;
      p.Name = "mt";
      p.Value.Add_UInt32(NumThreads);
      props.Add(p);
    }
    if (DictionaryLog != 0)
    {
      CProperty p;
      p.Name = "d";
      p.Value.Add_UInt32(DictionaryLog);
      props.Add(p);
    }

    CCodecs *codecs = GetCodecs();
    if (!codecs)
    {
      Result = E_FAIL;
      return;
    }
    try
    {
      Result = Bench(EXTERNAL_CODECS_VARS_L
          &printer, NULL, props, 1, /* numIterations */
          false /* multiDict */);
    }
    catch (...) { Result = E_FAIL; }
    if (!printer.Line.IsEmpty())
      printer.NewLine();
  }
};

} // namespace


CBenchmarkDialog::CBenchmarkDialog(QWidget *parent):
    QDialog(parent), _thread(nullptr), _stop(false)
{
  setWindowTitle(tr("Benchmark"));
  resize(760, 520);

  _dictionary = new QComboBox;
  for (unsigned i = 20; i <= 31; i++)
    _dictionary->addItem(QString::number(1u << (i - 20)) + QStringLiteral(" MB"), i);
  _dictionary->setCurrentIndex(4);   // 16 MB, 7-Zip's default

  _threads = new QComboBox;
  const UInt32 numHardwareThreads = NSystem::GetNumberOfProcessors();
  for (UInt32 i = 1; i <= numHardwareThreads * 2 && i <= 256; i++)
    _threads->addItem(QString::number(i), i);
  _threads->setCurrentIndex((int)numHardwareThreads - 1);

  _output = new QPlainTextEdit;
  _output->setReadOnly(true);
  QFont f(QStringLiteral("monospace"));
  f.setStyleHint(QFont::TypeWriter);
  _output->setFont(f);

  _startButton = new QPushButton(tr("&Restart"));
  _stopButton = new QPushButton(tr("&Stop"));
  _stopButton->setEnabled(false);

  QGridLayout *top = new QGridLayout;
  top->addWidget(MakeLabel(tr("&Dictionary size:"), _dictionary), 0, 0);
  top->addWidget(_dictionary, 0, 1);
  top->addWidget(MakeLabel(tr("Number of CPU &threads:"), _threads), 0, 2);
  top->addWidget(_threads, 0, 3);
  top->setColumnStretch(4, 1);

  QHBoxLayout *buttons = new QHBoxLayout;
  buttons->addWidget(_startButton);
  buttons->addWidget(_stopButton);
  buttons->addStretch(1);
  QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close);
  buttons->addWidget(box);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addLayout(top);
  main->addWidget(_output, 1);
  main->addLayout(buttons);

  connect(_startButton, &QPushButton::clicked, this, &CBenchmarkDialog::OnStart);
  connect(_stopButton, &QPushButton::clicked, this, &CBenchmarkDialog::OnStop);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  _timer = new QTimer(this);
  _timer->setInterval(100);
  connect(_timer, &QTimer::timeout, this, &CBenchmarkDialog::OnTimer);

  // The header 7-Zip prints before the measurements.
  {
    AString s1, s2;
    GetSysInfo(s1, s2);
    AString cpu, cpuRegisters;
    GetCpuName_MultiLine(cpu, cpuRegisters);
    _output->appendPlainText(QString::fromUtf8(s1.Ptr()));
    if (!s2.IsEmpty())
      _output->appendPlainText(QString::fromUtf8(s2.Ptr()));
    _output->appendPlainText(QString::fromUtf8(cpu.Ptr()));
    _output->appendPlainText(QString());
  }
}

CBenchmarkDialog::~CBenchmarkDialog()
{
  if (_thread)
  {
    OnStop();
    _thread->wait();
  }
}

void CBenchmarkDialog::AppendText(const QString &s)
{
  QMutexLocker lock(&_mutex);
  _pending += s;
  _pending += QLatin1Char('\n');
}

bool CBenchmarkDialog::StopRequested()
{
  QMutexLocker lock(&_mutex);
  return _stop;
}

void CBenchmarkDialog::OnStart()
{
  if (_thread)
    return;
  {
    QMutexLocker lock(&_mutex);
    _stop = false;
    _pending.clear();
  }
  _output->appendPlainText(tr("--- Benchmark started ---"));

  CBenchThread *t = new CBenchThread(this);
  t->Dialog = this;
  t->NumThreads = (UInt32)_threads->currentData().toUInt();
  t->DictionaryLog = (UInt32)_dictionary->currentData().toUInt();
  _thread = t;
  connect(t, &QThread::finished, this, &CBenchmarkDialog::OnFinished);

  _startButton->setEnabled(false);
  _stopButton->setEnabled(true);
  _timer->start();
  t->start();
}

void CBenchmarkDialog::OnStop()
{
  QMutexLocker lock(&_mutex);
  _stop = true;
}

void CBenchmarkDialog::OnTimer()
{
  QString text;
  {
    QMutexLocker lock(&_mutex);
    text = _pending;
    _pending.clear();
  }
  if (text.isEmpty())
    return;
  while (text.endsWith(QLatin1Char('\n')))
    text.chop(1);
  _output->appendPlainText(text);
}

void CBenchmarkDialog::OnFinished()
{
  OnTimer();
  _timer->stop();
  _startButton->setEnabled(true);
  _stopButton->setEnabled(false);
  if (_thread)
  {
    _thread->deleteLater();
    _thread = nullptr;
  }
  _output->appendPlainText(tr("--- Benchmark finished ---"));
}
