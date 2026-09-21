// Qt/Codecs.cpp

#include "StdAfx.h"

#include "../Agent/Agent.h"

#include "Codecs.h"
#include "Z7Qt.h"

HRESULT EnsureCodecsLoaded()
{
  return LoadGlobalCodecs();
}

CCodecs *GetCodecs()
{
  if (EnsureCodecsLoaded() != S_OK)
    return NULL;
  return g_CodecsObj;
}

QStringList GetUpdatableFormatNames()
{
  QStringList res;
  CCodecs *codecs = GetCodecs();
  if (!codecs)
    return res;
  FOR_VECTOR (i, codecs->Formats)
  {
    const CArcInfoEx &ai = codecs->Formats[i];
    if (!ai.UpdateEnabled || ai.Exts.IsEmpty())
      continue;
    res.append(Us2Q(ai.Name));
  }
  // 7-Zip puts its own format first, then the rest alphabetically.
  res.sort(Qt::CaseInsensitive);
  const int sevenZip = res.indexOf(QStringLiteral("7z"));
  if (sevenZip > 0)
    res.move(sevenZip, 0);
  return res;
}

int FindFormatByName(const QString &name)
{
  CCodecs *codecs = GetCodecs();
  if (!codecs)
    return -1;
  const UString n = Q2Us(name);
  FOR_VECTOR (i, codecs->Formats)
    if (codecs->Formats[i].Name.IsEqualTo_NoCase(n))
      return (int)i;
  return -1;
}

QString GetFormatMainExt(int formatIndex)
{
  CCodecs *codecs = GetCodecs();
  if (!codecs || formatIndex < 0 || (unsigned)formatIndex >= codecs->Formats.Size())
    return QString();
  const CArcInfoEx &ai = codecs->Formats[(unsigned)formatIndex];
  if (ai.Exts.IsEmpty())
    return Us2Q(ai.Name);
  return Us2Q(ai.Exts[0].Ext);
}

const QSet<QString> &ArchiveExtensions()
{
  static QSet<QString> exts;
  static bool filled = false;
  if (!filled)
  {
    filled = true;
    if (CCodecs *codecs = GetCodecs())
    {
      FOR_VECTOR (i, codecs->Formats)
      {
        const CArcInfoEx &ai = codecs->Formats[i];
        FOR_VECTOR (k, ai.Exts)
          exts.insert(Us2Q(ai.Exts[k].Ext).toLower());
      }
    }
  }
  return exts;
}
