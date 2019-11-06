// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/ui_module_factory.h"

#include <dlfcn.h>

#include <utility>

#include "base/android/bundle_utils.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/audio_delegate.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/ui_interface.h"

namespace vr {

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
  DCHECK(ui_library_handle_ == nullptr);
  ui_library_handle_ =
      base::android::BundleUtils::DlOpenModuleLibraryPartition("vr");
  DCHECK(ui_library_handle_ != nullptr)
      << "Could not open VR UI library:" << dlerror();

  CreateUiFunction* create_ui = reinterpret_cast<CreateUiFunction*>(
      dlsym(ui_library_handle_, "CreateUi"));
  DCHECK(create_ui != nullptr);

  std::unique_ptr<UiInterface> ui = base::WrapUnique(
      create_ui(browser, content_input_forwarder, std::move(keyboard_delegate),
                std::move(text_input_delegate), std::move(audio_delegate),
                ui_initial_state));
  DCHECK(ui != nullptr);

  return ui;
}

std::unique_ptr<UiFactory> CreateUiFactory() {
  return std::make_unique<UiModuleFactory>();
}

}  // namespace vr
