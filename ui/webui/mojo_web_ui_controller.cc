// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/mojo_web_ui_controller.h"

#include "content/public/common/bindings_policy.h"

namespace ui {

MojoWebUIControllerBase::MojoWebUIControllerBase(content::WebUI* contents)
    : content::WebUIController(contents) {}

MojoWebUIControllerBase::~MojoWebUIControllerBase() = default;

void MojoWebUIControllerBase::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  render_frame_host->AllowBindings(content::BINDINGS_POLICY_WEB_UI);
}

}  // namespace ui
