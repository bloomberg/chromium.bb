// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_USER_SETTING_KEYS_H_
#define REMOTING_BASE_USER_SETTING_KEYS_H_

#include "build/build_config.h"
#include "remoting/base/user_settings.h"

namespace remoting {

#if defined(OS_WIN)

// Windows settings are stored in the registry where the key and value names use
// pascal case.

constexpr UserSettingKey kWinPreviousDefaultWebBrowserProgId =
    "PreviousDefaultBrowserProgId";

#endif  // defined(OS_WIN)

}  // namespace remoting

#endif  // REMOTING_BASE_USER_SETTING_KEYS_H_
