// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/simple_browser/simple_browser_service.h"

#include "build/build_config.h"
#include "services/content/simple_browser/window.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "ui/views/mus/aura_init.h"

#if defined(OS_LINUX)
#include "third_party/skia/include/ports/SkFontConfigInterface.h"  // nogncheck
#endif

namespace simple_browser {

SimpleBrowserService::SimpleBrowserService(
    UIInitializationMode ui_initialization_mode)
    : ui_initialization_mode_(ui_initialization_mode) {}

SimpleBrowserService::~SimpleBrowserService() = default;

void SimpleBrowserService::OnStart() {
  if (ui_initialization_mode_ == UIInitializationMode::kInitializeUI) {
#if defined(OS_LINUX)
    font_loader_ = sk_make_sp<font_service::FontLoader>(context()->connector());
    SkFontConfigInterface::SetGlobal(font_loader_);
#endif

    views::AuraInit::InitParams params;
    params.connector = context()->connector();
    params.identity = context()->identity();
    params.register_path_provider = false;
    aura_init_ = views::AuraInit::Create(params);
    CHECK(aura_init_);
  }

  window_ = std::make_unique<Window>(context()->connector());
}

}  // namespace simple_browser
