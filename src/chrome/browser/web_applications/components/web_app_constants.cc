// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_constants.h"

namespace web_app {

const char* LaunchContainerEnumToStr(LaunchContainer launch_container) {
  switch (launch_container) {
    case LaunchContainer::kDefault:
      return "default";
    case LaunchContainer::kTab:
      return "tab";
    case LaunchContainer::kWindow:
      return "window";
  }
}

bool IsSuccess(InstallResultCode code) {
  return code == InstallResultCode::kSuccessNewInstall ||
         code == InstallResultCode::kSuccessAlreadyInstalled;
}

}  // namespace web_app
