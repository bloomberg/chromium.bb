// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_MOJO_CONNECTION_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_MOJO_CONNECTION_SERVICE_PROVIDER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

// This class processes bootstrap mojo connection requests for other processes.
// Current it supports IIO Service and Sensor Clients, such as
// iioservice_simpleclient and powerd.
//
// The following methods are exported:
//
// Interface:
// org.chromium.MojoConnectionService
//    (mojo_connection_service::kMojoConnectionServiceInterfaceName)
// Methods: BootstrapMojoConnectionForIioService
//     (mojo_connection_service::kBootstrapMojoConnectionForIioServiceMethod),
//          BootstrapMojoConnectionForSensorClients
//     (mojo_connection_service::kBootstrapMojoConnectionForSensorClientsMethod)
// Parameters: none
//
//   Returns an endpoint of a mojo pipe via an asynchronous response, containing
//   one value:
//
//   base::ScopedFD:file_handle - an endpoint of a mojo pipe, used to accept an
//                                incoming invitation.
//
class MojoConnectionServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  MojoConnectionServiceProvider();
  MojoConnectionServiceProvider(const MojoConnectionServiceProvider&) = delete;
  MojoConnectionServiceProvider& operator=(
      const MojoConnectionServiceProvider&) = delete;
  ~MojoConnectionServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called when ResolveProxy() is exported as a D-Bus method.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  void BootstrapMojoConnectionForIioService(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  void BootstrapMojoConnectionForSensorClients(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  void BootstrapMojoConnectionForRollbackNetworkConfig(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  void SendInvitation(mojo::PlatformChannel* platform_channel,
                      mojo::ScopedMessagePipeHandle* pipe);

  void SendResponse(mojo::PlatformChannel platform_channel,
                    dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  scoped_refptr<dbus::ExportedObject> exported_object_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MojoConnectionServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
using ::ash::MojoConnectionServiceProvider;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_DBUS_MOJO_CONNECTION_SERVICE_PROVIDER_H_
