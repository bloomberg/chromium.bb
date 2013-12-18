// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/ime/input_method_context_factory_ozone.h"
#include "ui/ozone/ime/fake_input_method_context_ozone.h"

namespace ui {

InputMethodContextFactoryOzone::InputMethodContextFactoryOzone() {
}

InputMethodContextFactoryOzone::~InputMethodContextFactoryOzone() {
}

scoped_ptr<LinuxInputMethodContext>
InputMethodContextFactoryOzone::CreateInputMethodContext(
      LinuxInputMethodContextDelegate* /*delegate*/) const {
  return scoped_ptr<LinuxInputMethodContext>(new FakeInputMethodContextOzone());
}

}  // namespace ui
