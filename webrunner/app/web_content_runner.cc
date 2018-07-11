// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/app/web_content_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <utility>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/logging.h"
#include "webrunner/app/component_controller_impl.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace webrunner {

WebContentRunner::WebContentRunner(chromium::web::ContextPtr context,
                                   base::OnceClosure on_all_closed)
    : context_(std::move(context)), on_all_closed_(std::move(on_all_closed)) {
  DCHECK(context_);
  DCHECK(on_all_closed_);
}

WebContentRunner::~WebContentRunner() = default;

void WebContentRunner::StartComponent(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    ::fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  DCHECK(on_all_closed_)
      << "StartComponent() called after Runner was deactivated.";

  auto controller = std::make_unique<ComponentControllerImpl>(
      this, std::move(package), std::move(startup_info),
      std::move(controller_request));
  controller->BindToRequest(std::move(package), std::move(startup_info),
                            std::move(controller_request));
  controllers_.insert(std::move(controller));
}

void WebContentRunner::DestroyComponent(ComponentControllerImpl* component) {
  DCHECK(on_all_closed_);
  controllers_.erase(controllers_.find(component));
  if (controllers_.empty()) {
    std::move(on_all_closed_).Run();
  }
}

}  // namespace webrunner
