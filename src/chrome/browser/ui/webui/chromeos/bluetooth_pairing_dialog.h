// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_BLUETOOTH_PAIRING_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_BLUETOOTH_PAIRING_DIALOG_H_

#include <string>

#include "base/strings/string_piece.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

class BluetoothPairingDialog : public SystemWebDialogDelegate {
 public:
  BluetoothPairingDialog(const BluetoothPairingDialog&) = delete;
  BluetoothPairingDialog& operator=(const BluetoothPairingDialog&) = delete;

  // Show the Bluetooth pairing dialog. When provided, |device_address| is the
  // unique device address that the dialog should attempt to pair with and
  // should be in the form "XX:XX:XX:XX:XX:XX". When |device_address| is not
  // provided the dialog will show the device list instead. The returned object
  // manages its own lifetime, for more information see
  // chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h.
  static SystemWebDialogDelegate* ShowDialog(
      absl::optional<base::StringPiece> device_address = absl::nullopt);

  ~BluetoothPairingDialog() override;

 protected:
  BluetoothPairingDialog(
      const std::string& dialog_id,
      absl::optional<base::StringPiece> canonical_device_address);

 private:
  // SystemWebDialogDelegate
  const std::string& Id() override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;

  // The canonical Bluetooth address of a device when pairing a specific device,
  // otherwise |kChromeUIBluetoothPairingURL|.
  std::string dialog_id_;

  base::DictionaryValue device_data_;
};

// A WebUI to host the Bluetooth device pairing web UI.
class BluetoothPairingDialogUI : public ui::MojoWebDialogUI {
 public:
  explicit BluetoothPairingDialogUI(content::WebUI* web_ui);

  BluetoothPairingDialogUI(const BluetoothPairingDialogUI&) = delete;
  BluetoothPairingDialogUI& operator=(const BluetoothPairingDialogUI&) = delete;

  ~BluetoothPairingDialogUI() override;

  // Instantiates implementor of the mojom::CrosBluetoothConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          chromeos::bluetooth_config::mojom::CrosBluetoothConfig> receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_BLUETOOTH_PAIRING_DIALOG_H_
