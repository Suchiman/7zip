// Qt/OverwriteDialog.cpp

#include "StdAfx.h"

#include "../Common/IFileExtractCallback.h"

#include "OverwriteDialog.h"
#include "IconUtils.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

COverwriteDialog::COverwriteDialog(QWidget *parent):
    QDialog(parent), _answer(NOverwriteAnswer::kCancel)
{
  setWindowTitle(tr("Confirm File Replace"));

  QLabel *header = new QLabel(tr("Destination folder already contains processed file."));
  QLabel *q1 = new QLabel(tr("Would you like to replace the existing file"));
  QLabel *q2 = new QLabel(tr("with this one?"));

  QLabel *oldIcon = new QLabel;
  oldIcon->setPixmap(style()->standardIcon(QStyle::SP_FileIcon).pixmap(32, 32));
  oldIcon->setAlignment(Qt::AlignTop);
  QLabel *newIcon = new QLabel;
  newIcon->setPixmap(style()->standardIcon(QStyle::SP_FileIcon).pixmap(32, 32));
  newIcon->setAlignment(Qt::AlignTop);

  _oldFile = new QLabel;
  _oldFile->setTextFormat(Qt::PlainText);
  _oldFile->setWordWrap(true);
  _oldFile->setTextInteractionFlags(Qt::TextSelectableByMouse);
  _newFile = new QLabel;
  _newFile->setTextFormat(Qt::PlainText);
  _newFile->setWordWrap(true);
  _newFile->setTextInteractionFlags(Qt::TextSelectableByMouse);

  QGridLayout *files = new QGridLayout;
  files->addWidget(oldIcon, 0, 0);
  files->addWidget(_oldFile, 0, 1);
  files->addWidget(newIcon, 1, 0);
  files->addWidget(_newFile, 1, 1);
  files->setColumnStretch(1, 1);

  // The button block of the original dialog, in the same order.
  QPushButton *yes        = new QPushButton(tr("&Yes"));
  QPushButton *yesToAll   = new QPushButton(tr("Yes to &All"));
  QPushButton *autoRename = new QPushButton(tr("A&uto Rename"));
  QPushButton *no         = new QPushButton(tr("&No"));
  QPushButton *noToAll    = new QPushButton(tr("No to A&ll"));
  QPushButton *cancel     = new QPushButton(tr("&Cancel"));
  yes->setDefault(true);

  QGridLayout *buttons = new QGridLayout;
  buttons->addWidget(yes,        0, 0);
  buttons->addWidget(yesToAll,   0, 1);
  buttons->addWidget(autoRename, 0, 2);
  buttons->addWidget(no,         1, 0);
  buttons->addWidget(noToAll,    1, 1);
  buttons->addWidget(cancel,     1, 2);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addWidget(header);
  main->addWidget(q1);
  main->addLayout(files);
  main->addWidget(q2);
  main->addStretch(1);
  main->addLayout(buttons);

  connect(yes,        &QPushButton::clicked, this, [this]{ Finish(NOverwriteAnswer::kYes); });
  connect(yesToAll,   &QPushButton::clicked, this, [this]{ Finish(NOverwriteAnswer::kYesToAll); });
  connect(autoRename, &QPushButton::clicked, this, [this]{ Finish(NOverwriteAnswer::kAutoRename); });
  connect(no,         &QPushButton::clicked, this, [this]{ Finish(NOverwriteAnswer::kNo); });
  connect(noToAll,    &QPushButton::clicked, this, [this]{ Finish(NOverwriteAnswer::kNoToAll); });
  connect(cancel,     &QPushButton::clicked, this, [this]{ Finish(NOverwriteAnswer::kCancel); });
}

void COverwriteDialog::SetFiles(const QString &existName, const QString &existInfo,
    const QString &newName, const QString &newInfo)
{
  _oldFile->setText(existName + QStringLiteral("\n") + existInfo);
  _newFile->setText(newName + QStringLiteral("\n") + newInfo);
}

void COverwriteDialog::Finish(int answer)
{
  _answer = answer;
  if (answer == NOverwriteAnswer::kCancel)
    reject();
  else
    accept();
}
