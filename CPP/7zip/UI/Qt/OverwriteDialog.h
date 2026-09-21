// Qt/OverwriteDialog.h -- port of UI/FileManager/OverwriteDialog.{h,cpp,rc}

#ifndef ZIP7_INC_QT_OVERWRITE_DIALOG_H
#define ZIP7_INC_QT_OVERWRITE_DIALOG_H

#include <QDialog>

class QLabel;

class COverwriteDialog: public QDialog
{
  Q_OBJECT
public:
  explicit COverwriteDialog(QWidget *parent = nullptr);

  void SetFiles(const QString &existName, const QString &existInfo,
                const QString &newName, const QString &newInfo);

  // one of NOverwriteAnswer::EEnum
  int Answer() const { return _answer; }

private:
  void Finish(int answer);

  QLabel *_oldFile;
  QLabel *_newFile;
  int _answer;
};

#endif
