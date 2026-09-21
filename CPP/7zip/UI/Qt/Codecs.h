// Qt/Codecs.h
//
// One place that owns the global CCodecs instance (LoadGlobalCodecs() from
// UI/Agent/Agent.cpp) and answers the format questions the UI asks:
// which formats can be created, which extensions are archives, ...

#ifndef ZIP7_INC_QT_CODECS_H
#define ZIP7_INC_QT_CODECS_H

#include "../Common/LoadCodecs.h"

#include <QSet>
#include <QString>
#include <QStringList>

// Loads the codecs once. Returns the 7-Zip HRESULT.
HRESULT EnsureCodecsLoaded();
CCodecs *GetCodecs();

// Format names that support creating/updating archives ("7z", "zip", ...),
// in the order the Add dialog lists them.
QStringList GetUpdatableFormatNames();
// Index into CCodecs::Formats for a format name, -1 if unknown.
int FindFormatByName(const QString &name);
// The main extension of a format ("7z", "zip", "tar", ...).
QString GetFormatMainExt(int formatIndex);

const QSet<QString> &ArchiveExtensions();

#endif
