// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/simple_browser/simple_browser_service.h"

#include "build/build_config.h"
#include "services/content/simple_browser/window.h"

#if defined(OS_LINUX)
#include "third_party/skia/include/ports/SkFontConfigInterface.h"  // nogncheck
#endif

#if defined(USE_AURA) && BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
#include "ui/views/mus/aura_init.h"  // nogncheck
#endif

namespace simple_browser {

SimpleBrowserService::SimpleBrowserService(
    service_manager::mojom::ServiceRequest request,
    UIInitializationMode ui_initialization_mode)
    : service_binding_(this, std::move(request)),
      ui_initialization_mode_(ui_initialization_mode) {}

SimpleBrowserService::~SimpleBrowserService() = default;

void SimpleBrowserService::OnStart() {
  if (ui_initialization_mode_ == UIInitializationMode::kInitializeUI) {
#if defined(OS_LINUX)
    font_loader_ =
        sk_make_sp<font_service::FontLoader>(service_binding_.GetConnector());
    SkFontConfigInterface::SetGlobal(font_loader_);
#endif

#if defined(USE_AURA) && BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
    views::AuraInit::InitParams params;
    params.connector = service_binding_.GetConnector();
    params.identity = service_binding_.identity();
    params.register_path_provider = false;
    aura_init_ = views::AuraInit::Create(params);
    CHECK(aura_init_);
#else
    NOTREACHED() << "Remote UI embedding not supported on this platform.";

#endif
  }

  window_ = std::make_unique<Window>(service_binding_.GetConnector());
}

}  // namespace simple_browser
