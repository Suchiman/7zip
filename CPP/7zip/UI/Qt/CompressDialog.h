// Qt/CompressDialog.h -- port of UI/GUI/CompressDialog.{h,cpp,rc}

#ifndef ZIP7_INC_QT_COMPRESS_DIALOG_H
#define ZIP7_INC_QT_COMPRESS_DIALOG_H

#include "Operations.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

class CCompressDialog: public QDialog
{
  Q_OBJECT
public:
  explicit CCompressDialog(QWidget *parent = nullptr);

  // (name) is the archive path proposed by the caller, without extension;
  // it comes from ArchiveName.cpp, as in 7-Zip.
  void SetArchiveBaseName(const QString &baseNameNoExt);
  void SetCurDirPrefix(const QString &dir) { _curDirPrefix = dir; }

  NQtCompress::CInfo Info() const { return _info; }

protected:
  void accept() override;

private slots:
  void OnFormatChanged();
  void OnLevelChanged();
  void OnMethodChanged();
  void OnBrowse();
  void UpdateMemoryUsage();

private:
  void SetFormats();
  void SetLevels();
  void SetMethods();
  void SetDictionary();
  void SetOrder();
  void SetSolidBlockSize();
  void SetNumThreads();
  void SetEncryptionMethods();
  void UpdateArchiveExtension();

  int CurFormatIndex() const;         // index into CCodecs::Formats
  int StaticFormatIndex() const;      // index into the g_Formats table
  int CurMethodID() const;
  UInt32 CurLevel() const;

  QComboBox *_archive;
  QComboBox *_format;
  QComboBox *_level;
  QComboBox *_method;
  QComboBox *_dictionary;
  QComboBox *_order;
  QComboBox *_solid;
  QComboBox *_threads;
  QLabel    *_hardwareThreads;
  QLabel    *_memoryValue;
  QComboBox *_volume;
  QLineEdit *_parameters;
  QComboBox *_updateMode;
  QComboBox *_pathMode;
  QCheckBox *_deleteAfter;
  QLineEdit *_password1;
  QLineEdit *_password2;
  QCheckBox *_showPassword;
  QComboBox *_encryptionMethod;
  QCheckBox *_encryptFileNames;

  QString _curDirPrefix;
  QString _baseName;
  NQtCompress::CInfo _info;
  bool _inSetup;
};

#endif
