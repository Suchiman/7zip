// Qt/ExtractDialog.h -- port of UI/GUI/ExtractDialog.{h,cpp,rc}

#ifndef ZIP7_INC_QT_EXTRACT_DIALOG_H
#define ZIP7_INC_QT_EXTRACT_DIALOG_H

#include "Operations.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;

class CExtractDialog: public QDialog
{
  Q_OBJECT
public:
  explicit CExtractDialog(QWidget *parent = nullptr);

  void SetPath(const QString &path);
  void SetArchiveName(const QString &name);   // for the "\<name>" sub-folder box

  // Filled in when the dialog is accepted.
  CQtExtractInfo Info() const;

private slots:
  void OnBrowse();

private:
  QComboBox *_path;
  QCheckBox *_nameEnable;
  QLineEdit *_name;
  QComboBox *_pathMode;
  QCheckBox *_elimDup;
  QComboBox *_overwriteMode;
  QLineEdit *_password;
  QCheckBox *_showPassword;
};

#endif
