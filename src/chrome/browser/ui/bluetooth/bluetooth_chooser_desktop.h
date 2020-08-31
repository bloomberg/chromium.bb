// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_DESKTOP_H_
#define CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_DESKTOP_H_

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/bluetooth_chooser.h"

class BluetoothChooserController;

// Represents a Bluetooth chooser to ask the user to select a Bluetooth
// device from a list of options. This implementation is for desktop.
// BluetoothChooserAndroid implements the mobile part.
class BluetoothChooserDesktop : public content::BluetoothChooser {
 public:
  BluetoothChooserDesktop(
      BluetoothChooserController* bluetooth_chooser_controller,
      base::OnceClosure&& close_closure);
  ~BluetoothChooserDesktop() override;

  // BluetoothChooser:
  void SetAdapterPresence(AdapterPresence presence) override;
  void ShowDiscoveryState(DiscoveryState state) override;
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const base::string16& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override;

 private:
  // Weak. DeviceChooserContentView[Cocoa] owns it.
  BluetoothChooserController* bluetooth_chooser_controller_;

  // Closes the displayed UI if it is still open. Used to ensure the bubble
  // closes if this controller is torn down.
  base::OnceClosure close_closure_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothChooserDesktop);
};

#endif  // CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_DESKTOP_H_
