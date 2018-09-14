// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/webrunner_browser_main_parts.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "ui/aura/screen_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "webrunner/browser/context_impl.h"
#include "webrunner/browser/webrunner_browser_context.h"
#include "webrunner/browser/webrunner_screen.h"
#include "webrunner/service/common.h"

namespace webrunner {

WebRunnerBrowserMainParts::WebRunnerBrowserMainParts(
    zx::channel context_channel)
    : context_channel_(std::move(context_channel)) {}

WebRunnerBrowserMainParts::~WebRunnerBrowserMainParts() {
  display::Screen::SetScreenInstance(nullptr);
}

void WebRunnerBrowserMainParts::PreMainMessageLoopRun() {
  DCHECK(!screen_);

  auto platform_screen = ui::OzonePlatform::GetInstance()->CreateScreen();
  if (platform_screen) {
    screen_ = std::make_unique<aura::ScreenOzone>(std::move(platform_screen));
  } else {
    // Use dummy display::Screen for Ozone platforms that don't provide
    // PlatformScreen.
    screen_ = std::make_unique<WebRunnerScreen>();
  }

  display::Screen::SetScreenInstance(screen_.get());

  DCHECK(!browser_context_);
  browser_context_ = std::make_unique<WebRunnerBrowserContext>();

  context_service_ = std::make_unique<ContextImpl>(browser_context_.get());

  context_binding_ = std::make_unique<fidl::Binding<chromium::web::Context>>(
      context_service_.get(), fidl::InterfaceRequest<chromium::web::Context>(
                                  std::move(context_channel_)));

  // Quit the browser main loop when the Context connection is dropped.
  context_binding_->set_error_handler([this]() {
    DLOG(WARNING) << "Client connection to Context service dropped.";
    context_service_.reset();
    std::move(quit_closure_).Run();
  });
}

void WebRunnerBrowserMainParts::PreDefaultMainMessageLoopRun(
    base::OnceClosure quit_closure) {
  quit_closure_ = std::move(quit_closure);
}

void WebRunnerBrowserMainParts::PostMainMessageLoopRun() {
  // The service and its binding should have already been released by the error
  // handler.
  DCHECK(!context_service_);
  DCHECK(!context_binding_->is_bound());

  // These resources must be freed while a MessageLoop is still available, so
  // that they may post cleanup tasks during teardown.
  // NOTE: Please destroy objects in the reverse order of their creation.
  browser_context_.reset();
  screen_.reset();
}

}  // namespace webrunner
