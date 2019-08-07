// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/ui_module_dummy_factory.h"

#include <dlfcn.h>

#include <utility>

#include "base/android/bundle_utils.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/audio_delegate.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_interface.h"

namespace vr {

namespace {

typedef int CreateUiDummyFunction();

void HandleError(const std::string& error_message) {
  DEBUG_ALIAS_FOR_CSTR(minidump, error_message.c_str(), 1024);
  base::debug::DumpWithoutCrashing();
  // Crash if we are in a debug build to catch problems early.
  DLOG(FATAL) << error_message;
}

}  // namespace

UiModuleFactory::~UiModuleFactory() {
  if (ui_library_handle_ != nullptr) {
    dlclose(ui_library_handle_);
  }
}

std::unique_ptr<UiInterface> UiModuleFactory::Create(
    UiBrowserInterface* browser,
    PlatformInputHandler* content_input_forwarder,
    std::unique_ptr<KeyboardDelegate> keyboard_delegate,
    std::unique_ptr<TextInputDelegate> text_input_delegate,
    std::unique_ptr<AudioDelegate> audio_delegate,
    const UiInitialState& ui_initial_state) {
  auto ui = std::make_unique<Ui>(browser, content_input_forwarder,
                                 std::move(keyboard_delegate),
                                 std::move(text_input_delegate),
                                 std::move(audio_delegate), ui_initial_state);
  if (ui_library_handle_ == nullptr) {
    ui_library_handle_ =
        base::android::BundleUtils::DlOpenModuleLibrary("vr_ui_dummy_lib");
  }
  if (ui_library_handle_ == nullptr) {
    HandleError(std::string("Could not open VR UI library: ") + dlerror());
    return ui;
  }

  CreateUiDummyFunction* create_ui = reinterpret_cast<CreateUiDummyFunction*>(
      dlsym(ui_library_handle_, "CreateUi"));
  if (create_ui == nullptr) {
    HandleError(std::string("Could not find UI creation function: ") +
                dlerror());
    return ui;
  }

  int result = create_ui();
  if (result != 42) {
    HandleError("Did not get correct result");
    return ui;
  }

  return ui;
}

std::unique_ptr<UiFactory> CreateUiFactory() {
  return std::make_unique<UiModuleFactory>();
}

}  // namespace vr
