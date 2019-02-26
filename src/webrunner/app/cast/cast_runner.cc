// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/app/cast/cast_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <utility>

#include "base/logging.h"
#include "url/gurl.h"
#include "webrunner/app/common/web_component.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace castrunner {

CastRunner::CastRunner(base::fuchsia::ServiceDirectory* service_directory,
                       chromium::web::ContextPtr context,
                       base::OnceClosure on_idle_closure)
    : webrunner::WebContentRunner(service_directory,
                                  std::move(context),
                                  std::move(on_idle_closure)) {}

CastRunner::~CastRunner() = default;

void CastRunner::StartComponent(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  // TODO(https://crbug.com/893229): Define a CastAppConfig FIDL and dummy
  // service implementation to hold this logic.
  constexpr char kCastPresentationUrlScheme[] = "cast";
  GURL cast_url(*package.resolved_url);
  if (!cast_url.is_valid() || !cast_url.SchemeIs(kCastPresentationUrlScheme) ||
      cast_url.GetContent().empty()) {
    LOG(ERROR) << "Rejected invalid URL: " << cast_url;
    return;
  }

  constexpr char kTestCastAppId[] = "00000000";
  const std::string cast_app_id(cast_url.GetContent());
  if (cast_app_id != kTestCastAppId) {
    LOG(ERROR) << "Unknown Cast app Id: " << cast_app_id;
    return;
  }

  GURL cast_app_url("https://www.google.com");
  RegisterComponent(webrunner::WebComponent::ForUrlRequest(
      this, std::move(cast_app_url), std::move(startup_info),
      std::move(controller_request)));
}

}  // namespace castrunner
