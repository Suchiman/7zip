// Qt/CompressDialog.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"
#include "../../../Common/Wildcard.h"
#include "../../../Windows/System.h"

#include "../Common/LoadCodecs.h"

#include "CompressDialog.h"
#include "Codecs.h"
#include "Z7Qt.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

using namespace NWindows;

// ---------------------------------------------------------------------------
// The format / method tables, ported verbatim from UI/GUI/CompressDialog.cpp
// ---------------------------------------------------------------------------

enum EMethodID
{
  kCopy,
  kLZMA,
  kLZMA2,
  kPPMd,
  kBZip2,
  kDeflate,
  kDeflate64,
  kPPMdZip,
  kSha256,
  kSha1,
  kCrc32,
  kCrc64,
  kGnu,
  kPosix
};

static const char * const kMethodsNames[] =
{
    "Copy"
  , "LZMA"
  , "LZMA2"
  , "PPMd"
  , "BZip2"
  , "Deflate"
  , "Deflate64"
  , "PPMd"
  , "SHA256"
  , "SHA1"
  , "CRC32"
  , "CRC64"
  , "GNU"
  , "POSIX"
};

static const EMethodID g_7zMethods[]    = { kLZMA2, kLZMA, kPPMd, kBZip2, kDeflate, kDeflate64, kCopy };
static const EMethodID g_ZipMethods[]   = { kDeflate, kDeflate64, kBZip2, kLZMA, kPPMdZip };
static const EMethodID g_GZipMethods[]  = { kDeflate };
static const EMethodID g_BZip2Methods[] = { kBZip2 };
static const EMethodID g_XzMethods[]    = { kLZMA2 };
static const EMethodID g_TarMethods[]   = { kGnu, kPosix };

static const UInt32 kFF_Filter           = 1 << 0;
static const UInt32 kFF_Solid            = 1 << 1;
static const UInt32 kFF_MultiThread      = 1 << 2;
static const UInt32 kFF_Encrypt          = 1 << 3;
static const UInt32 kFF_EncryptFileNames = 1 << 4;
static const UInt32 kFF_MemUse           = 1 << 5;
static const UInt32 kFF_SFX              = 1 << 6;

struct CFormatInfo
{
  const char *Name;
  UInt32 LevelsMask;
  unsigned NumMethods;
  const EMethodID *MethodIDs;
  UInt32 Flags;

  bool Filter_() const { return (Flags & kFF_Filter) != 0; }
  bool Solid_() const { return (Flags & kFF_Solid) != 0; }
  bool MultiThread_() const { return (Flags & kFF_MultiThread) != 0; }
  bool Encrypt_() const { return (Flags & kFF_Encrypt) != 0; }
  bool EncryptFileNames_() const { return (Flags & kFF_EncryptFileNames) != 0; }
  bool MemUse_() const { return (Flags & kFF_MemUse) != 0; }
};

#define METHODS_PAIR(x) Z7_ARRAY_SIZE(x), x

static const CFormatInfo g_Formats[] =
{
  { "",      ((UInt32)1 << 10) - 1,                                    0, NULL,
    kFF_MultiThread | kFF_MemUse },
  { "7z",    ((UInt32)1 << 10) - 1,                                    METHODS_PAIR(g_7zMethods),
    kFF_Filter | kFF_Solid | kFF_MultiThread | kFF_Encrypt | kFF_EncryptFileNames | kFF_MemUse | kFF_SFX },
  { "Zip",   (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9), METHODS_PAIR(g_ZipMethods),
    kFF_MultiThread | kFF_Encrypt | kFF_MemUse },
  { "GZip",  (1 << 1) | (1 << 5) | (1 << 7) | (1 << 9),                METHODS_PAIR(g_GZipMethods),
    kFF_MemUse },
  { "BZip2", (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),     METHODS_PAIR(g_BZip2Methods),
    kFF_MultiThread | kFF_MemUse },
  { "xz",    ((UInt32)1 << 10) - 1 - (1 << 0),                         METHODS_PAIR(g_XzMethods),
    kFF_Solid | kFF_MultiThread | kFF_MemUse },
  { "Tar",   (1 << 0),                                                 METHODS_PAIR(g_TarMethods), 0 },
  { "wim",   (1 << 0),                                                 0, NULL, 0 }
};

// 7-Zip's level names (IDS_METHOD_*); index is the level value.
static const char * const g_LevelNames[] =
{
  "Store", "Fastest", NULL, "Fast", NULL, "Normal", NULL, "Maximum", NULL, "Ultra"
};

static const UInt32 kSolidLog_NoSolid = 0;
static const UInt32 kSolidLog_FullSolid = 64;
static const UInt32 kLzmaMaxDictSize = (UInt32)15 << 28;
static const UInt64 k_Auto = (UInt64)(Int64)-1;

static QString SizeToShortString(UInt64 v)
{
  static const char * const kUnits[] = { " B", " KB", " MB", " GB", " TB" };
  unsigned u = 0;
  while (v >= 1024 && u + 1 < (unsigned)Z7_ARRAY_SIZE(kUnits) && (v & 1023) == 0)
  {
    v >>= 10;
    u++;
  }
  return QString::number((qulonglong)v) + QLatin1String(kUnits[u]);
}

static QString AutoText(const QString &s)
{
  return QObject::tr("* %1").arg(s);
}


// ---------------------------------------------------------------------------

CCompressDialog::CCompressDialog(QWidget *parent):
    QDialog(parent), _inSetup(false)
{
  setWindowTitle(tr("Add to Archive"));

  _archive = new QComboBox;
  _archive->setEditable(true);
  _archive->setInsertPolicy(QComboBox::NoInsert);
  _archive->setMinimumWidth(420);
  QPushButton *browse = new QPushButton(QStringLiteral("..."));
  browse->setFixedWidth(32);

  QHBoxLayout *arcRow = new QHBoxLayout;
  arcRow->addWidget(MakeLabel(tr("&Archive:"), _archive));
  arcRow->addWidget(_archive, 1);
  arcRow->addWidget(browse);

  _format = new QComboBox;
  _level = new QComboBox;
  _method = new QComboBox;
  _dictionary = new QComboBox;
  _order = new QComboBox;
  _solid = new QComboBox;
  _threads = new QComboBox;
  _hardwareThreads = new QLabel;
  _memoryValue = new QLabel;
  _volume = new QComboBox;
  _volume->setEditable(true);
  _parameters = new QLineEdit;

  QGridLayout *left = new QGridLayout;
  int r = 0;
  left->addWidget(MakeLabel(tr("Archive &format:"), _format),        r, 0); left->addWidget(_format,     r, 1, 1, 2); r++;
  left->addWidget(MakeLabel(tr("Compression &level:"), _level),     r, 0); left->addWidget(_level,      r, 1, 1, 2); r++;
  left->addWidget(MakeLabel(tr("Compression &method:"), _method),   r, 0); left->addWidget(_method,     r, 1, 1, 2); r++;
  left->addWidget(MakeLabel(tr("&Dictionary size:"), _dictionary),  r, 0); left->addWidget(_dictionary, r, 1, 1, 2); r++;
  left->addWidget(MakeLabel(tr("&Word size:"), _order),             r, 0); left->addWidget(_order,      r, 1, 1, 2); r++;
  left->addWidget(MakeLabel(tr("&Solid Block size:"), _solid),      r, 0); left->addWidget(_solid,      r, 1, 1, 2); r++;
  left->addWidget(MakeLabel(tr("Number of CPU &threads:"), _threads), r, 0);
  left->addWidget(_threads, r, 1); left->addWidget(_hardwareThreads, r, 2); r++;
  left->addWidget(new QLabel(tr("Memory usage for Compressing:")), r, 0);
  left->addWidget(_memoryValue, r, 1, 1, 2); r++;
  left->addWidget(MakeLabel(tr("Split to &volumes, bytes:"), _volume), r, 0);
  left->addWidget(_volume, r, 1, 1, 2); r++;
  left->addWidget(new QLabel(tr("Parameters:")), r, 0);
  left->addWidget(_parameters, r, 1, 1, 2); r++;
  left->setColumnStretch(1, 1);

  _updateMode = new QComboBox;
  _updateMode->addItem(tr("Add and replace files"),  (int)NQtCompress::NUpdateMode::kAdd);
  _updateMode->addItem(tr("Update and add files"),   (int)NQtCompress::NUpdateMode::kUpdate);
  _updateMode->addItem(tr("Freshen existing files"), (int)NQtCompress::NUpdateMode::kFresh);
  _updateMode->addItem(tr("Synchronize files"),      (int)NQtCompress::NUpdateMode::kSync);

  _pathMode = new QComboBox;
  _pathMode->addItem(tr("Relative pathnames"), (int)NWildcard::k_RelatPath);
  _pathMode->addItem(tr("Full pathnames"),     (int)NWildcard::k_FullPath);
  _pathMode->addItem(tr("Absolute pathnames"), (int)NWildcard::k_AbsPath);

  _deleteAfter = new QCheckBox(tr("Delete files after compression"));

  QGroupBox *optGroup = new QGroupBox(tr("Options"));
  QVBoxLayout *optLayout = new QVBoxLayout(optGroup);
  optLayout->addWidget(_deleteAfter);

  _password1 = new QLineEdit;
  _password1->setEchoMode(QLineEdit::Password);
  _password2 = new QLineEdit;
  _password2->setEchoMode(QLineEdit::Password);
  _showPassword = new QCheckBox(tr("Show Password"));
  _encryptionMethod = new QComboBox;
  _encryptFileNames = new QCheckBox(tr("Encrypt file &names"));

  QGroupBox *encGroup = new QGroupBox(tr("Encryption"));
  QGridLayout *enc = new QGridLayout(encGroup);
  int er = 0;
  enc->addWidget(MakeLabel(tr("Enter &password:"), _password1), er++, 0, 1, 2);
  enc->addWidget(_password1, er++, 0, 1, 2);
  enc->addWidget(new QLabel(tr("Reenter password:")), er++, 0, 1, 2);
  enc->addWidget(_password2, er++, 0, 1, 2);
  enc->addWidget(_showPassword, er++, 0, 1, 2);
  enc->addWidget(MakeLabel(tr("&Encryption method:"), _encryptionMethod), er, 0);
  enc->addWidget(_encryptionMethod, er++, 1);
  enc->addWidget(_encryptFileNames, er++, 0, 1, 2);

  QGridLayout *right = new QGridLayout;
  right->addWidget(MakeLabel(tr("&Update mode:"), _updateMode), 0, 0);
  right->addWidget(_updateMode, 0, 1);
  right->addWidget(new QLabel(tr("Path mode:")), 1, 0);
  right->addWidget(_pathMode, 1, 1);
  right->addWidget(optGroup, 2, 0, 1, 2);
  right->addWidget(encGroup, 3, 0, 1, 2);
  right->setRowStretch(4, 1);
  right->setColumnStretch(1, 1);

  QHBoxLayout *columns = new QHBoxLayout;
  columns->addLayout(left, 1);
  columns->addLayout(right, 1);

  QDialogButtonBox *box = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  QVBoxLayout *main = new QVBoxLayout(this);
  main->addLayout(arcRow);
  main->addLayout(columns, 1);
  main->addWidget(box);

  connect(browse, &QPushButton::clicked, this, &CCompressDialog::OnBrowse);
  connect(_format, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &CCompressDialog::OnFormatChanged);
  connect(_level, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &CCompressDialog::OnLevelChanged);
  connect(_method, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &CCompressDialog::OnMethodChanged);
  connect(_dictionary, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &CCompressDialog::UpdateMemoryUsage);
  connect(_threads, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &CCompressDialog::UpdateMemoryUsage);
  connect(_showPassword, &QCheckBox::toggled, this, [this](bool on)
  {
    const QLineEdit::EchoMode m = on ? QLineEdit::Normal : QLineEdit::Password;
    _password1->setEchoMode(m);
    _password2->setEchoMode(m);
    _password2->setVisible(!on);
  });
  connect(box, &QDialogButtonBox::accepted, this, &CCompressDialog::accept);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  _hardwareThreads->setText(QStringLiteral("/ %1").arg(NSystem::GetNumberOfProcessors()));

  SetFormats();
  SetEncryptionMethods();
  OnFormatChanged();
}


void CCompressDialog::SetArchiveBaseName(const QString &baseNameNoExt)
{
  _baseName = baseNameNoExt;
  UpdateArchiveExtension();
}

void CCompressDialog::SetFormats()
{
  _inSetup = true;
  _format->clear();
  CCodecs *codecs = GetCodecs();
  if (codecs)
  {
    // Same filter as UI/GUI/UpdateGUI.cpp: updatable, no hash handlers.
    FOR_VECTOR (i, codecs->Formats)
    {
      const CArcInfoEx &ai = codecs->Formats[i];
      if (!ai.UpdateEnabled || ai.Flags_HashHandler())
        continue;
      _format->addItem(Us2Q(ai.Name), (int)i);
    }
  }
  _format->model()->sort(0);
  const int sevenZ = _format->findText(QStringLiteral("7z"), Qt::MatchFixedString);
  if (sevenZ >= 0)
    _format->setCurrentIndex(sevenZ);
  _inSetup = false;
}

int CCompressDialog::CurFormatIndex() const
{
  if (_format->currentIndex() < 0)
    return -1;
  return _format->currentData().toInt();
}

int CCompressDialog::StaticFormatIndex() const
{
  CCodecs *codecs = GetCodecs();
  const int fi = CurFormatIndex();
  if (!codecs || fi < 0)
    return 0;
  const UString &name = codecs->Formats[(unsigned)fi].Name;
  for (unsigned i = 1; i < Z7_ARRAY_SIZE(g_Formats); i++)
    if (name.IsEqualTo_Ascii_NoCase(g_Formats[i].Name))
      return (int)i;
  return 0;
}

UInt32 CCompressDialog::CurLevel() const
{
  if (_level->currentIndex() < 0)
    return 5;
  return (UInt32)_level->currentData().toUInt();
}

int CCompressDialog::CurMethodID() const
{
  if (_method->currentIndex() < 0)
    return -1;
  const QVariant v = _method->currentData();
  return v.isValid() ? v.toInt() : -1;
}


void CCompressDialog::OnFormatChanged()
{
  if (_inSetup)
    return;
  _inSetup = true;
  SetLevels();
  SetMethods();
  _inSetup = false;
  SetDictionary();
  SetOrder();
  SetSolidBlockSize();
  SetNumThreads();
  UpdateArchiveExtension();
  UpdateMemoryUsage();

  const CFormatInfo &fi = g_Formats[StaticFormatIndex()];
  _solid->setEnabled(fi.Solid_());
  _threads->setEnabled(fi.MultiThread_());
  _password1->setEnabled(fi.Encrypt_());
  _password2->setEnabled(fi.Encrypt_());
  _encryptionMethod->setEnabled(fi.Encrypt_());
  _encryptFileNames->setEnabled(fi.EncryptFileNames_());
  SetEncryptionMethods();
}

void CCompressDialog::OnLevelChanged()
{
  if (_inSetup)
    return;
  _inSetup = true;
  SetMethods();
  _inSetup = false;
  SetDictionary();
  SetOrder();
  SetSolidBlockSize();
  UpdateMemoryUsage();
}

void CCompressDialog::OnMethodChanged()
{
  if (_inSetup)
    return;
  SetDictionary();
  SetOrder();
  SetSolidBlockSize();
  UpdateArchiveExtension();
  UpdateMemoryUsage();
}


void CCompressDialog::SetLevels()
{
  const UInt32 prev = _level->currentIndex() >= 0 ? CurLevel() : 5;
  _level->clear();
  const CFormatInfo &fi = g_Formats[StaticFormatIndex()];
  int sel = -1;
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_LevelNames); i++)
  {
    if ((fi.LevelsMask & ((UInt32)1 << i)) == 0 || !g_LevelNames[i])
      continue;
    _level->addItem(QString::asprintf("%u - %s", i, g_LevelNames[i]), i);
    if (i <= prev)
      sel = _level->count() - 1;
  }
  if (sel < 0)
    sel = _level->count() / 2;
  _level->setCurrentIndex(sel);
}

void CCompressDialog::SetMethods()
{
  _method->clear();
  const CFormatInfo &fi = g_Formats[StaticFormatIndex()];
  if (fi.NumMethods == 0)
  {
    _method->setEnabled(false);
    return;
  }
  _method->setEnabled(true);
  if (CurLevel() == 0 && fi.Solid_())
  {
    // level 0 = store: 7-Zip still lists Copy first
  }
  for (unsigned i = 0; i < fi.NumMethods; i++)
  {
    const EMethodID id = fi.MethodIDs[i];
    _method->addItem(QString::fromLatin1(kMethodsNames[id]), (int)id);
  }
  _method->setCurrentIndex(0);
}


void CCompressDialog::SetDictionary()
{
  _dictionary->clear();
  const int methodID = CurMethodID();
  const UInt32 level = CurLevel();
  if (methodID < 0)
  {
    _dictionary->setEnabled(false);
    return;
  }
  _dictionary->setEnabled(true);

  UInt64 autoDict = k_Auto;
  QVector<UInt64> values;

  switch (methodID)
  {
    case kLZMA:
    case kLZMA2:
    {
      autoDict = level <= 4
          ? (UInt64)1 << (level * 2 + 16)
          : (level <= sizeof(size_t) / 2 + 4
              ? (UInt64)1 << (level + 20)
              : (UInt64)1 << (sizeof(size_t) / 2 + 24));
      const UInt64 kMaxUp = (UInt64)1 << (20 + sizeof(size_t) / 4 * 6);
      for (unsigned i = (16 - 1) * 2; i <= (32 - 1) * 2; i++)
      {
        if (i < (20 - 1) * 2 && i != (16 - 1) * 2 && i != (18 - 1) * 2)
          continue;
        if (i == (20 - 1) * 2 + 1)
          continue;
        const UInt64 dictUp = (UInt64)(2 + (i & 1)) << (i / 2);
        values.append(dictUp >= kLzmaMaxDictSize ? kLzmaMaxDictSize : dictUp);
        if (dictUp >= kMaxUp)
          break;
      }
      break;
    }
    case kPPMd:
    {
      autoDict = (UInt64)1 << (level + 19);
      const UInt64 kPpmdDefault4g = (UInt64)0xFFFFFC00;
      const UInt64 kMaxUp = (UInt64)1 << (29 + sizeof(size_t) / 8);
      for (unsigned i = (20 - 1) * 2; i <= (32 - 1) * 2; i++)
      {
        if (i == (20 - 1) * 2 + 1)
          continue;
        const UInt64 dictUp = (UInt64)(2 + (i & 1)) << (i / 2);
        values.append(dictUp >= kPpmdDefault4g ? kPpmdDefault4g : dictUp);
        if (dictUp >= kMaxUp)
          break;
      }
      break;
    }
    case kPPMdZip:
    {
      autoDict = (UInt64)1 << (level + 19);
      for (unsigned i = 20; i <= 28; i++)
        values.append((UInt64)1 << i);
      break;
    }
    case kDeflate:
      autoDict = (UInt64)1 << 15;
      break;
    case kDeflate64:
      autoDict = (UInt64)1 << 16;
      break;
    case kBZip2:
    {
      autoDict = level >= 5 ? (900u << 10) : (level >= 3 ? (500u << 10) : (100u << 10));
      for (unsigned i = 1; i <= 9; i++)
        values.append(((UInt64)i * 100) << 10);
      break;
    }
    case kCopy:
      autoDict = 0;
      break;
    default:
      break;
  }

  _dictionary->addItem(AutoText(SizeToShortString(autoDict)), QVariant((qulonglong)k_Auto));
  int sel = 0;
  for (int i = 0; i < values.size(); i++)
  {
    _dictionary->addItem(SizeToShortString(values[i]), QVariant((qulonglong)values[i]));
    if (values[i] <= autoDict)
      sel = _dictionary->count() - 1;
  }
  _dictionary->setCurrentIndex(sel == 0 ? 0 : 0);   // "*" (auto) is the default
}


void CCompressDialog::SetOrder()
{
  _order->clear();
  const int methodID = CurMethodID();
  const UInt32 level = CurLevel();
  if (methodID < 0)
  {
    _order->setEnabled(false);
    return;
  }

  UInt32 autoOrder = 1;
  QVector<UInt32> values;
  switch (methodID)
  {
    case kLZMA:
    case kLZMA2:
    {
      autoOrder = (level < 7 ? 32 : 64);
      for (unsigned i = 2 * 2; i < 8 * 2; i++)
      {
        UInt32 order = ((UInt32)(2 + (i & 1)) << (i / 2));
        if (order > 256)
          order = 273;
        values.append(order);
      }
      break;
    }
    case kDeflate:
    case kDeflate64:
    {
      autoOrder = (level >= 9 ? 128 : (level >= 7 ? 64 : 32));
      for (unsigned i = 2 * 2; i < 8 * 2; i++)
      {
        UInt32 order = ((UInt32)(2 + (i & 1)) << (i / 2));
        if (order > 256)
          order = (methodID == kDeflate64 ? 257 : 258);
        values.append(order);
      }
      break;
    }
    case kPPMd:
    {
      autoOrder = (level >= 9 ? 32 : (level >= 7 ? 16 : (level >= 5 ? 6 : 4)));
      for (unsigned i = 0;; i++)
      {
        UInt32 order = i + 2;
        if (i >= 2)
          order = (4 + ((i - 2) & 3)) << ((i - 2) / 4);
        values.append(order);
        if (order >= 32)
          break;
      }
      break;
    }
    default:
      _order->setEnabled(false);
      return;
  }

  _order->setEnabled(true);
  _order->addItem(AutoText(QString::number(autoOrder)), QVariant((uint)(UInt32)(Int32)-1));
  for (int i = 0; i < values.size(); i++)
    _order->addItem(QString::number(values[i]), QVariant((uint)values[i]));
  _order->setCurrentIndex(0);
}


void CCompressDialog::SetSolidBlockSize()
{
  _solid->clear();
  const CFormatInfo &fi = g_Formats[StaticFormatIndex()];
  if (!fi.Solid_() || CurLevel() == 0)
  {
    _solid->setEnabled(false);
    return;
  }
  _solid->setEnabled(true);

  _solid->addItem(AutoText(tr("auto")), QVariant((uint)(UInt32)(Int32)-1));
  _solid->addItem(tr("Non-solid"), QVariant((uint)kSolidLog_NoSolid));
  for (unsigned i = 20; i <= 36; i++)
    _solid->addItem(SizeToShortString((UInt64)1 << i), QVariant((uint)i));
  _solid->addItem(tr("Solid"), QVariant((uint)kSolidLog_FullSolid));
  _solid->setCurrentIndex(0);
}


void CCompressDialog::SetNumThreads()
{
  _threads->clear();
  const CFormatInfo &fi = g_Formats[StaticFormatIndex()];
  if (!fi.MultiThread_())
  {
    _threads->setEnabled(false);
    return;
  }
  _threads->setEnabled(true);
  const UInt32 numHardwareThreads = NSystem::GetNumberOfProcessors();
  _threads->addItem(AutoText(QString::number(numHardwareThreads)),
      QVariant((uint)(UInt32)(Int32)-1));
  for (UInt32 i = 1; i <= numHardwareThreads * 2 && i <= 256; i++)
    _threads->addItem(QString::number(i), QVariant((uint)i));
  _threads->setCurrentIndex(0);
}


void CCompressDialog::SetEncryptionMethods()
{
  const int si = StaticFormatIndex();
  _encryptionMethod->clear();
  if (QString::fromLatin1(g_Formats[si].Name) == QLatin1String("Zip"))
  {
    _encryptionMethod->addItem(QStringLiteral("ZipCrypto"));
    _encryptionMethod->addItem(QStringLiteral("AES-256"));
  }
  else
    _encryptionMethod->addItem(QStringLiteral("AES-256"));
  _encryptionMethod->setCurrentIndex(0);
}


void CCompressDialog::UpdateArchiveExtension()
{
  CCodecs *codecs = GetCodecs();
  const int fi = CurFormatIndex();
  if (!codecs || fi < 0 || _baseName.isEmpty())
    return;
  const CArcInfoEx &ai = codecs->Formats[(unsigned)fi];
  QString name = _baseName;
  name += QLatin1Char('.');
  name += Us2Q(ai.GetMainExt());
  _archive->setEditText(name);
}


void CCompressDialog::UpdateMemoryUsage()
{
  // 7-Zip prints an estimate here; we show the dictionary-derived figure that
  // the LZMA encoder documents (about 11.5 x dictionary for compression).
  const int methodID = CurMethodID();
  const QVariant d = _dictionary->currentData();
  UInt64 dict = d.isValid() ? (UInt64)d.toULongLong() : k_Auto;
  if (dict == k_Auto)
  {
    _memoryValue->setText(QString());
    return;
  }
  UInt64 mem = dict;
  if (methodID == kLZMA || methodID == kLZMA2)
    mem = dict / 2 * 23 + (UInt64)(2 << 20);
  else if (methodID == kPPMd || methodID == kPPMdZip)
    mem = dict;
  else if (methodID == kBZip2)
    mem = dict * 10;
  _memoryValue->setText(SizeToStringShort(mem));
}


void CCompressDialog::OnBrowse()
{
  const QString path = QFileDialog::getSaveFileName(this, tr("Browse"),
      _archive->currentText(), tr("All Files (*)"), nullptr,
      QFileDialog::DontConfirmOverwrite);
  if (!path.isEmpty())
    _archive->setEditText(path);
}


void CCompressDialog::accept()
{
  if (_archive->currentText().isEmpty())
  {
    QMessageBox::warning(this, windowTitle(), tr("Specify the archive name"));
    return;
  }
  if (!_showPassword->isChecked() && _password1->text() != _password2->text())
  {
    QMessageBox::warning(this, windowTitle(), tr("Passwords do not match"));
    return;
  }

  NQtCompress::CInfo info;
  info.FormatIndex = CurFormatIndex();
  info.UpdateMode = (NQtCompress::NUpdateMode::EEnum)_updateMode->currentData().toInt();
  info.PathMode = _pathMode->currentData().toInt();
  info.Level = CurLevel();

  const int methodID = CurMethodID();
  if (methodID >= 0 && _method->isEnabled())
    info.Method = GetUnicodeString(kMethodsNames[methodID]);

  {
    const QVariant v = _dictionary->currentData();
    const UInt64 dict = v.isValid() ? (UInt64)v.toULongLong() : k_Auto;
    if (dict != k_Auto)
      info.Dict64 = dict;
  }
  {
    const QVariant v = _order->currentData();
    const UInt32 order = v.isValid() ? (UInt32)v.toUInt() : (UInt32)(Int32)-1;
    if (order != (UInt32)(Int32)-1)
      info.Order = order;
    // PPMd/LZMA use "o" vs "fb"; OrderMode selects which name is written.
    info.OrderMode = (methodID == kPPMd || methodID == kPPMdZip);
  }
  {
    const CFormatInfo &fi = g_Formats[StaticFormatIndex()];
    info.SolidIsSpecified = fi.Solid_() && _solid->isEnabled();
    const QVariant v = _solid->currentData();
    const UInt32 solidLog = v.isValid() ? (UInt32)v.toUInt() : (UInt32)(Int32)-1;
    info.SolidBlockSize = 0;
    if (solidLog == (UInt32)(Int32)-1)
      info.SolidIsSpecified = false;
    else if (solidLog > 0)
      info.SolidBlockSize = (solidLog >= 64) ? (UInt64)(Int64)-1 : ((UInt64)1 << solidLog);
  }
  {
    const QVariant v = _threads->currentData();
    const UInt32 t = v.isValid() ? (UInt32)v.toUInt() : (UInt32)(Int32)-1;
    if (t != (UInt32)(Int32)-1)
      info.NumThreads = t;
  }

  info.DeleteAfterCompressing = _deleteAfter->isChecked();
  info.Options = Q2Us(_parameters->text());
  info.ArcPath = Q2Us(QFileInfo(_archive->currentText()).isAbsolute()
      ? _archive->currentText()
      : _curDirPrefix + _archive->currentText());

  if (!_password1->text().isEmpty())
  {
    info.Password = Q2Us(_password1->text());
    if (_encryptionMethod->isEnabled() && _encryptionMethod->count() > 1)
      info.EncryptionMethod = Q2Us(_encryptionMethod->currentText());
    if (_encryptFileNames->isEnabled())
    {
      info.EncryptHeadersIsAllowed = true;
      info.EncryptHeaders = _encryptFileNames->isChecked();
    }
  }

  // "Split to volumes" accepts sizes like "10m", "700m", "4480m"
  {
    const QString vs = _volume->currentText().trimmed();
    if (!vs.isEmpty())
    {
      const UString u = Q2Us(vs);
      const wchar_t *end;
      UInt64 v = ConvertStringToUInt64(u.Ptr(), &end);
      if (v != 0)
      {
        switch (*end)
        {
          case 'k': case 'K': v <<= 10; break;
          case 'm': case 'M': v <<= 20; break;
          case 'g': case 'G': v <<= 30; break;
          default: break;
        }
        info.VolumeSizes.Add(v);
      }
    }
  }

  _info = info;
  QDialog::accept();
}
