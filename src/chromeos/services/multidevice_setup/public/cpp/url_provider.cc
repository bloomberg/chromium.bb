// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/url_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/common/url_constants.h"

namespace chromeos {

namespace multidevice_setup {

GURL GetBoardSpecificBetterTogetherSuiteLearnMoreUrl() {
  return GURL(std::string(chrome::kMultiDeviceLearnMoreURL) +
              "&b=" + base::SysInfo::GetLsbReleaseBoard());
}

GURL GetBoardSpecificMessagesLearnMoreUrl() {
  return GURL(std::string(chrome::kAndroidMessagesLearnMoreURL) +
              "&b=" + base::SysInfo::GetLsbReleaseBoard());
}

}  // namespace multidevice_setup

}  // namespace chromeos
