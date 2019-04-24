// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/plugin_vm_service_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/dbus/plugin_vm_service/plugin_vm_service.pb.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

PluginVmServiceProvider::PluginVmServiceProvider() : weak_ptr_factory_(this) {}

PluginVmServiceProvider::~PluginVmServiceProvider() = default;

void PluginVmServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kPluginVmServiceInterface, kPluginVmServiceGetLicenseDataMethod,
      base::BindRepeating(&PluginVmServiceProvider::GetLicenseData,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PluginVmServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmServiceProvider::OnExported(const std::string& interface_name,
                                         const std::string& method_name,
                                         bool success) {
  if (!success)
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

void PluginVmServiceProvider::GetLicenseData(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  plugin_vm_service::GetLicenseDataResponse payload;
  payload.set_device_id(g_browser_process->platform_part()
                            ->browser_policy_connector_chromeos()
                            ->GetDirectoryApiID());
  payload.set_license_key(plugin_vm::GetPluginVmLicenseKey());
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(payload);
  response_sender.Run(std::move(response));
}

}  // namespace chromeos
