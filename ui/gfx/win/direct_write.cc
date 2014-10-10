// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/direct_write.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/win/windows_version.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/win/dpi.h"

namespace gfx {
namespace win {

bool ShouldUseDirectWrite() {
  // If the flag is currently on, and we're on Win7 or above, we enable
  // DirectWrite. Skia does not require the additions to DirectWrite in QFE
  // 2670838, but a simple 'better than XP' check is not enough.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  base::win::OSInfo::VersionNumber os_version =
      base::win::OSInfo::GetInstance()->version_number();
  if ((os_version.major == 6) && (os_version.minor == 1)) {
    // We can't use DirectWrite for pre-release versions of Windows 7.
    if (os_version.build < 7600)
      return false;
  }
  // If forced off, don't use it.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableDirectWrite))
    return false;

  // Can't use GDI on HiDPI.
  if (gfx::GetDPIScale() > 1.0f)
    return true;

  // Otherwise, check the field trial.
  const std::string group_name =
      base::FieldTrialList::FindFullName("DirectWrite");
  return group_name != "Disabled";
}

}  // namespace win
}  // namespace gfx
