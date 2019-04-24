// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/vr_module_delegate.h"

namespace device {

namespace {
// Storing the global factory in a raw pointer - as opposed to e.g. an
// std::unique_ptr - to avoid adding a static initializer.
VrModuleDelegateFactory* g_vr_module_delegate_factory = nullptr;
}  // namespace

// static
VrModuleDelegateFactory* VrModuleDelegateFactory::Get() {
  return g_vr_module_delegate_factory;
}

// static
void VrModuleDelegateFactory::Set(
    std::unique_ptr<VrModuleDelegateFactory> factory) {
  if (g_vr_module_delegate_factory) {
    delete g_vr_module_delegate_factory;
  }
  g_vr_module_delegate_factory = factory.release();
}

}  // namespace device
