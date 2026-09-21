// Qt/SmallDialogs.cpp

#include "StdAfx.h"

#include "../../../Common/StringToInt.h"
#include "../../MyVersion.h"

#include "SmallDialogs.h"
#include "Z7Qt.h"

#include <QComboBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// CComboDialog
// ---------------------------------------------------------------------------

CComboDialog::CComboDialog(QWidget *parent, const QString &title,
    const QString &staticText, const QString &value, const QStringList &history):
    QDialog(parent)
{
  setWindowTitle(title);

  QLabel *label = new QLabel(staticText);
  _combo = new QComboBox;
  _combo->setEditable(true);
  _combo->setInsertPolicy(QComboBox::NoInsert);
  _combo->addItems(history);
  _combo->setEditText(value);
  _combo->setMinimumWidth(400);
  label->setBuddy(_combo);

  QDialogButtonBox *box = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addWidget(label);
  main->addWidget(_combo);
  main->addWidget(box);

  connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  // 7-Zip preselects the name without its extension when renaming.
  _combo->lineEdit()->selectAll();
}

QString CComboDialog::Value() const
{
  return _combo->currentText();
}


// ---------------------------------------------------------------------------
// CCopyDialog
// ---------------------------------------------------------------------------

CCopyDialog::CCopyDialog(QWidget *parent, const QString &title,
    const QString &staticText, const QString &value, const QString &info):
    QDialog(parent), _title(title)
{
  setWindowTitle(title);

  QLabel *label = new QLabel(staticText);
  _combo = new QComboBox;
  _combo->setEditable(true);
  _combo->setInsertPolicy(QComboBox::NoInsert);
  _combo->setEditText(value);
  _combo->setMinimumWidth(440);

  QPushButton *browse = new QPushButton(QStringLiteral("..."));
  browse->setFixedWidth(32);

  QHBoxLayout *row = new QHBoxLayout;
  row->addWidget(_combo, 1);
  row->addWidget(browse);

  QLabel *infoLabel = new QLabel(info);
  infoLabel->setTextFormat(Qt::PlainText);
  infoLabel->setWordWrap(true);

  QDialogButtonBox *box = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addWidget(label);
  main->addLayout(row);
  main->addWidget(infoLabel, 1);
  main->addWidget(box);

  connect(browse, &QPushButton::clicked, this, &CCopyDialog::OnBrowse);
  connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CCopyDialog::OnBrowse()
{
  const QString dir = QFileDialog::getExistingDirectory(this, _title, _combo->currentText());
  if (!dir.isEmpty())
    _combo->setEditText(dir);
}

QString CCopyDialog::Value() const
{
  return _combo->currentText();
}


// ---------------------------------------------------------------------------
// CListViewDialog
// ---------------------------------------------------------------------------

CListViewDialog::CListViewDialog(QWidget *parent, const QString &title,
    const QList<QPair<QString, QString> > &rows): QDialog(parent)
{
  setWindowTitle(title);
  resize(640, 480);

  _tree = new QTreeWidget;
  _tree->setColumnCount(2);
  _tree->setHeaderLabels(QStringList() << tr("Property") << tr("Value"));
  _tree->setRootIsDecorated(false);
  _tree->setAlternatingRowColors(true);
  _tree->setSelectionMode(QAbstractItemView::ExtendedSelection);

  for (const auto &row : rows)
  {
    QTreeWidgetItem *item = new QTreeWidgetItem(_tree);
    item->setText(0, row.first);
    item->setText(1, row.second);
  }
  _tree->resizeColumnToContents(0);

  QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addWidget(_tree, 1);
  main->addWidget(box);

  connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
}


// ---------------------------------------------------------------------------
// CSplitDialog
// ---------------------------------------------------------------------------

CSplitDialog::CSplitDialog(QWidget *parent, const QString &path): QDialog(parent)
{
  setWindowTitle(tr("Split File"));

  _path = new QComboBox;
  _path->setEditable(true);
  _path->setEditText(path);
  _path->setMinimumWidth(420);
  QPushButton *browse = new QPushButton(QStringLiteral("..."));
  browse->setFixedWidth(32);

  QHBoxLayout *row = new QHBoxLayout;
  row->addWidget(_path, 1);
  row->addWidget(browse);

  _volume = new QComboBox;
  _volume->setEditable(true);
  // The default list 7-Zip offers.
  _volume->addItem(QStringLiteral("10M"));
  _volume->addItem(QStringLiteral("100M"));
  _volume->addItem(QStringLiteral("700M"));
  _volume->addItem(QStringLiteral("4480M"));
  _volume->setEditText(QStringLiteral("10M"));

  QDialogButtonBox *box = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addWidget(MakeLabel(tr("&Split to:"), _path));
  main->addLayout(row);
  main->addWidget(MakeLabel(tr("Split to &volumes,  bytes:"), _volume));
  main->addWidget(_volume);
  main->addWidget(box);

  connect(browse, &QPushButton::clicked, this, &CSplitDialog::OnBrowse);
  connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CSplitDialog::OnBrowse()
{
  const QString dir = QFileDialog::getExistingDirectory(this, windowTitle(), _path->currentText());
  if (!dir.isEmpty())
    _path->setEditText(dir);
}

QString CSplitDialog::Path() const { return _path->currentText(); }

quint64 CSplitDialog::VolumeSize() const
{
  const UString s = Q2Us(_volume->currentText().trimmed());
  if (s.IsEmpty())
    return 0;
  const wchar_t *end;
  UInt64 v = ConvertStringToUInt64(s.Ptr(), &end);
  if (v == 0)
    return 0;
  switch (*end)
  {
    case 'k': case 'K': v <<= 10; break;
    case 'm': case 'M': v <<= 20; break;
    case 'g': case 'G': v <<= 30; break;
    default: break;
  }
  return v;
}


// ---------------------------------------------------------------------------
// CAboutDialog
// ---------------------------------------------------------------------------

CAboutDialog::CAboutDialog(QWidget *parent): QDialog(parent)
{
  setWindowTitle(tr("About 7-Zip"));

  QLabel *title = new QLabel(QStringLiteral("7-Zip ") + QLatin1String(MY_VERSION_CPU));
  QFont f = title->font();
  f.setPointSize(f.pointSize() + 4);
  f.setBold(true);
  title->setFont(f);

  QLabel *text = new QLabel(QLatin1String(
      MY_COPYRIGHT "  :  " MY_DATE "\n\n"
      "Qt port of the 7-Zip file manager.\n"
      "7-Zip is free software distributed under the GNU LGPL."));
  text->setTextFormat(Qt::PlainText);

  QLabel *link = new QLabel(QStringLiteral(
      "<a href=\"https://www.7-zip.org\">www.7-zip.org</a>"));
  link->setOpenExternalLinks(true);

  QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addWidget(title);
  main->addWidget(text);
  main->addWidget(link);
  main->addStretch(1);
  main->addWidget(box);

  connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
}


// ---------------------------------------------------------------------------
// CMessagesDialog
// ---------------------------------------------------------------------------

CMessagesDialog::CMessagesDialog(QWidget *parent, const QStringList &messages):
    QDialog(parent)
{
  setWindowTitle(tr("Diagnostic messages"));
  resize(640, 400);

  QListWidget *list = new QListWidget;
  list->addItems(messages);
  list->setSelectionMode(QAbstractItemView::ExtendedSelection);

  QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addWidget(list, 1);
  main->addWidget(box);

  connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
}
