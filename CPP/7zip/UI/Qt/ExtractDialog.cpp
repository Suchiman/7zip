// Qt/ExtractDialog.cpp

#include "StdAfx.h"

#include "ExtractDialog.h"
#include "OptionsDialog.h"
#include "Z7Qt.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

// The same two lists as kPathMode_IDs / kOverwriteMode_IDs, in the same order.
static const struct { const char *Text; int Value; } kPathModes[] =
{
  { "Full pathnames",     NExtract::NPathMode::kFullPaths },
  { "No pathnames",       NExtract::NPathMode::kNoPaths },
  { "Absolute pathnames", NExtract::NPathMode::kAbsPaths }
};

static const struct { const char *Text; int Value; } kOverwriteModes[] =
{
  { "Ask before overwrite",        NExtract::NOverwriteMode::kAsk },
  { "Overwrite without prompt",    NExtract::NOverwriteMode::kOverwrite },
  { "Skip existing files",         NExtract::NOverwriteMode::kSkip },
  { "Auto rename",                 NExtract::NOverwriteMode::kRename },
  { "Auto rename existing files",  NExtract::NOverwriteMode::kRenameExisting }
};


CExtractDialog::CExtractDialog(QWidget *parent): QDialog(parent)
{
  setWindowTitle(tr("Extract"));

  QLabel *pathLabel = new QLabel(tr("E&xtract to:"));
  _path = new QComboBox;
  _path->setEditable(true);
  _path->setInsertPolicy(QComboBox::NoInsert);
  _path->setMinimumWidth(420);
  pathLabel->setBuddy(_path);

  QPushButton *browse = new QPushButton(QStringLiteral("..."));
  browse->setFixedWidth(32);

  QHBoxLayout *pathRow = new QHBoxLayout;
  pathRow->addWidget(_path, 1);
  pathRow->addWidget(browse);

  _nameEnable = new QCheckBox;
  _name = new QLineEdit;
  _name->setEnabled(false);
  QHBoxLayout *nameRow = new QHBoxLayout;
  nameRow->addWidget(_nameEnable);
  nameRow->addWidget(_name, 1);

  _pathMode = new QComboBox;
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(kPathModes); i++)
    _pathMode->addItem(tr(kPathModes[i].Text), kPathModes[i].Value);

  _elimDup = new QCheckBox(tr("Eliminate duplication of root folder"));
  _elimDup->setChecked(GlobalFmSettings().ElimDup);

  _overwriteMode = new QComboBox;
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(kOverwriteModes); i++)
    _overwriteMode->addItem(tr(kOverwriteModes[i].Text), kOverwriteModes[i].Value);

  QVBoxLayout *left = new QVBoxLayout;
  left->addWidget(new QLabel(tr("Path mode:")));
  left->addWidget(_pathMode);
  left->addWidget(_elimDup);
  left->addSpacing(8);
  left->addWidget(new QLabel(tr("Overwrite mode:")));
  left->addWidget(_overwriteMode);
  left->addStretch(1);

  _password = new QLineEdit;
  _password->setEchoMode(QLineEdit::Password);
  _showPassword = new QCheckBox(tr("Show Password"));

  QGroupBox *pwGroup = new QGroupBox(tr("Password"));
  QVBoxLayout *pwLayout = new QVBoxLayout(pwGroup);
  pwLayout->addWidget(_password);
  pwLayout->addWidget(_showPassword);

  QVBoxLayout *right = new QVBoxLayout;
  right->addWidget(pwGroup);
  right->addStretch(1);

  QHBoxLayout *columns = new QHBoxLayout;
  columns->addLayout(left, 1);
  columns->addLayout(right, 1);

  QDialogButtonBox *box = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addWidget(pathLabel);
  main->addLayout(pathRow);
  main->addLayout(nameRow);
  main->addSpacing(8);
  main->addLayout(columns, 1);
  main->addWidget(box);

  connect(browse, &QPushButton::clicked, this, &CExtractDialog::OnBrowse);
  connect(_nameEnable, &QCheckBox::toggled, _name, &QLineEdit::setEnabled);
  connect(_showPassword, &QCheckBox::toggled, this, [this](bool on)
      { _password->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password); });
  connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CExtractDialog::SetPath(const QString &path)
{
  _path->setEditText(path);
}

void CExtractDialog::SetArchiveName(const QString &name)
{
  _name->setText(name);
}

void CExtractDialog::OnBrowse()
{
  const QString dir = QFileDialog::getExistingDirectory(this,
      tr("Specify a location for extracted files."), _path->currentText());
  if (!dir.isEmpty())
    _path->setEditText(dir);
}

CQtExtractInfo CExtractDialog::Info() const
{
  CQtExtractInfo info;
  QString dir = _path->currentText();
  if (dir.isEmpty())
    dir = QDir::currentPath();
  if (_nameEnable->isChecked() && !_name->text().isEmpty())
  {
    if (!dir.endsWith(QLatin1Char('/')))
      dir += QLatin1Char('/');
    dir += _name->text();
  }
  info.OutputDir = dir;
  info.PathMode = (NExtract::NPathMode::EEnum)_pathMode->currentData().toInt();
  info.OverwriteMode = (NExtract::NOverwriteMode::EEnum)_overwriteMode->currentData().toInt();
  info.ElimDup = _elimDup->isChecked();
  if (!_password->text().isEmpty())
  {
    info.Password = Q2Us(_password->text());
    info.PasswordIsDefined = true;
  }
  return info;
}
