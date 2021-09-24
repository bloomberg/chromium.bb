// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches_util.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace switches {

bool IsElasticOverscrollEnabled() {
// On macOS this value is adjusted in `UpdateScrollbarTheme()`,
// but the system default is true.
#if defined(OS_MAC)
  return true;
#elif defined(OS_WIN)
  return base::FeatureList::IsEnabled(features::kElasticOverscroll);
#elif defined(OS_ANDROID)
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
             base::android::SDK_VERSION_S &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableOverscrollEdgeEffect) &&
         base::FeatureList::IsEnabled(features::kElasticOverscroll);
#else
  return false;
#endif
}

bool IsTouchDragDropEnabled() {
  const auto* const command_line = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    defined(OS_ANDROID)
  return !command_line->HasSwitch(kDisableTouchDragDrop);
#else
  return command_line->HasSwitch(kEnableTouchDragDrop);
#endif
}

}  // namespace switches
