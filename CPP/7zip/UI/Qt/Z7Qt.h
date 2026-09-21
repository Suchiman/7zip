// Qt/Z7Qt.h
//
// Glue between 7-Zip's own string/PROPVARIANT world and Qt.
// Everything in the Qt port goes through these helpers so that the 7-Zip
// engine keeps owning the data and Qt only ever sees copies.

#ifndef ZIP7_INC_QT_Z7QT_H
#define ZIP7_INC_QT_Z7QT_H

#include "../../../Common/MyString.h"
#include "../../../Common/MyCom.h"
#include "../../../Windows/PropVariant.h"

#include <QString>
#include <QDateTime>
#include <QLabel>

// ---------- string conversion ----------

inline QString Us2Q(const UString &s)
  { return QString::fromWCharArray(s.Ptr(), (int)s.Len()); }

inline QString Us2Q(const wchar_t *s)
  { return s ? QString::fromWCharArray(s) : QString(); }

inline UString Q2Us(const QString &s)
{
  UString res;
  const int len = s.length();
  if (len == 0)
    return res;
  wchar_t *dest = res.GetBuf((unsigned)len + 8);
  const int written = s.toWCharArray(dest);
  dest[written] = 0;
  res.ReleaseBuf_SetLen((unsigned)written);
  return res;
}

inline QString Fs2Q(const FString &s)  { return Us2Q(fs2us(s)); }
inline FString Q2Fs(const QString &s)  { return us2fs(Q2Us(s)); }

// ---------- PROPVARIANT ----------

// Formats one property value the way 7-Zip.s file manager shows it in a list
// column: size properties get space-grouped digits, strings get their line
// breaks flattened, and everything else goes through 7-Zip.s own
// ConvertPropertyToString2(), which is why the real PROPID has to be passed
// (it is what turns an attribute word into "D....A" or "drwxr-xr-x").
QString PropToString(const NWindows::NCOM::CPropVariant &prop, PROPID propID);
QString PropToString(const PROPVARIANT &prop, PROPID propID);

// Column caption for a PROPID, using 7-Zip's own property names.
QString GetPropName(PROPID propID, const wchar_t *nameFromFolder);

// true if the property is right aligned in a list column (numbers, times).
bool IsPropRightAligned(PROPID propID);

// "17 179 869 184"
QString NumberToStringGrouped(UInt64 v);
// "16 GB", the status-bar/notes style
QString SizeToStringShort(UInt64 v);

// Creates a QLabel whose "&" is a real accelerator for (buddy). A QLabel with
// no buddy shows the ampersand literally, so every label that carries one
// must be built this way.
QLabel *MakeLabel(const QString &text, QWidget *buddy);

// 7-Zip.s natural sort for file names (verbatim from UI/FileManager/PanelSort.cpp)
int CompareFileNames_ForFolderList(const wchar_t *s1, const wchar_t *s2);

// HRESULT -> human readable message, using 7-Zip's NError::MyFormatMessage.
QString HResultToMessage(HRESULT res);

#endif
