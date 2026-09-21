// Qt/PasswordDialog.h -- port of UI/FileManager/PasswordDialog.{h,cpp,rc}

#ifndef ZIP7_INC_QT_PASSWORD_DIALOG_H
#define ZIP7_INC_QT_PASSWORD_DIALOG_H

#include <QDialog>

class QCheckBox;
class QLineEdit;

class CPasswordDialog: public QDialog
{
  Q_OBJECT
public:
  explicit CPasswordDialog(QWidget *parent = nullptr);
  QString Password() const;

private:
  QLineEdit *_edit;
  QCheckBox *_show;
};

#endif
