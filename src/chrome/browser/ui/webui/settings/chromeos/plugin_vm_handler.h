// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PLUGIN_VM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PLUGIN_VM_HANDLER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/chromeos/usb/cros_usb_detector.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class Profile;

namespace chromeos {
namespace settings {

class PluginVmHandler : public ::settings::SettingsPageUIHandler,
                        public chromeos::CrosUsbDeviceObserver {
 public:
  explicit PluginVmHandler(Profile* profile);
  ~PluginVmHandler() override;

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
  void HandleGetPluginVmSharedPathsDisplayText(const base::ListValue* args);
  // Remove a specified path from being shared.
  void HandleRemovePluginVmSharedPath(const base::ListValue* args);
  // Called when the shared USB devices page is ready.
  void HandleNotifyPluginVmSharedUsbDevicesPageReady(
      const base::ListValue* args);
  // Set the share state of a USB device.
  void HandleSetPluginVmUsbDeviceShared(const base::ListValue* args);
  // Checks if Plugin VM would need to be relaunched if the proposed changes are
  // made.
  void HandleIsRelaunchNeededForNewPermissions(const base::ListValue* args);
  // Relaunches Plugin VM.
  void HandleRelaunchPluginVm(const base::ListValue* args);

  void OnPluginVmSharedPathRemoved(const std::string& callback_id,
                                   const std::string& path,
                                   bool success,
                                   const std::string& failure_reason);

  Profile* profile_;
  ScopedObserver<CrosUsbDetector,
                 CrosUsbDeviceObserver,
                 &CrosUsbDetector::AddUsbDeviceObserver,
                 &CrosUsbDetector::RemoveUsbDeviceObserver>
      cros_usb_device_observer_{this};
  // weak_ptr_factory_ should always be last member.
  base::WeakPtrFactory<PluginVmHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PluginVmHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PLUGIN_VM_HANDLER_H_
