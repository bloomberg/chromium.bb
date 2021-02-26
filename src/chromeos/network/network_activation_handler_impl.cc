// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_activation_handler_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

const char kErrorShillError[] = "shill-error";

}  // namespace

NetworkActivationHandlerImpl::NetworkActivationHandlerImpl() = default;

NetworkActivationHandlerImpl::~NetworkActivationHandlerImpl() = default;

void NetworkActivationHandlerImpl::CompleteActivation(
    const std::string& service_path,
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "CompleteActivation: " << NetworkPathId(service_path);
  ShillServiceClient::Get()->CompleteCellularActivation(
      dbus::ObjectPath(service_path),
      base::BindOnce(&NetworkActivationHandlerImpl::HandleShillSuccess,
                     AsWeakPtr(), std::move(success_callback)),
      base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                     kErrorShillError, service_path,
                     std::move(error_callback)));
}

void NetworkActivationHandlerImpl::HandleShillSuccess(
    base::OnceClosure success_callback) {
  if (!success_callback.is_null())
    std::move(success_callback).Run();
}

}  // namespace chromeos
