// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_initializer.h"

#include "ui/base/ime/input_method_factory.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/ime/ibus_daemon_controller.h"
#elif defined(OS_WIN)
#include "base/win/metro.h"
#include "ui/base/ime/win/tsf_bridge.h"
#endif

#if defined(OS_CHROMEOS)
namespace {
bool dbus_thread_manager_was_initialized = false;
}
#endif  // OS_CHROMEOS

namespace ui {

void InitializeInputMethod() {
#if defined(OS_WIN)
  if (base::win::IsTSFAwareRequired())
    ui::TSFBridge::Initialize();
#endif
}

void ShutdownInputMethod() {
#if defined(OS_WIN)
  ui::internal::DestroySharedInputMethod();
  if (base::win::IsTSFAwareRequired())
    ui::TSFBridge::Shutdown();
#endif
}

void InitializeInputMethodForTesting() {
#if defined(OS_CHROMEOS)
  if (!chromeos::DBusThreadManager::IsInitialized()) {
    chromeos::DBusThreadManager::InitializeWithStub();
    dbus_thread_manager_was_initialized = true;
  }
  if (!chromeos::IBusDaemonController::GetInstance()) {
    // Passing NULL is okay because IBusDaemonController will be initialized
    // with stub implementation on non-ChromeOS device.
    DCHECK(!base::chromeos::IsRunningOnChromeOS());
    chromeos::IBusDaemonController::Initialize(NULL, NULL);
  }
#elif defined(OS_WIN)
  if (base::win::IsTSFAwareRequired()) {
    // Make sure COM is initialized because TSF depends on COM.
    CoInitialize(NULL);
    ui::TSFBridge::Initialize();
  }
#endif
}

void ShutdownInputMethodForTesting() {
#if defined(OS_CHROMEOS)
  if (dbus_thread_manager_was_initialized) {
    chromeos::DBusThreadManager::Shutdown();
    dbus_thread_manager_was_initialized = false;
  }
  if (chromeos::IBusDaemonController::GetInstance())
    chromeos::IBusDaemonController::Shutdown();
#elif defined(OS_WIN)
  ui::internal::DestroySharedInputMethod();
  if (base::win::IsTSFAwareRequired()) {
    ui::TSFBridge::Shutdown();
    CoUninitialize();
  }
#endif
}

}  // namespace ui
