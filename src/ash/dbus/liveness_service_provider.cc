// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/liveness_service_provider.h"

#include "base/bind.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

LivenessServiceProvider::LivenessServiceProvider() : weak_ptr_factory_(this) {}

LivenessServiceProvider::~LivenessServiceProvider() = default;

void LivenessServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kLivenessServiceInterface,
      chromeos::kLivenessServiceCheckLivenessMethod,
      base::Bind(&LivenessServiceProvider::CheckLiveness,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&LivenessServiceProvider::OnExported,
                 weak_ptr_factory_.GetWeakPtr()));
}

void LivenessServiceProvider::OnExported(const std::string& interface_name,
                                         const std::string& method_name,
                                         bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void LivenessServiceProvider::CheckLiveness(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

}  // namespace ash
