// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chromeos/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher.pb.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace plugin_vm {

// The PluginVmManager is responsible for connecting to the D-Bus services to
// manage the Plugin Vm.
class PluginVmManager : public KeyedService {
 public:
  static PluginVmManager* GetForProfile(Profile* profile);

  explicit PluginVmManager(Profile* profile);
  ~PluginVmManager() override;

  void LaunchPluginVm();
  void StopPluginVm();

  // Seneschal server handle to use for path sharing.
  uint64_t seneschal_server_handle() { return seneschal_server_handle_; }

 private:
  // The flow to launch a Plugin Vm. We'll probably want to add additional
  // abstraction around starting the services in the future but this is
  // sufficient for now.
  void OnStartPluginVmDispatcher(bool success);
  void OnListVms(
      base::Optional<vm_tools::plugin_dispatcher::ListVmResponse> reply);
  void OnStartVm(
      base::Optional<vm_tools::plugin_dispatcher::StartVmResponse> reply);
  void ShowVm();
  void OnShowVm(
      base::Optional<vm_tools::plugin_dispatcher::ShowVmResponse> reply);
  void OnGetVmInfo(
      base::Optional<vm_tools::concierge::GetVmInfoResponse> reply);
  void OnDefaultSharedDirExists(const base::FilePath& dir, bool exists);

  Profile* profile_;
  std::string owner_id_;
  uint64_t seneschal_server_handle_ = 0;

  base::WeakPtrFactory<PluginVmManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginVmManager);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_MANAGER_H_
