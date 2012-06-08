// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/breakpad.h"

#include <windows.h>

#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/win/registry.h"
#include "remoting/host/constants.h"

namespace {

// The following strings are used to construct the registry key names where
// the user's consent to collect crash dumps is recorded.
const wchar_t kOmahaClientStateKeyFormat[] = L"Google\\Update\\%ls\\%ls";
const wchar_t kOmahaClientState[] = L"ClientState";
const wchar_t kOmahaClientStateMedium[] = L"ClientStateMedium";
const wchar_t kOmahaUsagestatsValue[] = L"usagestats";

}  // namespace

namespace remoting {

bool IsCrashReportingEnabled() {
  // The user's consent to collect crash dumps is recored as the "usagestats"
  // value in the ClientState or ClientStateMedium key. Probe
  // the ClientStateMedium key first.
  string16 client_state = StringPrintf(kOmahaClientStateKeyFormat,
                                       kOmahaClientStateMedium,
                                       remoting::kHostOmahaAppid);
  base::win::RegKey key;
  LONG result = key.Open(HKEY_LOCAL_MACHINE, client_state.c_str(), KEY_READ);
  if (result == ERROR_SUCCESS) {
    DWORD value = 0;
    result = key.ReadValueDW(kOmahaUsagestatsValue, &value);
    if (result == ERROR_SUCCESS) {
      return value != 0;
    }
  }

  client_state = StringPrintf(kOmahaClientStateKeyFormat,
                              kOmahaClientState,
                              remoting::kHostOmahaAppid);
  result = key.Open(HKEY_LOCAL_MACHINE, client_state.c_str(), KEY_READ);
  if (result == ERROR_SUCCESS) {
    DWORD value = 0;
    result = key.ReadValueDW(kOmahaUsagestatsValue, &value);
    if (result == ERROR_SUCCESS) {
      return value != 0;
    }
  }

  // Do not collect anything unless the user has explicitly allowed it.
  return false;
}

}  // namespace remoting
