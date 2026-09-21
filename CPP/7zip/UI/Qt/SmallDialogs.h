// Qt/SmallDialogs.h
//
// Ports of the small file-manager dialogs:
//   ComboDialog.rc    -> CComboDialog      (Create Folder, Rename, ...)
//   CopyDialog.rc     -> CCopyDialog       (Copy to / Move to)
//   ListViewDialog.rc -> CListViewDialog   (properties, checksum results)
//   SplitDialog.rc    -> CSplitDialog
//   AboutDialog.rc    -> CAboutDialog
//   MessagesDialog.rc -> CMessagesDialog

#ifndef ZIP7_INC_QT_SMALL_DIALOGS_H
#define ZIP7_INC_QT_SMALL_DIALOGS_H

#include <QDialog>
#include <QStringList>

class QComboBox;
class QLabel;
class QListWidget;
class QTreeWidget;


class CComboDialog: public QDialog
{
  Q_OBJECT
public:
  CComboDialog(QWidget *parent, const QString &title, const QString &staticText,
               const QString &value, const QStringList &history = QStringList());
  QString Value() const;

private:
  QComboBox *_combo;
};


class CCopyDialog: public QDialog
{
  Q_OBJECT
public:
  CCopyDialog(QWidget *parent, const QString &title, const QString &staticText,
              const QString &value, const QString &info);
  QString Value() const;

private slots:
  void OnBrowse();

private:
  QComboBox *_combo;
  QString _title;
};


class CListViewDialog: public QDialog
{
  Q_OBJECT
public:
  // Two-column name/value list, as 7-Zip's properties window shows it.
  CListViewDialog(QWidget *parent, const QString &title,
                  const QList<QPair<QString, QString> > &rows);

private:
  QTreeWidget *_tree;
};


class CSplitDialog: public QDialog
{
  Q_OBJECT
public:
  CSplitDialog(QWidget *parent, const QString &path);
  QString Path() const;
  // The volume size in bytes; 0 when the entry could not be parsed.
  quint64 VolumeSize() const;

private slots:
  void OnBrowse();

private:
  QComboBox *_path;
  QComboBox *_volume;
};


class CAboutDialog: public QDialog
{
  Q_OBJECT
public:
  explicit CAboutDialog(QWidget *parent = nullptr);
};


class CMessagesDialog: public QDialog
{
  Q_OBJECT
public:
  CMessagesDialog(QWidget *parent, const QStringList &messages);
};

#endif
