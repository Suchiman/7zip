// Qt/Z7Qt.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/PropVariantConv.h"
#include "../../PropID.h"
#include "../Common/PropIDUtils.h"

#include "Z7Qt.h"
#include "PropertyNameTable.h"

using namespace NWindows;

// ---------------------------------------------------------------------------
// Verbatim from UI/FileManager/PanelListNotify.cpp: 7-Zip groups the digits of
// a size with plain spaces ("17 179 869 184") instead of a locale separator.
// ---------------------------------------------------------------------------

#define INT_TO_STR_SPEC(v) \
  while (v >= 10) { temp[i++] = (Byte)('0' + (unsigned)(v % 10)); v /= 10; } \
  *s++ = (Byte)('0' + (unsigned)v);

static void ConvertSizeToString(UInt64 val, wchar_t *s) throw()
{
  Byte temp[32];
  unsigned i = 0;

  if (val <= (UInt32)0xFFFFFFFF)
  {
    UInt32 val32 = (UInt32)val;
    INT_TO_STR_SPEC(val32)
  }
  else
  {
    INT_TO_STR_SPEC(val)
  }

  if (i < 3)
  {
    if (i != 0)
    {
      *s++ = temp[(size_t)i - 1];
      if (i == 2)
        *s++ = temp[0];
    }
    *s = 0;
    return;
  }

  unsigned r = i % 3;
  if (r != 0)
  {
    s[0] = temp[--i];
    if (r == 2)
      s[1] = temp[--i];
    s += r;
  }

  do
  {
    s[0] = ' ';
    s[1] = temp[(size_t)i - 1];
    s[2] = temp[(size_t)i - 2];
    s[3] = temp[(size_t)i - 3];
    s += 4;
  }
  while (i -= 3);

  *s = 0;
}

QString NumberToStringGrouped(UInt64 v)
{
  wchar_t s[32];
  ConvertSizeToString(v, s);
  return Us2Q(s);
}

QString SizeToStringShort(UInt64 v)
{
  static const char * const kUnits[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB" };
  unsigned u = 0;
  UInt64 scaled = v;
  while (scaled >= 9999 && u + 1 < (unsigned)Z7_ARRAY_SIZE(kUnits))
  {
    scaled = (scaled + 512) >> 10;
    u++;
  }
  return QString::number((qulonglong)scaled) + QLatin1Char(' ') + QLatin1String(kUnits[u]);
}

// Verbatim from UI/FileManager/PanelListNotify.cpp.
static bool IsSizeProp(UInt32 propID) throw()
{
  switch (propID)
  {
    case kpidSize:
    case kpidPackSize:
    case kpidNumSubDirs:
    case kpidNumSubFiles:
    case kpidOffset:
    case kpidLinks:
    case kpidNumBlocks:
    case kpidNumVolumes:
    case kpidPhySize:
    case kpidHeadersSize:
    case kpidTotalSize:
    case kpidFreeSpace:
    case kpidClusterSize:
    case kpidNumErrors:
    case kpidNumStreams:
    case kpidNumAltStreams:
    case kpidAltStreamsSize:
    case kpidVirtualSize:
    case kpidUnpackSize:
    case kpidTotalPhySize:
    case kpidTailSize:
    case kpidEmbeddedStubSize:
      return true;
  }
  return false;
}

bool IsPropRightAligned(PROPID propID)
{
  return IsSizeProp(propID);
}

QString PropToString(const PROPVARIANT &prop, PROPID propID)
{
  if (prop.vt == VT_EMPTY)
    return QString();

  if (IsSizeProp(propID)
      && (prop.vt == VT_UI8 || prop.vt == VT_UI4 || prop.vt == VT_UI2))
  {
    UInt64 v = 0;
    ConvertPropVariantToUInt64(prop, v);
    return NumberToStringGrouped(v);
  }

  if (prop.vt == VT_BSTR)
  {
    // 7-Zip flattens line breaks so that one item stays on one list row.
    QString s = QString::fromWCharArray(prop.bstrVal);
    s.replace(QLatin1Char('\n'), QLatin1Char(' '));
    s.replace(QLatin1Char('\r'), QLatin1Char(' '));
    return s;
  }

  UString dest;
  // kTimestampPrintLevel_MIN is the file manager's default timestamp level.
  // (propID) matters here: it is what makes 7-Zip print an attribute word as
  // "D....A" and a posix mode as "drwxr-xr-x" instead of a raw number.
  ConvertPropertyToString2(dest, prop, propID, kTimestampPrintLevel_MIN);
  return Us2Q(dest);
}

QString PropToString(const NWindows::NCOM::CPropVariant &prop, PROPID propID)
{
  return PropToString((const PROPVARIANT &)prop, propID);
}

QString GetPropName(PROPID propID, const wchar_t *nameFromFolder)
{
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_PropNames); i++)
    if (g_PropNames[i].PropID == propID)
      return QString::fromLatin1(g_PropNames[i].Name);
  if (nameFromFolder && *nameFromFolder)
    return Us2Q(nameFromFolder);
  return QString::number((uint)propID);
}

// ---------------------------------------------------------------------------
// Verbatim from UI/FileManager/PanelSort.cpp.
// ---------------------------------------------------------------------------

int CompareFileNames_ForFolderList(const wchar_t *s1, const wchar_t *s2)
{
  for (;;)
  {
    wchar_t c1 = *s1;
    wchar_t c2 = *s2;
    if ((c1 >= '0' && c1 <= '9') &&
        (c2 >= '0' && c2 <= '9'))
    {
      for (; *s1 == '0'; s1++);
      for (; *s2 == '0'; s2++);
      size_t len1 = 0;
      size_t len2 = 0;
      for (; (s1[len1] >= '0' && s1[len1] <= '9'); len1++);
      for (; (s2[len2] >= '0' && s2[len2] <= '9'); len2++);
      if (len1 < len2) return -1;
      if (len1 > len2) return 1;
      for (; len1 > 0; s1++, s2++, len1--)
      {
        if (*s1 == *s2) continue;
        return (*s1 < *s2) ? -1 : 1;
      }
      c1 = *s1;
      c2 = *s2;
    }
    s1++;
    s2++;
    if (c1 != c2)
    {
      const wchar_t u1 = MyCharUpper(c1);
      const wchar_t u2 = MyCharUpper(c2);
      if (u1 < u2) return -1;
      if (u1 > u2) return 1;
    }
    if (c1 == 0) return 0;
  }
}

QLabel *MakeLabel(const QString &text, QWidget *buddy)
{
  QLabel *label = new QLabel(text);
  label->setBuddy(buddy);
  return label;
}

QString HResultToMessage(HRESULT res)
{
  if (res == E_ABORT)
    return QObject::tr("Operation was cancelled");
  if (res == E_OUTOFMEMORY)
    return QObject::tr("There is not enough memory");
  return Us2Q(NError::MyFormatMessage(res));
}
