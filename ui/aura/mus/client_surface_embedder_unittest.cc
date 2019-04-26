// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/client_surface_embedder.h"

#include "ui/aura/test/aura_mus_test_base.h"
#include "ui/aura/test/mus/window_port_mus_test_helper.h"

namespace aura {

namespace {

using ClientSurfaceEmbedderTest = test::AuraMusClientTestBase;

// Test that reparenting a window from a hidden parent to a visible one updates
// the embedded surface layer visibility. https://crbug.com/956822.
TEST_F(ClientSurfaceEmbedderTest, SurfaceVisibility) {
  std::unique_ptr<Window> parent1(
      CreateNormalWindow(100, root_window(), nullptr));
  parent1->Show();
  std::unique_ptr<Window> parent2(
      CreateNormalWindow(200, root_window(), nullptr));
  parent2->Show();
  std::unique_ptr<Window> window(
      CreateNormalWindow(10, parent1.get(), nullptr));
  window->Show();

  WindowPortMusTestHelper(window.get()).SimulateEmbedding();

  ClientSurfaceEmbedder* client_surface_embedder =
      WindowPortMusTestHelper(window.get()).GetClientSurfaceEmbedder();

  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(client_surface_embedder->GetSurfaceLayerForTesting()
                  ->GetTargetVisibility());

  // Hide the current parent and expect the visibility will be updated.
  parent1->Hide();
  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(client_surface_embedder->GetSurfaceLayerForTesting()
                   ->GetTargetVisibility());

  // Reparent the window from the hidden parent to the visible one.
  parent2->AddChild(window.get());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(client_surface_embedder->GetSurfaceLayerForTesting()
                  ->GetTargetVisibility());
}

}  // namespace

}  // namespace aura
