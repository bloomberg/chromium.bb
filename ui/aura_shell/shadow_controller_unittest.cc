// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shadow_controller.h"

#include <algorithm>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/shadow_types.h"
#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/shadow.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/test/aura_shell_test_base.h"
#include "ui/gfx/compositor/layer.h"

namespace aura_shell {
namespace test {

typedef aura_shell::test::AuraShellTestBase ShadowControllerTest;

// Tests that various methods in Window update the Shadow object as expected.
TEST_F(ShadowControllerTest, Shadow) {
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(aura::WINDOW_TYPE_NORMAL);
  window->SetIntProperty(aura::kShadowTypeKey, aura::SHADOW_TYPE_RECTANGULAR);
  window->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window->SetParent(NULL);

  // We should create the shadow before the window is visible (the shadow's
  // layer won't get drawn yet since it's a child of the window's layer).
  internal::ShadowController::TestApi api(
      aura_shell::Shell::GetInstance()->shadow_controller());
  const internal::Shadow* shadow = api.GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);
  EXPECT_TRUE(shadow->layer()->visible());

  // The shadow should remain visible after window visibility changes.
  window->Show();
  EXPECT_TRUE(shadow->layer()->visible());
  window->Hide();
  EXPECT_TRUE(shadow->layer()->visible());

  // If the shadow is disabled, it should be hidden.
  window->SetIntProperty(aura::kShadowTypeKey, aura::SHADOW_TYPE_NONE);
  window->Show();
  EXPECT_FALSE(shadow->layer()->visible());
  window->SetIntProperty(aura::kShadowTypeKey, aura::SHADOW_TYPE_RECTANGULAR);
  EXPECT_TRUE(shadow->layer()->visible());

  // The shadow's layer should be a child of the window's layer.
  EXPECT_EQ(window->layer(), shadow->layer()->parent());

  window->parent()->RemoveChild(window.get());
  aura::Window* window_ptr = window.get();
  window.reset();
  EXPECT_TRUE(api.GetShadowForWindow(window_ptr) == NULL);
}

// Tests that the window's shadow's bounds are updated correctly.
TEST_F(ShadowControllerTest, ShadowBounds) {
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(aura::WINDOW_TYPE_NORMAL);
  window->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window->SetParent(NULL);
  window->Show();

  const gfx::Rect kOldBounds(20, 30, 400, 300);
  window->SetBounds(kOldBounds);

  // When the shadow is first created, it should use the window's size (but
  // remain at the origin, since it's a child of the window's layer).
  window->SetIntProperty(aura::kShadowTypeKey, aura::SHADOW_TYPE_RECTANGULAR);
  internal::ShadowController::TestApi api(
      aura_shell::Shell::GetInstance()->shadow_controller());
  const internal::Shadow* shadow = api.GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);
  EXPECT_EQ(gfx::Rect(kOldBounds.size()).ToString(),
            shadow->content_bounds().ToString());

  // When we change the window's bounds, the shadow's should be updated too.
  gfx::Rect kNewBounds(50, 60, 500, 400);
  window->SetBounds(kNewBounds);
  EXPECT_EQ(gfx::Rect(kNewBounds.size()).ToString(),
            shadow->content_bounds().ToString());
}

}  // namespace test
}  // namespace aura_shell
