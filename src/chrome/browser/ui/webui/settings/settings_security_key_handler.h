// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURITY_KEY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURITY_KEY_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/fido/fido_constants.h"

namespace base {
class ListValue;
}

namespace device {
class SetPINRequestHandler;
class ResetRequestHandler;
}  // namespace device

namespace settings {

// SecurityKeysHandler processes messages from the "Security Keys" section of a
// settings page. An instance of this class is created for each settings tab and
// is destroyed when the tab is closed. See the comments in
// security_keys_browser_proxy.js about the interface.
class SecurityKeysHandler : public SettingsPageUIHandler {
 public:
  SecurityKeysHandler();
  ~SecurityKeysHandler() override;

  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  enum class State {
    kNone,

    kStartSetPIN,
    kGatherNewPIN,
    kGatherChangePIN,
    kSettingPIN,

    kStartReset,
    kWaitingForResetNoCallbackYet,
    kWaitingForResetHaveCallback,
    kWaitingForCompleteReset,
  };

  void Close();
  void HandleStartSetPIN(const base::ListValue* args);
  void OnGatherPIN(base::Optional<int64_t> num_retries);
  void OnSetPINComplete(device::CtapDeviceResponseCode code);
  void HandleSetPIN(const base::ListValue* args);
  void HandleReset(const base::ListValue* args);
  void OnResetSent();
  void HandleCompleteReset(const base::ListValue* args);
  void OnResetFinished(device::CtapDeviceResponseCode result);
  void HandleClose(const base::ListValue* args);

  State state_;
  std::unique_ptr<device::SetPINRequestHandler> set_pin_;
  std::unique_ptr<device::ResetRequestHandler> reset_;
  base::Optional<device::CtapDeviceResponseCode> reset_result_;
  std::string callback_id_;
  std::unique_ptr<base::WeakPtrFactory<SecurityKeysHandler>> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SecurityKeysHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_SECURITY_KEY_HANDLER_H_
