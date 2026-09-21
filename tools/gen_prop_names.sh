#!/bin/sh
# Regenerates CPP/7zip/UI/Qt/PropertyNameTable.h from the file manager's own
# resources, so the Qt port shows exactly the column captions 7-Zip shows.
#
# The resource id of a property name is 1000 + PROPID; see
# UI/FileManager/PropertyName.cpp::GetNameOfProperty().
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
fm=$root/CPP/7zip/UI/FileManager
out=$root/CPP/7zip/UI/Qt/PropertyNameTable.h

{
  cat <<'HEADER'
// Qt/PropertyNameTable.h
//
// PROPID -> column caption, generated from
//   UI/FileManager/PropertyNameRes.h + UI/FileManager/PropertyName.rc
// (the resource id of a property name is 1000 + PROPID, see
//  UI/FileManager/PropertyName.cpp::GetNameOfProperty).
// Regenerate with tools/gen_prop_names.sh after updating the 7-Zip sources.

#ifndef ZIP7_INC_QT_PROPERTY_NAME_TABLE_H
#define ZIP7_INC_QT_PROPERTY_NAME_TABLE_H

struct CPropNamePair
{
  unsigned PropID;
  const char *Name;
};

static const CPropNamePair g_PropNames[] =
{
HEADER

  awk '
    FNR==NR { if ($1=="#define" && $2 ~ /^IDS_/) id[$2]=$3; next }
    {
      line=$0
      if (match(line, /IDS_[A-Z0-9_]+[ \t]+"/)) {
        sym=substr(line, RSTART, RLENGTH); sub(/[ \t]+"$/,"",sym)
        q1=index(line,"\""); rest=substr(line,q1+1); q2=index(rest,"\"")
        txt=substr(rest,1,q2-1)
        if (sym in id) { pid=id[sym]-1000; if (pid>=0) printf "  { %s, \"%s\" }, // %s\n", pid, txt, sym }
      }
    }' "$fm/PropertyNameRes.h" "$fm/PropertyName.rc" | sort -t'{' -k2 -n

  cat <<'FOOTER'
};

#endif
FOOTER
} > "$out"

echo "wrote $out"
