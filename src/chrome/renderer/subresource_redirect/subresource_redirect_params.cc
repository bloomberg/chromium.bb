// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"

#include "base/command_line.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_switches.h"

namespace subresource_redirect {

bool ShouldForceEnableSubresourceRedirect() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      subresource_redirect::kEnableSubresourceRedirect);
}

}  // namespace subresource_redirect