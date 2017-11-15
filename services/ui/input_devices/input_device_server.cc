// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/input_devices/input_device_server.h"

#include <utility>
#include <vector>

#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"

#if defined(OS_CHROMEOS)
#include "services/ui/input_devices/touch_device_server.h"
#endif

namespace ui {

InputDeviceServer::InputDeviceServer() {
#if defined(OS_CHROMEOS)
  touch_device_server_ = std::make_unique<TouchDeviceServer>();
#endif
}

InputDeviceServer::~InputDeviceServer() {
  if (manager_ && ui::DeviceDataManager::HasInstance()) {
    manager_->RemoveObserver(this);
    manager_ = nullptr;
  }
}

void InputDeviceServer::RegisterAsObserver() {
  if (!manager_ && ui::DeviceDataManager::HasInstance()) {
    manager_ = ui::DeviceDataManager::GetInstance();
    manager_->AddObserver(this);
  }
}

bool InputDeviceServer::IsRegisteredAsObserver() const {
  return manager_ != nullptr;
}

void InputDeviceServer::AddInterface(
    service_manager::BinderRegistryWithArgs<
        const service_manager::BindSourceInfo&>* registry) {
  DCHECK(IsRegisteredAsObserver());
  registry->AddInterface<mojom::InputDeviceServer>(
      base::Bind(&InputDeviceServer::BindInputDeviceServerRequest,
                 base::Unretained(this)));
#if defined(OS_CHROMEOS)
  touch_device_server_->AddInterface(registry);
#endif
}

void InputDeviceServer::AddObserver(
    mojom::InputDeviceObserverMojoPtr observer) {
  // We only want to send this message once, so we need to check to make sure
  // device lists are actually complete before sending it to a new observer.
  if (manager_->AreDeviceListsComplete())
    SendDeviceListsComplete(observer.get());
  observers_.AddPtr(std::move(observer));
}

void InputDeviceServer::OnKeyboardDeviceConfigurationChanged() {
  if (!manager_->AreDeviceListsComplete())
    return;

  auto& devices = manager_->GetKeyboardDevices();
  observers_.ForAllPtrs([&devices](mojom::InputDeviceObserverMojo* observer) {
    observer->OnKeyboardDeviceConfigurationChanged(devices);
  });
}

void InputDeviceServer::OnTouchscreenDeviceConfigurationChanged() {
  CallOnTouchscreenDeviceConfigurationChanged();
}

void InputDeviceServer::OnMouseDeviceConfigurationChanged() {
  if (!manager_->AreDeviceListsComplete())
    return;

  auto& devices = manager_->GetMouseDevices();
  observers_.ForAllPtrs([&devices](mojom::InputDeviceObserverMojo* observer) {
    observer->OnMouseDeviceConfigurationChanged(devices);
  });
}

void InputDeviceServer::OnTouchpadDeviceConfigurationChanged() {
  if (!manager_->AreDeviceListsComplete())
    return;

  auto& devices = manager_->GetTouchpadDevices();
  observers_.ForAllPtrs([&devices](mojom::InputDeviceObserverMojo* observer) {
    observer->OnTouchpadDeviceConfigurationChanged(devices);
  });
}

void InputDeviceServer::OnDeviceListsComplete() {
  observers_.ForAllPtrs([this](mojom::InputDeviceObserverMojo* observer) {
    SendDeviceListsComplete(observer);
  });
}

void InputDeviceServer::OnStylusStateChanged(StylusState state) {
  observers_.ForAllPtrs([state](mojom::InputDeviceObserverMojo* observer) {
    observer->OnStylusStateChanged(state);
  });
}

void InputDeviceServer::OnTouchDeviceAssociationChanged() {
  CallOnTouchscreenDeviceConfigurationChanged();
}

void InputDeviceServer::SendDeviceListsComplete(
    mojom::InputDeviceObserverMojo* observer) {
  DCHECK(manager_->AreDeviceListsComplete());

  observer->OnDeviceListsComplete(
      manager_->GetKeyboardDevices(), manager_->GetTouchscreenDevices(),
      manager_->GetMouseDevices(), manager_->GetTouchpadDevices(),
      manager_->AreTouchscreenTargetDisplaysValid());
}

void InputDeviceServer::CallOnTouchscreenDeviceConfigurationChanged() {
  if (!manager_->AreDeviceListsComplete())
    return;

  auto& devices = manager_->GetTouchscreenDevices();
  const bool are_touchscreen_target_displays_valid =
      manager_->AreTouchscreenTargetDisplaysValid();
  observers_.ForAllPtrs([&devices, are_touchscreen_target_displays_valid](
                            mojom::InputDeviceObserverMojo* observer) {
    observer->OnTouchscreenDeviceConfigurationChanged(
        devices, are_touchscreen_target_displays_valid);
  });
}

void InputDeviceServer::BindInputDeviceServerRequest(
    mojom::InputDeviceServerRequest request,
    const service_manager::BindSourceInfo& source_info) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace ui
