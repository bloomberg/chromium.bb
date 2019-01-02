// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/audio_delegate.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/ui.h"

#if defined(FEATURE_MODULES)
#include <dlfcn.h>
#endif

namespace vr {

UiFactory::~UiFactory() {
#if defined(FEATURE_MODULES)
  if (ui_library_handle_ != nullptr) {
    dlclose(ui_library_handle_);
  }
#endif  // defined(FEATURE_MODULES)
}

std::unique_ptr<UiInterface> UiFactory::Create(
    UiBrowserInterface* browser,
    PlatformInputHandler* content_input_forwarder,
    std::unique_ptr<KeyboardDelegate> keyboard_delegate,
    std::unique_ptr<TextInputDelegate> text_input_delegate,
    std::unique_ptr<AudioDelegate> audio_delegate,
    const UiInitialState& ui_initial_state) {
#if defined(FEATURE_MODULES)
  DCHECK(ui_library_handle_ == nullptr);

  // TODO(http://crbug.com/874584): Rather than attempting to open two different
  // libraries, either package the library under a unified name, or run code
  // conditionally based on whether we're monochrome or not.
  ui_library_handle_ = dlopen("libvr_ui.so", RTLD_LOCAL | RTLD_NOW);
  if (!ui_library_handle_) {
    ui_library_handle_ =
        dlopen("libvr_ui_monochrome.so", RTLD_LOCAL | RTLD_NOW);
  }
  CHECK(ui_library_handle_ != nullptr)
      << "Could not open VR UI library:" << dlerror();

  CreateUiFunction* create_ui = reinterpret_cast<CreateUiFunction*>(
      dlsym(ui_library_handle_, "CreateUi"));
  CHECK(create_ui != nullptr) << "Could not obtain UI creation method";

  return base::WrapUnique(
      create_ui(browser, content_input_forwarder, std::move(keyboard_delegate),
                std::move(text_input_delegate), std::move(audio_delegate),
                ui_initial_state));
#else
  return std::make_unique<Ui>(browser, content_input_forwarder,
                              std::move(keyboard_delegate),
                              std::move(text_input_delegate),
                              std::move(audio_delegate), ui_initial_state);
#endif  // defined(FEATURE_MODULES)
}

}  // namespace vr
