// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_GUEST_OS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_GUEST_OS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/usb/cros_usb_detector.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class Profile;

namespace chromeos {
namespace settings {

class GuestOsHandler : public ::settings::SettingsPageUIHandler,
                       public chromeos::CrosUsbDeviceObserver {
 public:
  explicit GuestOsHandler(Profile* profile);
  GuestOsHandler(const GuestOsHandler&) = delete;
  GuestOsHandler& operator=(const GuestOsHandler&) = delete;
  ~GuestOsHandler() override;

  // SettingsPageUIHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // chromeos::SharedUsbDeviceObserver.
  void OnUsbDevicesChanged() override;

  // Callback for the "getSharedPathsDisplayText" message.  Converts actual
  // paths in chromeos to values suitable to display to users.
  // E.g. /home/chronos/u-<hash>/Downloads/foo => "Downloads > foo".
  void HandleGetGuestOsSharedPathsDisplayText(const base::ListValue* args);
  // Remove a specified path from being shared.
  void HandleRemoveGuestOsSharedPath(const base::ListValue* args);
  // Called when the shared USB devices page is ready.
  void HandleNotifyGuestOsSharedUsbDevicesPageReady(
      const base::ListValue* args);
  // Set the share state of a USB device.
  void HandleSetGuestOsUsbDeviceShared(const base::ListValue* args);

  void OnGuestOsSharedPathRemoved(const std::string& callback_id,
                                  const std::string& path,
                                  bool success,
                                  const std::string& failure_reason);

  Profile* profile_;
  base::ScopedObservation<CrosUsbDetector,
                          CrosUsbDeviceObserver,
                          &CrosUsbDetector::AddUsbDeviceObserver,
                          &CrosUsbDetector::RemoveUsbDeviceObserver>
      cros_usb_device_observation_{this};
  // weak_ptr_factory_ should always be last member.
  base::WeakPtrFactory<GuestOsHandler> weak_ptr_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_GUEST_OS_HANDLER_H_
