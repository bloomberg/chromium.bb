// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chooser_controller/fake_bluetooth_chooser_controller.h"
#include "chrome/browser/chooser_controller/fake_usb_chooser_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/extensions/chooser_dialog_view.h"
#include "chrome/browser/ui/views/permission_bubble/chooser_bubble_ui.h"
#include "components/constrained_window/constrained_window_views.h"

namespace {

void ShowChooserBubble(Browser* browser,
                       std::unique_ptr<ChooserController> controller) {
  auto bubble =
      std::make_unique<ChooserBubbleUi>(browser, std::move(controller));
  bubble->Show(nullptr);
}

void ShowChooserModal(Browser* browser,
                      std::unique_ptr<ChooserController> controller) {
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  constrained_window::ShowWebModalDialogViews(
      new ChooserDialogView(std::move(controller)), web_contents);
}

void ShowChooser(const std::string& test_name,
                 Browser* browser,
                 std::unique_ptr<ChooserController> controller) {
  if (base::EndsWith(test_name, "Modal", base::CompareCase::SENSITIVE))
    ShowChooserModal(browser, std::move(controller));
  else
    ShowChooserBubble(browser, std::move(controller));
}

}  // namespace

// Invokes a dialog allowing the user to select a USB device for a web page or
// extension.
class UsbChooserBrowserTest : public DialogBrowserTest {
 public:
  UsbChooserBrowserTest() {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowChooser(name, browser(),
                std::make_unique<FakeUsbChooserController>(device_count_));
  }

 protected:
  // Number of devices to show in the chooser.
  int device_count_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbChooserBrowserTest);
};

IN_PROC_BROWSER_TEST_F(UsbChooserBrowserTest, InvokeUi_NoDevicesBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(UsbChooserBrowserTest, InvokeUi_NoDevicesModal) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(UsbChooserBrowserTest, InvokeUi_WithDevicesBubble) {
  device_count_ = 5;
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(UsbChooserBrowserTest, InvokeUi_WithDevicesModal) {
  device_count_ = 5;
  ShowAndVerifyUi();
}

// Invokes a dialog allowing the user to select a Bluetooth device for a web
// page or extension.
class BluetoothChooserBrowserTest : public DialogBrowserTest {
 public:
  BluetoothChooserBrowserTest()
      : status_(FakeBluetoothChooserController::BluetoothStatus::UNAVAILABLE) {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto controller =
        std::make_unique<FakeBluetoothChooserController>(std::move(devices_));
    auto* controller_unowned = controller.get();
    ShowChooser(name, browser(), std::move(controller));
    controller_unowned->SetBluetoothStatus(status_);
  }

  void set_status(FakeBluetoothChooserController::BluetoothStatus status) {
    status_ = status;
  }

  void AddDeviceForAllStrengths() {
    devices_.push_back({"Device with Strength 0",
                        FakeBluetoothChooserController::NOT_CONNECTED,
                        FakeBluetoothChooserController::NOT_PAIRED,
                        FakeBluetoothChooserController::kSignalStrengthLevel0});
    devices_.push_back({"Device with Strength 1",
                        FakeBluetoothChooserController::NOT_CONNECTED,
                        FakeBluetoothChooserController::NOT_PAIRED,
                        FakeBluetoothChooserController::kSignalStrengthLevel1});
    devices_.push_back({"Device with Strength 2",
                        FakeBluetoothChooserController::NOT_CONNECTED,
                        FakeBluetoothChooserController::NOT_PAIRED,
                        FakeBluetoothChooserController::kSignalStrengthLevel2});
    devices_.push_back({"Device with Strength 3",
                        FakeBluetoothChooserController::NOT_CONNECTED,
                        FakeBluetoothChooserController::NOT_PAIRED,
                        FakeBluetoothChooserController::kSignalStrengthLevel3});
    devices_.push_back({"Device with Strength 4",
                        FakeBluetoothChooserController::NOT_CONNECTED,
                        FakeBluetoothChooserController::NOT_PAIRED,
                        FakeBluetoothChooserController::kSignalStrengthLevel4});
  }

  void AddConnectedDevice() {
    devices_.push_back({"Connected Device",
                        FakeBluetoothChooserController::CONNECTED,
                        FakeBluetoothChooserController::NOT_PAIRED,
                        FakeBluetoothChooserController::kSignalStrengthLevel4});
  }

  void AddPairedDevice() {
    devices_.push_back({"Paired Device",
                        FakeBluetoothChooserController::NOT_CONNECTED,
                        FakeBluetoothChooserController::PAIRED,
                        FakeBluetoothChooserController::kSignalStrengthLevel4});
  }

 private:
  FakeBluetoothChooserController::BluetoothStatus status_;
  std::vector<FakeBluetoothChooserController::FakeDevice> devices_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothChooserBrowserTest);
};

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest,
                       InvokeUi_UnavailableBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_UnavailableModal) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_NoDevicesBubble) {
  set_status(FakeBluetoothChooserController::BluetoothStatus::IDLE);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_NoDevicesModal) {
  set_status(FakeBluetoothChooserController::BluetoothStatus::IDLE);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_ScanningBubble) {
  set_status(FakeBluetoothChooserController::BluetoothStatus::SCANNING);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_ScanningModal) {
  set_status(FakeBluetoothChooserController::BluetoothStatus::SCANNING);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest,
                       InvokeUi_ScanningWithDevicesBubble) {
  set_status(FakeBluetoothChooserController::BluetoothStatus::SCANNING);
  AddDeviceForAllStrengths();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest,
                       InvokeUi_ScanningWithDevicesModal) {
  set_status(FakeBluetoothChooserController::BluetoothStatus::SCANNING);
  AddDeviceForAllStrengths();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_ConnectedBubble) {
  set_status(FakeBluetoothChooserController::BluetoothStatus::IDLE);
  AddConnectedDevice();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_ConnectedModal) {
  set_status(FakeBluetoothChooserController::BluetoothStatus::IDLE);
  AddConnectedDevice();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_PairedBubble) {
  set_status(FakeBluetoothChooserController::BluetoothStatus::IDLE);
  AddPairedDevice();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BluetoothChooserBrowserTest, InvokeUi_PairedModal) {
  set_status(FakeBluetoothChooserController::BluetoothStatus::IDLE);
  AddPairedDevice();
  ShowAndVerifyUi();
}
