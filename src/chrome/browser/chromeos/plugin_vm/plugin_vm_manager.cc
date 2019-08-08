// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"

#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/crostini/crostini_share_path.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/vm_plugin_dispatcher_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace plugin_vm {

namespace {

class PluginVmManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PluginVmManager* GetForProfile(Profile* profile) {
    return static_cast<PluginVmManager*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static PluginVmManagerFactory* GetInstance() {
    static base::NoDestructor<PluginVmManagerFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<PluginVmManagerFactory>;

  PluginVmManagerFactory()
      : BrowserContextKeyedServiceFactory(
            "PluginVmManager",
            BrowserContextDependencyManager::GetInstance()) {}

  ~PluginVmManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new PluginVmManager(profile);
  }
};

}  // namespace

PluginVmManager* PluginVmManager::GetForProfile(Profile* profile) {
  return PluginVmManagerFactory::GetForProfile(profile);
}

PluginVmManager::PluginVmManager(Profile* profile)
    : profile_(profile),
      owner_id_(chromeos::ProfileHelper::GetUserIdHashFromProfile(profile)),
      weak_ptr_factory_(this) {}

PluginVmManager::~PluginVmManager() {}

void PluginVmManager::LaunchPluginVm() {
  if (!IsPluginVmAllowedForProfile(profile_)) {
    LOG(ERROR) << "Attempted to launch PluginVm when it is not allowed";
    RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult::kError);
    return;
  }

  // Launching Plugin Vm goes through the following steps:
  // 1) Start the Plugin Vm Dispatcher
  // 2) Call ListVms to get the state of the VM
  // 3) Start the VM if necessary
  // 4) Show the UI.
  // 5) Call Concierge::GetVmInfo to get seneschal server handle.
  //    This happens in parallel with step 4.
  // 6) Ensure default shared path exists.
  // 7) Share paths with PluginVm
  chromeos::DBusThreadManager::Get()
      ->GetDebugDaemonClient()
      ->StartPluginVmDispatcher(
          base::BindOnce(&PluginVmManager::OnStartPluginVmDispatcher,
                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManager::StopPluginVm() {
  vm_tools::plugin_dispatcher::StopVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->StopVm(
      std::move(request), base::DoNothing());

  // Reset seneschal handle to indicate that it is no longer valid.
  seneschal_server_handle_ = 0;
}

void PluginVmManager::OnStartPluginVmDispatcher(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to start Plugin Vm Dispatcher.";
    RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult::kError);
    return;
  }

  vm_tools::plugin_dispatcher::ListVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->ListVms(
      std::move(request), base::BindOnce(&PluginVmManager::OnListVms,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManager::OnListVms(
    base::Optional<vm_tools::plugin_dispatcher::ListVmResponse> reply) {
  if (!reply.has_value() || reply->error()) {
    LOG(ERROR) << "Failed to list VMs.";
    RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult::kError);
    return;
  }
  if (reply->vm_info_size() != 1) {
    LOG(ERROR) << "Default VM is missing.";
    RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult::kError);
    return;
  }

  auto vm_state = reply->vm_info(0).state();
  switch (vm_state) {
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_STARTING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING:
      ShowVm();
      break;
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_PAUSED:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED: {
      vm_tools::plugin_dispatcher::StartVmRequest request;
      request.set_owner_id(owner_id_);
      request.set_vm_name_uuid(kPluginVmName);

      chromeos::DBusThreadManager::Get()
          ->GetVmPluginDispatcherClient()
          ->StartVm(std::move(request),
                    base::BindOnce(&PluginVmManager::OnStartVm,
                                   weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    default:
      // TODO(timloh): We need some way to handle states like stopping, perhaps
      // we should retry after a few seconds?
      LOG(ERROR) << "Didn't start VM as it is in state " << vm_state;
      RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult::kError);
      break;
  }
}

void PluginVmManager::OnStartVm(
    base::Optional<vm_tools::plugin_dispatcher::StartVmResponse> reply) {
  if (!reply.has_value() || reply->error()) {
    LOG(ERROR) << "Failed to start VM.";
    RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult::kError);
    return;
  }

  ShowVm();
}

void PluginVmManager::ShowVm() {
  vm_tools::plugin_dispatcher::ShowVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmName);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->ShowVm(
      std::move(request), base::BindOnce(&PluginVmManager::OnShowVm,
                                         weak_ptr_factory_.GetWeakPtr()));

  vm_tools::concierge::GetVmInfoRequest concierge_request;
  concierge_request.set_owner_id(owner_id_);
  concierge_request.set_name(kPluginVmName);

  chromeos::DBusThreadManager::Get()->GetConciergeClient()->GetVmInfo(
      std::move(concierge_request),
      base::BindOnce(&PluginVmManager::OnGetVmInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManager::OnShowVm(
    base::Optional<vm_tools::plugin_dispatcher::ShowVmResponse> reply) {
  if (!reply.has_value() || reply->error()) {
    LOG(ERROR) << "Failed to show VM.";
    RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult::kError);
    return;
  }

  VLOG(1) << "ShowVm completed successfully.";
  RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult::kSuccess);
}

void PluginVmManager::OnGetVmInfo(
    base::Optional<vm_tools::concierge::GetVmInfoResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to get concierge VM info.";
    return;
  }
  if (!reply->success()) {
    LOG(ERROR) << "VM not started, cannot share paths";
    return;
  }
  seneschal_server_handle_ = reply->vm_info().seneschal_server_handle();

  // Create and share default folder, and other persisted shares.
  EnsureDefaultSharedDirExists(
      profile_, base::BindOnce(&PluginVmManager::OnDefaultSharedDirExists,
                               weak_ptr_factory_.GetWeakPtr()));
  crostini::CrostiniSharePath::GetForProfile(profile_)->SharePersistedPaths(
      kPluginVmName, base::DoNothing());
}

void PluginVmManager::OnDefaultSharedDirExists(const base::FilePath& dir,
                                               bool exists) {
  if (exists) {
    crostini::CrostiniSharePath::GetForProfile(profile_)->SharePath(
        kPluginVmName, dir, false,
        base::BindOnce([](const base::FilePath& dir, bool success,
                          std::string failure_reason) {
          if (!success) {
            LOG(ERROR) << "Error sharing PluginVm default dir " << dir.value()
                       << ": " << failure_reason;
          }
        }));
  }
}
}  // namespace plugin_vm
