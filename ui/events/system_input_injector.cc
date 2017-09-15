// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/system_input_injector.h"

#include "base/memory/ptr_util.h"

namespace ui {

namespace {
SystemInputInjectorFactory* override_factory_ = nullptr;
SystemInputInjectorFactory* native_factory_ = nullptr;
}  // namespace

void SetOverrideInputInjectorFactory(SystemInputInjectorFactory* factory) {
  DCHECK(!factory || !override_factory_);
  override_factory_ = factory;
}

void SetNativeInputInjectorFactory(SystemInputInjectorFactory* factory) {
  DCHECK(!factory || !native_factory_);
  native_factory_ = factory;
}

SystemInputInjectorFactory* GetSystemInputInjectorFactory() {
  if (override_factory_)
    return override_factory_;
  if (native_factory_)
    return native_factory_;
  return nullptr;
}

}  // namespace ui
