// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_file_handler_registration.h"

#include "base/check.h"

namespace web_app {

// This block defines stub implementations of OS specific methods for
// FileHandling. Currently, Windows, MacOSX and Desktop Linux (but not Chrome
// OS) have their own implementations.
#if defined(OS_CHROMEOS)
bool ShouldRegisterFileHandlersWithOs() {
  return false;
}

bool FileHandlingIconsSupportedByOs() {
  return false;
}

void RegisterFileHandlersWithOs(const AppId& app_id,
                                const std::string& app_name,
                                Profile* profile,
                                const apps::FileHandlers& file_handlers) {
  DCHECK(ShouldRegisterFileHandlersWithOs());
  // Stub function for OS's which don't register file handlers with the OS.
}

void UnregisterFileHandlersWithOs(const AppId& app_id,
                                  Profile* profile,
                                  ResultCallback callback) {
  DCHECK(ShouldRegisterFileHandlersWithOs());
  // Stub function for OS's which don't register file handlers with the OS.
  std::move(callback).Run(Result::kOk);
}
#endif

}  // namespace web_app
