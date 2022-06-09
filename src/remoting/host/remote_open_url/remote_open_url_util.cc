// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/remote_open_url_util.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "base/win/windows_version.h"
#endif

namespace remoting {

#if defined(OS_WIN)

#if defined(OFFICIAL_BUILD)
const wchar_t kUrlForwarderRegisteredAppName[] =
    L"Chrome Remote Desktop URL Forwarder";
#else
const wchar_t kUrlForwarderRegisteredAppName[] = L"Chromoting URL Forwarder";
#endif

const wchar_t kRegisteredApplicationsKeyName[] =
    L"SOFTWARE\\RegisteredApplications";

#endif  // defined (OS_WIN)

bool IsRemoteOpenUrlSupported() {
#if defined(OS_LINUX)
  return true;
#elif defined(OS_WIN)
  // The modern default apps settings dialog is only available to Windows 8+.
  // Given older Windows versions are EOL, we only advertise the feature on
  // Windows 8+.
  if (base::win::GetVersion() < base::win::Version::WIN8) {
    return false;
  }
  // The MSI installs the ProgID and capabilities into registry, but not the
  // entry in RegisteredApplications, which must be applied out of band to
  // enable the feature.
  base::win::RegKey registered_apps_key;
  LONG result = registered_apps_key.Open(
      HKEY_LOCAL_MACHINE, kRegisteredApplicationsKeyName, KEY_READ);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to determine whether URL forwarding is supported "
                  "since registry key HKLM\\"
               << kRegisteredApplicationsKeyName
               << "cannot be opened. Result: " << result;
    return false;
  }
  return registered_apps_key.HasValue(kUrlForwarderRegisteredAppName);
#else
  // Not supported on other platforms.
  return false;
#endif
}

}  // namespace remoting
