// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_BLUETOOTH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_BLUETOOTH_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {
namespace settings {

// Chrome OS Bluetooth subpage UI handler.
class BluetoothHandler : public ::settings::SettingsPageUIHandler {
 public:
  BluetoothHandler();
  BluetoothHandler(const BluetoothHandler&) = delete;
  BluetoothHandler& operator=(const BluetoothHandler&) = delete;
  ~BluetoothHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class BluetoothHandlerTest;

  void BluetoothDeviceAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  void HandleIsDeviceBlockedByPolicy(const base::ListValue* args);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  base::WeakPtrFactory<BluetoothHandler> weak_ptr_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_BLUETOOTH_HANDLER_H_
