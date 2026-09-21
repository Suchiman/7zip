// Qt/Operations.h
//
// The GUI-side entry points, i.e. the port of UI/GUI/ExtractGUI.cpp and
// UI/GUI/UpdateGUI.cpp. Each of these opens the progress dialog, runs the
// matching UI/Common function on a worker thread and reports the result.

#ifndef ZIP7_INC_QT_OPERATIONS_H
#define ZIP7_INC_QT_OPERATIONS_H

#include "../../../Common/MyTypes.h"
#include "../../../Common/MyString.h"
#include "../Common/ExtractMode.h"
#include "../Common/Property.h"

#include <QString>
#include <QStringList>
#include <QVector>

class QWidget;

// ---------------------------------------------------------------------------
// Add to archive: mirrors NCompressDialog::CInfo
// ---------------------------------------------------------------------------

namespace NQtCompress {

namespace NUpdateMode {
enum EEnum { kAdd, kUpdate, kFresh, kSync };
}

struct CInfo
{
  NUpdateMode::EEnum UpdateMode;
  int PathMode;               // NWildcard::ECensorPathMode

  bool SolidIsSpecified;
  UInt64 SolidBlockSize;
  UInt32 NumThreads;

  CRecordVector<UInt64> VolumeSizes;

  UInt32 Level;
  UString Method;
  UInt64 Dict64;
  bool OrderMode;
  UInt32 Order;
  UString Options;

  UString EncryptionMethod;

  bool DeleteAfterCompressing;

  CBoolPair SymLinks;
  CBoolPair HardLinks;
  CBoolPair StoreOwnerId;
  CBoolPair StoreOwnerName;

  UInt32 TimePrec;
  CBoolPair MTime;
  CBoolPair CTime;
  CBoolPair ATime;
  CBoolPair SetArcMTime;

  UString ArcPath;            // the full archive path, extension included
  int FormatIndex;            // index into CCodecs::Formats

  UString Password;
  bool EncryptHeadersIsAllowed;
  bool EncryptHeaders;

  CInfo():
      UpdateMode(NUpdateMode::kAdd),
      PathMode(0),
      SolidIsSpecified(false),
      SolidBlockSize(0),
      NumThreads((UInt32)(Int32)-1),
      Level((UInt32)(Int32)-1),
      Dict64((UInt64)(Int64)-1),
      OrderMode(false),
      Order((UInt32)(Int32)-1),
      DeleteAfterCompressing(false),
      TimePrec((UInt32)(Int32)-1),
      FormatIndex(-1),
      EncryptHeadersIsAllowed(false),
      EncryptHeaders(false)
      {}
};

}

// Runs the compression. (paths) are absolute file-system paths of the items
// to put into the archive; (curDirPrefix) is the folder they are relative to.
// Returns the operation HRESULT; (errorMessage) holds the engine's message.
HRESULT CompressFiles(QWidget *parent,
    const QString &curDirPrefix,
    const QStringList &paths,
    const NQtCompress::CInfo &info,
    QString *errorMessage);


// ---------------------------------------------------------------------------
// Extract / test
// ---------------------------------------------------------------------------

struct CQtExtractInfo
{
  QString OutputDir;
  NExtract::NPathMode::EEnum PathMode;
  NExtract::NOverwriteMode::EEnum OverwriteMode;
  bool TestMode;
  bool ElimDup;
  UString Password;
  bool PasswordIsDefined;

  CQtExtractInfo():
      PathMode(NExtract::NPathMode::kFullPaths),
      OverwriteMode(NExtract::NOverwriteMode::kAsk),
      TestMode(false),
      ElimDup(true),
      PasswordIsDefined(false)
      {}
};

HRESULT ExtractArchives(QWidget *parent,
    const QStringList &archivePaths,
    const CQtExtractInfo &info,
    QString *errorMessage);


// ---------------------------------------------------------------------------
// Checksum
// ---------------------------------------------------------------------------

struct CQtHashResult
{
  QString Name;      // method name, e.g. "CRC32"
  QString Value;     // hash of all data
};

// Calculates the requested hash over (paths). (methods) uses 7-Zip's names
// ("CRC32", "CRC64", "SHA1", "SHA256", "BLAKE2sp", "*" for all).
HRESULT CalcChecksum(QWidget *parent,
    const QString &curDirPrefix,
    const QStringList &paths,
    const QString &methods,
    QVector<CQtHashResult> *results,
    QString *errorMessage);

#endif

// ---------------------------------------------------------------------------
// Split file
// ---------------------------------------------------------------------------

// Splits (filePath) into (volBasePath).001, .002, ... of (volumeSize) bytes.
// This is the port of UI/FileManager/PanelSplitFile.cpp; the "split" handler
// is read-only, so splitting is plain file I/O in 7-Zip too.
HRESULT SplitFile(QWidget *parent,
    const QString &filePath,
    const QString &volBasePath,
    UInt64 volumeSize,
    QString *errorMessage);
