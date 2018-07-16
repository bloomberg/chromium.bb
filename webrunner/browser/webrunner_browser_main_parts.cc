// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/webrunner_browser_main_parts.h"

#include "webrunner/browser/webrunner_browser_context.h"
#include "webrunner/browser/webrunner_screen.h"
#include "webrunner/service/common.h"
#include "webrunner/service/context_impl.h"

namespace webrunner {

WebRunnerBrowserMainParts::WebRunnerBrowserMainParts() = default;
WebRunnerBrowserMainParts::~WebRunnerBrowserMainParts() = default;

void WebRunnerBrowserMainParts::PreMainMessageLoopRun() {
  DCHECK(!screen_);
  screen_ = std::make_unique<WebRunnerScreen>();
  display::Screen::SetScreenInstance(screen_.get());

  DCHECK(!browser_context_);
  browser_context_ = std::make_unique<WebRunnerBrowserContext>();

  // Get the Context InterfaceRequest which was passed to the process by
  // the ContextProvider.
  zx::channel context_handle{zx_take_startup_handle(kContextRequestHandleId)};
  CHECK(context_handle) << "Could not find the Context request startup handle.";
  fidl::InterfaceRequest<chromium::web::Context> context_request(
      std::move(context_handle));

  context_impl_ = std::make_unique<ContextImpl>();
  context_binding_ = std::make_unique<fidl::Binding<chromium::web::Context>>(
      context_impl_.get(), std::move(context_request));

  // Quit the context process as soon as context is disconnected.
  context_binding_->set_error_handler(
      [this]() { std::move(quit_closure_).Run(); });
}

void WebRunnerBrowserMainParts::PreDefaultMainMessageLoopRun(
    base::OnceClosure quit_closure) {
  quit_closure_ = std::move(quit_closure);
}

}  // namespace webrunner
