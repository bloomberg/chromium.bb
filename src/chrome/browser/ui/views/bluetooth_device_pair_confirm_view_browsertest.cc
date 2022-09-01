// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

#if PAIR_BLUETOOTH_ON_DEMAND()

namespace {

constexpr char16_t kDeviceIdentifier[] = u"test-device";

}  // namespace

class BluetoothDevicePairConfirmViewBrowserTest : public DialogBrowserTest {
 public:
  BluetoothDevicePairConfirmViewBrowserTest() = default;
  BluetoothDevicePairConfirmViewBrowserTest(
      const BluetoothDevicePairConfirmViewBrowserTest&) = delete;
  BluetoothDevicePairConfirmViewBrowserTest& operator=(
      const BluetoothDevicePairConfirmViewBrowserTest&) = delete;
  ~BluetoothDevicePairConfirmViewBrowserTest() override = default;

  void ShowUi(const std::string& name) override {
    chrome::ShowBluetoothDevicePairConfirmDialog(
        browser()->tab_strip_model()->GetActiveWebContents(), kDeviceIdentifier,
        base::NullCallback());
  }
};

IN_PROC_BROWSER_TEST_F(BluetoothDevicePairConfirmViewBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

#endif  // PAIR_BLUETOOTH_ON_DEMAND()
