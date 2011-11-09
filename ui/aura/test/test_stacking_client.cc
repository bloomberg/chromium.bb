// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_stacking_client.h"

#include "ui/aura/desktop.h"

namespace aura {
namespace test {

TestStackingClient::TestStackingClient()
    : default_container_(new ToplevelWindowContainer) {
  Desktop::GetInstance()->SetStackingClient(this);
  default_container_->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
  default_container_->SetBounds(
      gfx::Rect(gfx::Point(), Desktop::GetInstance()->GetHostSize()));
  Desktop::GetInstance()->AddChild(default_container_.get());
  default_container_->Show();
}

TestStackingClient::~TestStackingClient() {
}

void TestStackingClient::AddChildToDefaultParent(Window* window) {
  default_container_->AddChild(window);
}

Window* TestStackingClient::GetTopmostWindowToActivate(Window* ignore) const {
  return default_container_->GetTopmostWindowToActivate(ignore);
}

}  // namespace test
}  // namespace aura
