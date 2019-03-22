// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CONTENT_SIMPLE_BROWSER_SIMPLE_BROWSER_SERVICE_H_
#define SERVICES_CONTENT_SIMPLE_BROWSER_SIMPLE_BROWSER_SERVICE_H_

#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "services/content/public/cpp/buildflags.h"
#include "services/service_manager/public/cpp/service.h"

#if defined(OS_LINUX)
#include "components/services/font/public/cpp/font_loader.h"  // nogncheck
#endif

namespace views {
class AuraInit;
}

namespace simple_browser {

class Window;

class COMPONENT_EXPORT(SIMPLE_BROWSER) SimpleBrowserService
    : public service_manager::Service {
 public:
  // Determines how a SimpleBrowserService instance is initialized.
  enum class UIInitializationMode {
    // The service is being run in an isolated process which has not yet
    // initialized a UI framework.
    kInitializeUI,

    // The service is being run in a process which has already initialized a
    // UI framework. No need to do that.
    kUseEnvironmentUI,
  };

  explicit SimpleBrowserService(UIInitializationMode mode);
  ~SimpleBrowserService() override;

 private:
  // service_manager::Service:
  void OnStart() override;

#if defined(OS_LINUX)
  sk_sp<font_service::FontLoader> font_loader_;
#endif

  const UIInitializationMode ui_initialization_mode_;

#if defined(USE_AURA) && BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
  std::unique_ptr<views::AuraInit> aura_init_;
#endif

  std::unique_ptr<Window> window_;

  DISALLOW_COPY_AND_ASSIGN(SimpleBrowserService);
};

}  // namespace simple_browser

#endif  // SERVICES_CONTENT_SIMPLE_BROWSER_SIMPLE_BROWSER_SERVICE_H_
