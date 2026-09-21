// Qt/BenchmarkDialog.h -- port of UI/GUI/BenchmarkDialog.{h,cpp,rc}
//
// Runs UI/Common/Bench.cpp, the same benchmark the "7zz b" command uses, and
// streams its output into the dialog.

#ifndef ZIP7_INC_QT_BENCHMARK_DIALOG_H
#define ZIP7_INC_QT_BENCHMARK_DIALOG_H

#include <QDialog>
#include <QMutex>

class QComboBox;
class QPlainTextEdit;
class QPushButton;
class QThread;
class QTimer;

class CBenchmarkDialog: public QDialog
{
  Q_OBJECT
public:
  explicit CBenchmarkDialog(QWidget *parent = nullptr);
  ~CBenchmarkDialog() override;

  // called from the benchmark thread
  void AppendText(const QString &s);
  bool StopRequested();

private slots:
  void OnStart();
  void OnStop();
  void OnTimer();
  void OnFinished();

private:
  QComboBox *_dictionary;
  QComboBox *_threads;
  QPlainTextEdit *_output;
  QPushButton *_startButton;
  QPushButton *_stopButton;
  QTimer *_timer;
  QThread *_thread;

  QMutex _mutex;
  QString _pending;
  bool _stop;
};

#endif
