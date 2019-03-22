// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"

#include <fuchsia/ui/policy/cpp/fidl.h>
#include <lib/zx/eventpair.h>

#include "base/fuchsia/component_context.h"
#include "base/fuchsia/fuchsia_logging.h"

namespace ui {
namespace fuchsia {

void InitializeViewTokenAndPresentView(
    ui::PlatformWindowInitProperties* window_properties_out) {
  DCHECK(window_properties_out);

  // Generate and set the view tokens for the |window_properties_out| and the
  // Presenter API.
  zx::eventpair view_holder_token;
  zx_status_t status = zx::eventpair::create(
      /* options = */ 0, &window_properties_out->view_token,
      &view_holder_token);
  ZX_CHECK(status == ZX_OK, status) << "zx_eventpair_create";

  // Request Presenter to show the view full-screen.
  auto presenter = base::fuchsia::ComponentContext::GetDefault()
                       ->ConnectToService<::fuchsia::ui::policy::Presenter>();

  presenter->Present2(std::move(view_holder_token), nullptr);
}

}  // namespace fuchsia
}  // namespace ui
