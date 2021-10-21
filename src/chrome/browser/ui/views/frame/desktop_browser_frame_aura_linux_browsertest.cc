// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/ozone/public/ozone_platform.h"

using DesktopBrowserFrameAuraLinuxTest = InProcessBrowserTest;
using SupportsSsdForTest =
    ui::OzonePlatform::PlatformRuntimeProperties::SupportsSsdForTest;

// Tests that DesktopBrowserFrameAuraLinux::UseCustomFrame() returns the correct
// value that respects 1) the current value of the user preference and
// 2) capabilities of the platform.
// Also tests the regression found in crbug.com/1243937.
IN_PROC_BROWSER_TEST_F(DesktopBrowserFrameAuraLinuxTest, UseCustomFrame) {
  const BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  const DesktopBrowserFrameAuraLinux* const frame =
      static_cast<DesktopBrowserFrameAuraLinux*>(
          browser_view->frame()->native_browser_frame());
  auto* pref_service = browser_view->browser()->profile()->GetPrefs();

    // Try overriding the runtime platform property that indicates whether the
    // platform supports server-side window decorations.  For each variant,
    // check what is the effective value of the property (the platform may
    // ignore the override), then pass through variants for the user preference
    // and check what DesktopBrowserFrameAuraLinux::UseCustomFrame() returns
    // finally.
    auto* const platform = ui::OzonePlatform::GetInstance();
    for (const auto ssd_support_override :
         {SupportsSsdForTest::kYes, SupportsSsdForTest::kNo}) {
      ui::OzonePlatform::PlatformRuntimeProperties::
          override_supports_ssd_for_test = ssd_support_override;

      if (platform->GetPlatformRuntimeProperties()
              .supports_server_side_window_decorations) {
        // This platform either supports overriding the property or supports SSD
        // always.
        // UseCustomFrame() should return what the user preference suggests.
        for (const auto setting : {true, false}) {
          pref_service->SetBoolean(prefs::kUseCustomChromeFrame, setting);
          EXPECT_EQ(frame->UseCustomFrame(), setting)
              << " when setting is " << setting;
        }
      } else {
        // This platform either does not support overriding the property or does
        // not support SSD.
        // UseCustomFrame() should return true always.
        for (const auto setting : {true, false}) {
          pref_service->SetBoolean(prefs::kUseCustomChromeFrame, setting);
          EXPECT_TRUE(frame->UseCustomFrame())
              << " when setting is " << setting;
        }
      }
    }

    // Reset the override.
    ui::OzonePlatform::PlatformRuntimeProperties::
        override_supports_ssd_for_test = SupportsSsdForTest::kNotSet;
}
