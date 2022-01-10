// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CROSTINI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CROSTINI_HANDLER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crostini/crostini_export_import.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_port_forwarder.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace crostini {
enum class CrostiniResult;
struct CrostiniDiskInfo;
}  // namespace crostini

namespace chromeos {
namespace settings {

class CrostiniHandler : public ::settings::SettingsPageUIHandler,
                        public crostini::CrostiniDialogStatusObserver,
                        public crostini::CrostiniExportImport::Observer,
                        public crostini::CrostiniContainerPropertiesObserver,
                        public crostini::CrostiniPortForwarder::Observer,
                        public crostini::ContainerStartedObserver,
                        public crostini::ContainerShutdownObserver {
 public:
  explicit CrostiniHandler(Profile* profile);

  CrostiniHandler(const CrostiniHandler&) = delete;
  CrostiniHandler& operator=(const CrostiniHandler&) = delete;

  ~CrostiniHandler() override;

  // SettingsPageUIHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleRequestCrostiniInstallerView(base::Value::ConstListView args);
  void HandleRequestRemoveCrostini(base::Value::ConstListView args);
  // Export the crostini container.
  void HandleExportCrostiniContainer(base::Value::ConstListView args);
  // Import the crostini container.
  void HandleImportCrostiniContainer(base::Value::ConstListView args);
  // Handle a request for the CrostiniInstallerView status.
  void HandleCrostiniInstallerStatusRequest(base::Value::ConstListView args);
  // crostini::CrostiniDialogStatusObserver
  void OnCrostiniDialogStatusChanged(crostini::DialogType dialog_type,
                                     bool open) override;
  // crostini::CrostiniContainerPropertiesObserver
  void OnContainerOsReleaseChanged(const crostini::ContainerId& container_id,
                                   bool can_upgrade) override;
  // Handle a request for the CrostiniExportImport operation status.
  void HandleCrostiniExportImportOperationStatusRequest(
      base::Value::ConstListView args);
  // CrostiniExportImport::Observer:
  void OnCrostiniExportImportOperationStatusChanged(bool in_progress) override;
  // Handle a request for querying status of ARC adb sideloading.
  void HandleQueryArcAdbRequest(base::Value::ConstListView args);
  // Handle a request for enabling adb sideloading in ARC.
  void HandleEnableArcAdbRequest(base::Value::ConstListView args);
  // Called after establishing whether enabling adb sideloading is allowed for
  // the user and device
  void OnCanEnableArcAdbSideloading(bool can_change_adb_sideloading);
  // Handle a request for disabling adb sideloading in ARC.
  void HandleDisableArcAdbRequest(base::Value::ConstListView args);
  // Called after establishing whether disabling adb sideloading is allowed for
  // the user and device
  void OnCanDisableArcAdbSideloading(bool can_change_adb_sideloading);
  // Launch the Crostini terminal, with |intent| specifying any non-default
  // container id.
  void LaunchTerminal(apps::mojom::IntentPtr intent);
  // Handle a request for showing the container upgrade view.
  void HandleRequestContainerUpgradeView(base::Value::ConstListView args);
  // Callback of HandleQueryArcAdbRequest.
  void OnQueryAdbSideload(
      SessionManagerClient::AdbSideloadResponseCode response_code,
      bool enabled);
  // Handle a request for the CrostiniUpgraderDialog status.
  void HandleCrostiniUpgraderDialogStatusRequest(
      base::Value::ConstListView args);
  // Handle a request for the availability of a container upgrade.
  void HandleCrostiniContainerUpgradeAvailableRequest(
      base::Value::ConstListView args);
  // Handles a request for forwarding a new port.
  void HandleAddCrostiniPortForward(base::Value::ConstListView args);
  // Handles a request for removing one port.
  void HandleRemoveCrostiniPortForward(base::Value::ConstListView args);
  // Handles a request for removing all ports.
  void HandleRemoveAllCrostiniPortForwards(base::Value::ConstListView args);
  // CrostiniPortForwarder::Observer.
  void OnActivePortsChanged(const base::ListValue& activePorts) override;
  // Handles a request for activating an existing port.
  void HandleActivateCrostiniPortForward(base::Value::ConstListView args);
  // Handles a request for deactivating an existing port.
  void HandleDeactivateCrostiniPortForward(base::Value::ConstListView args);
  // Callback of port forwarding requests.
  void OnPortForwardComplete(std::string callback_id, bool success);
  // Fetches disk info for a VM, can be slow (seconds).
  void HandleGetCrostiniDiskInfo(base::Value::ConstListView args);
  void ResolveGetCrostiniDiskInfoCallback(
      const std::string& callback_id,
      std::unique_ptr<crostini::CrostiniDiskInfo> disk_info);
  // Handles a request to resize a Crostini disk.
  void HandleResizeCrostiniDisk(base::Value::ConstListView args);
  void ResolveResizeCrostiniDiskCallback(const std::string& callback_id,
                                         bool succeeded);
  // Returns a list of currently forwarded ports.
  void HandleGetCrostiniActivePorts(base::Value::ConstListView args);
  // Checks if Crostini is running.
  void HandleCheckCrostiniIsRunning(base::Value::ConstListView args);
  // crostini::ContainerStartedObserver
  void OnContainerStarted(const crostini::ContainerId& container_id) override;
  // crostini::ContainerShutdownObserver
  void OnContainerShutdown(const crostini::ContainerId& container_id) override;
  // Handles a request to shut down Crostini.
  void HandleShutdownCrostini(base::Value::ConstListView args);
  // Handle a request for checking permission for changing ARC adb sideloading.
  void HandleCanChangeArcAdbSideloadingRequest(base::Value::ConstListView args);
  // Get permission of changing ARC adb sideloading
  void FetchCanChangeAdbSideloading();
  // Callback of FetchCanChangeAdbSideloading.
  void OnCanChangeArcAdbSideloading(bool can_change_arc_adb_sideloading);
  // Handle a request for creating a container
  void HandleCreateContainer(base::Value::ConstListView args);
  // Handle a request for deleting a container
  void HandleDeleteContainer(base::Value::ConstListView args);
  // Handle a request for the running info of all known containers
  void HandleRequestContainerInfo(base::Value::ConstListView args);
  // Handle a request to set the badge color for a container
  void HandleSetContainerBadgeColor(base::Value::ConstListView args);
  // Handle a request to stop a running lxd container
  void HandleStopContainer(base::Value::ConstListView args);

  Profile* profile_;
  base::CallbackListSubscription adb_sideloading_device_policy_subscription_;
  PrefChangeRegistrar pref_change_registrar_;

  // |handler_weak_ptr_factory_| is used for callbacks handling messages from
  // the WebUI page, and certain observers. These callbacks usually have the
  // same lifecycle as CrostiniHandler.
  base::WeakPtrFactory<CrostiniHandler> handler_weak_ptr_factory_{this};
  // |callback_weak_ptr_factory_| is used for callbacks passed into crostini
  // functions, which run JS functions/callbacks. However, running JS after
  // being disallowed (i.e. after the user closes the WebUI page) results in a
  // CHECK-fail. To avoid CHECK failing, the WeakPtrs are invalidated in
  // OnJavascriptDisallowed().
  base::WeakPtrFactory<CrostiniHandler> callback_weak_ptr_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CROSTINI_HANDLER_H_
