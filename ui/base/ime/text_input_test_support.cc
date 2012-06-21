// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/text_input_test_support.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#endif  // OS_CHROMEOS

namespace ui {

TextInputTestSupport::TextInputTestSupport() {
}

TextInputTestSupport::~TextInputTestSupport() {
}

void TextInputTestSupport::Initilaize() {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::InitializeWithStub();
#endif  // OS_CHROMEOS
}

void TextInputTestSupport::Shutdown() {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Shutdown();
#endif  // OS_CHROMEOS
}

}  // namespace ui
