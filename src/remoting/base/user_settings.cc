// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/user_settings.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "remoting/base/user_settings_win.h"
#endif

namespace remoting {

UserSettings::UserSettings() = default;

UserSettings::~UserSettings() = default;

UserSettings* UserSettings::GetInstance() {
#if defined(OS_WIN)
  static base::NoDestructor<UserSettingsWin> instance;
  return instance.get();
#else
  NOTREACHED();
  return nullptr;
#endif
}

}  // namespace remoting
