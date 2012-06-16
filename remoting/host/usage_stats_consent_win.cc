// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/usage_stats_consent.h"

#include <windows.h>
#include <string>

#include "base/stringprintf.h"
#include "base/win/registry.h"
#include "remoting/host/constants.h"

namespace {

// The following strings are used to construct the registry key names where
// we record whether the user has consented to crash dump collection.
const wchar_t kOmahaClientStateKeyFormat[] =
    L"Software\\Google\\Update\\%ls\\%ls";
const wchar_t kOmahaClientState[] = L"ClientState";
const wchar_t kOmahaClientStateMedium[] = L"ClientStateMedium";
const wchar_t kOmahaUsagestatsValue[] = L"usagestats";

LONG ReadUsageStatsValue(const wchar_t* state_key, DWORD* usagestats_out) {
  std::wstring client_state = StringPrintf(kOmahaClientStateKeyFormat,
                                           state_key,
                                           remoting::kHostOmahaAppid);
  base::win::RegKey key;
  LONG result = key.Open(HKEY_LOCAL_MACHINE, client_state.c_str(), KEY_READ);
  if (result != ERROR_SUCCESS) {
    return result;
  }

  return key.ReadValueDW(kOmahaUsagestatsValue, usagestats_out);
}

}  // namespace

namespace remoting {

bool IsCrashReportingEnabled() {
  // The user's consent to collect crash dumps is recored as the "usagestats"
  // value in the ClientState or ClientStateMedium key. Probe
  // the ClientStateMedium key first.
  DWORD value = 0;
  if (ReadUsageStatsValue(kOmahaClientStateMedium, &value) == ERROR_SUCCESS) {
    return value != 0;
  }
  if (ReadUsageStatsValue(kOmahaClientState, &value) == ERROR_SUCCESS) {
    return value != 0;
  }

  // Do not collect anything unless the user has explicitly allowed it.
  return false;
}

}  // namespace remoting
