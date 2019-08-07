// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"

#include <fuchsia/ui/policy/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/service_directory_client.h"

namespace ui {
namespace fuchsia {

void InitializeViewTokenAndPresentView(
    ui::PlatformWindowInitProperties* window_properties_out) {
  DCHECK(window_properties_out);

  // Generate ViewToken and ViewHolderToken for the new view.
  ::fuchsia::ui::views::ViewHolderToken view_holder_token;
  std::tie(window_properties_out->view_token, view_holder_token) =
      scenic::NewViewTokenPair();

  // Request Presenter to show the view full-screen.
  auto presenter = base::fuchsia::ServiceDirectoryClient::ForCurrentProcess()
                       ->ConnectToService<::fuchsia::ui::policy::Presenter>();

  presenter->PresentView(std::move(view_holder_token), nullptr);
}

}  // namespace fuchsia
}  // namespace ui
