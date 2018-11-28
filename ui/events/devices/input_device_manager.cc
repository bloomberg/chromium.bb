// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device_manager.h"

namespace ui {
namespace {

InputDeviceManager* g_instance = nullptr;

}  // namespace

// static
InputDeviceManager* InputDeviceManager::GetInstance() {
  DCHECK(g_instance) << "InputDeviceManager::SetInstance must be called before "
                        "getting the instance of InputDeviceManager.";
  return g_instance;
}

// static
bool InputDeviceManager::HasInstance() {
  return g_instance;
}

// static
void InputDeviceManager::SetInstance(InputDeviceManager* instance) {
  DCHECK(!g_instance);
  g_instance = instance;
}

// static
void InputDeviceManager::ClearInstance() {
  g_instance = nullptr;
}

}  // namespace ui
