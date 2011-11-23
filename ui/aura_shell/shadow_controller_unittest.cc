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

  // We shouldn't create the shadow before the window is visible.
  internal::ShadowController::TestApi api(
      aura_shell::Shell::GetInstance()->shadow_controller());
  EXPECT_TRUE(api.GetShadowForWindow(window.get()) == NULL);

  // The shadow's visibility should be updated along with the window's.
  window->Show();
  const internal::Shadow* shadow = api.GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);
  EXPECT_TRUE(shadow->layer()->visible());
  window->Hide();
  EXPECT_FALSE(shadow->layer()->visible());

  // If the shadow is disabled, it shouldn't be shown even when the window is.
  window->SetIntProperty(aura::kShadowTypeKey, aura::SHADOW_TYPE_NONE);
  window->Show();
  EXPECT_FALSE(shadow->layer()->visible());
  window->SetIntProperty(aura::kShadowTypeKey, aura::SHADOW_TYPE_RECTANGULAR);
  EXPECT_TRUE(shadow->layer()->visible());

  // The shadow's layer should have the same parent as the window's.
  EXPECT_EQ(window->parent()->layer(), shadow->layer()->parent());

  // When we remove the window from the hierarchy, its shadow should be removed
  // too.
  window->parent()->RemoveChild(window.get());
  EXPECT_TRUE(shadow->layer()->parent() == NULL);

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

  // When the shadow is first created, it should use the window's bounds.
  window->SetIntProperty(aura::kShadowTypeKey, aura::SHADOW_TYPE_RECTANGULAR);
  internal::ShadowController::TestApi api(
      aura_shell::Shell::GetInstance()->shadow_controller());
  const internal::Shadow* shadow = api.GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);
  EXPECT_EQ(kOldBounds, shadow->content_bounds());

  // When we change the window's bounds, the shadow's should be updated too.
  gfx::Rect kNewBounds(50, 60, 500, 400);
  window->SetBounds(kNewBounds);
  EXPECT_EQ(kNewBounds, shadow->content_bounds());
}

// Test that shadows are stacked correctly.
TEST_F(ShadowControllerTest, Stacking) {
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(aura::WINDOW_TYPE_NORMAL);
  window->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window->SetParent(NULL);
  window->Show();

  // Create a second window.  It will appear above the first window.
  scoped_ptr<aura::Window> window2(new aura::Window(NULL));
  window2->SetType(aura::WINDOW_TYPE_NORMAL);
  window2->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window2->SetParent(NULL);
  window2->Show();

  // Enable a shadow on the first window.
  window->SetIntProperty(aura::kShadowTypeKey, aura::SHADOW_TYPE_RECTANGULAR);
  internal::ShadowController::TestApi api(
      aura_shell::Shell::GetInstance()->shadow_controller());
  const internal::Shadow* shadow = api.GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);

  // Check that the second window is above the first window and that the first
  // window is above its shadow.
  ui::Layer* parent_layer = window->layer()->parent();
  ASSERT_EQ(parent_layer, shadow->layer()->parent());
  ASSERT_EQ(parent_layer, window2->layer()->parent());
  const std::vector<ui::Layer*>& layers = parent_layer->children();
  EXPECT_GT(std::find(layers.begin(), layers.end(), window2->layer()),
            std::find(layers.begin(), layers.end(), window->layer()));
  EXPECT_GT(std::find(layers.begin(), layers.end(), window->layer()),
            std::find(layers.begin(), layers.end(), shadow->layer()));

  // Raise the first window to the top and check that its shadow comes with it.
  window->parent()->StackChildAtTop(window.get());
  EXPECT_GT(std::find(layers.begin(), layers.end(), window->layer()),
            std::find(layers.begin(), layers.end(), shadow->layer()));
  EXPECT_GT(std::find(layers.begin(), layers.end(), shadow->layer()),
            std::find(layers.begin(), layers.end(), window2->layer()));
}

}  // namespace test
}  // namespace aura_shell
