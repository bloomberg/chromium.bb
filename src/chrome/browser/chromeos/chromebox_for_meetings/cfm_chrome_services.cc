// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromebox_for_meetings/cfm_chrome_services.h"

#include "chrome/browser/chromeos/chromebox_for_meetings/browser/cfm_browser_service.h"
#include "chromeos/components/chromebox_for_meetings/features/features.h"
#include "chromeos/dbus/chromebox_for_meetings/cfm_hotline_client.h"

namespace chromeos {
namespace cfm {

void InitializeCfmServices() {
  if (!features::IsCfmMojoEnabled() || !CfmHotlineClient::Get()) {
    return;
  }

  CfmHotlineClient::Get()->AddObserver(CfmBrowserService::GetInstance());
}

}  // namespace cfm
}  // namespace chromeos
