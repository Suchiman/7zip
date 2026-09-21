// Qt/WorkDirSettings.cpp
//
// NWorkDir::CInfo is declared in UI/Common/ZipRegistry.h and implemented in
// ZipRegistry.cpp against the Windows registry, which does not build here.
// The Qt port keeps the same interface and stores the setting with QSettings,
// so UI/Common/WorkDir.cpp (used when updating an archive in place) works
// unchanged and stays in sync with the Folders page of the Options dialog.

#include "StdAfx.h"

#include "../Common/ZipRegistry.h"

#include "OptionsDialog.h"
#include "Z7Qt.h"

namespace NWorkDir {

void CInfo::Load()
{
  SetDefault();
  const CFmSettings &s = GlobalFmSettings();
  switch (s.WorkDirMode)
  {
    case 1:  Mode = NMode::kCurrent; break;
    case 2:  Mode = NMode::kSpecified; break;
    default: Mode = NMode::kSystem; break;
  }
  Path = Q2Fs(s.WorkDirPath);
  ForRemovableOnly = false;  // no removable-drive concept here
}

void CInfo::Save() const
{
  CFmSettings &s = GlobalFmSettings();
  switch (Mode)
  {
    case NMode::kCurrent:   s.WorkDirMode = 1; break;
    case NMode::kSpecified: s.WorkDirMode = 2; break;
    default:                s.WorkDirMode = 0; break;
  }
  s.WorkDirPath = Fs2Q(Path);
  s.Save();
}

}
