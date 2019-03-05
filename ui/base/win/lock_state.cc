// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/lock_state.h"

#include <windows.h>
#include <wtsapi32.h>

#include "base/logging.h"
#include "base/win/windows_version.h"

namespace ui {

namespace {

// Checks if the current session is locked. Note: This API is only available
// starting Windows 7 and up. For Windows 7 level1->SessionFlags has inverted
// logic: https://msdn.microsoft.com/en-us/library/windows/desktop/ee621019.
bool IsSessionLocked() {
  DCHECK_GT(base::win::GetVersion(), base::win::VERSION_WIN7);
  bool is_locked = false;
  LPWSTR buffer = nullptr;
  DWORD buffer_length = 0;
  if (::WTSQuerySessionInformation(WTS_CURRENT_SERVER, WTS_CURRENT_SESSION,
                                   WTSSessionInfoEx, &buffer, &buffer_length) &&
      buffer_length >= sizeof(WTSINFOEXW)) {
    auto* info = reinterpret_cast<WTSINFOEXW*>(buffer);
    is_locked =
        info->Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_LOCK;
  }
  if (buffer)
    ::WTSFreeMemory(buffer);
  return is_locked;
}

// Checks if the current desktop is called "default" to see if we are on the
// lock screen or on the desktop. This returns true on Windows 10 if we are on
// the lock screen before the password prompt. Use IsSessionLocked for
// Windows 10 and above.
bool IsDesktopNameDefault() {
  DCHECK_LE(base::win::GetVersion(), base::win::VERSION_WIN7);
  bool is_locked = true;
  HDESK input_desk = ::OpenInputDesktop(0, 0, GENERIC_READ);
  if (input_desk) {
    wchar_t name[256] = {0};
    DWORD needed = 0;
    if (::GetUserObjectInformation(
            input_desk, UOI_NAME, name, sizeof(name), &needed)) {
      is_locked = lstrcmpi(name, L"default") != 0;
    }
    ::CloseDesktop(input_desk);
  }
  return is_locked;
}

}  // namespace

bool IsWorkstationLocked() {
  return base::win::GetVersion() <= base::win::VERSION_WIN7
             ? IsDesktopNameDefault()
             : IsSessionLocked();
}

}  // namespace ui
