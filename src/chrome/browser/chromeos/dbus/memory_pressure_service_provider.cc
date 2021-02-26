// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/memory_pressure_service_provider.h"

#include "base/bind.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

MemoryPressureServiceProvider::MemoryPressureServiceProvider() = default;

MemoryPressureServiceProvider::~MemoryPressureServiceProvider() {
  if (exported_object_) {
    chromeos::memory::pressure::PressureChecker::GetInstance()->RemoveObserver(
        this);
  }
}

void MemoryPressureServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  DCHECK(!exported_object_);
  exported_object_ = exported_object;
  exported_object->ExportMethod(
      memory_pressure::kMemoryPressureInterface,
      memory_pressure::kGetAvailableMemoryKBMethod,
      base::BindRepeating(&MemoryPressureServiceProvider::GetAvailableMemoryKB,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&MemoryPressureServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      memory_pressure::kMemoryPressureInterface,
      memory_pressure::kGetMemoryMarginsKBMethod,
      base::BindRepeating(&MemoryPressureServiceProvider::GetMemoryMarginsKB,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&MemoryPressureServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  chromeos::memory::pressure::PressureChecker::GetInstance()->AddObserver(this);
}

void MemoryPressureServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

void MemoryPressureServiceProvider::GetAvailableMemoryKB(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendUint64(chromeos::memory::pressure::GetAvailableMemoryKB());
  std::move(response_sender).Run(std::move(response));
}

void MemoryPressureServiceProvider::GetMemoryMarginsKB(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  std::pair<uint64_t, uint64_t> margins =
      chromeos::memory::pressure::GetMemoryMarginsKB();
  writer.AppendUint64(margins.first);
  writer.AppendUint64(margins.second);
  std::move(response_sender).Run(std::move(response));
}

void MemoryPressureServiceProvider::OnCriticalPressure() {
  DCHECK(exported_object_);

  dbus::Signal signal(memory_pressure::kMemoryPressureInterface,
                      memory_pressure::kCriticalMemoryPressure);
  exported_object_->SendSignal(&signal);
}

void MemoryPressureServiceProvider::OnModeratePressure() {
  DCHECK(exported_object_);

  dbus::Signal signal(memory_pressure::kMemoryPressureInterface,
                      memory_pressure::kModerateMemoryPressure);
  exported_object_->SendSignal(&signal);
}

}  // namespace chromeos
