// Qt/PasswordDialog.cpp

#include "StdAfx.h"

#include "PasswordDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

CPasswordDialog::CPasswordDialog(QWidget *parent): QDialog(parent)
{
  setWindowTitle(tr("Enter password"));

  QLabel *label = new QLabel(tr("&Enter password:"));
  _edit = new QLineEdit;
  _edit->setEchoMode(QLineEdit::Password);
  label->setBuddy(_edit);

  _show = new QCheckBox(tr("&Show password"));

  QDialogButtonBox *box = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addWidget(label);
  main->addWidget(_edit);
  main->addWidget(_show);
  main->addWidget(box);

  connect(_show, &QCheckBox::toggled, this, [this](bool on)
      { _edit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password); });
  connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  setMinimumWidth(360);
}

QString CPasswordDialog::Password() const
{
  return _edit->text();
}
