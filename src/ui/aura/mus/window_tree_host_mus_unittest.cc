// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_host_mus.h"

#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/test/aura_mus_test_base.h"
#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/aura/window.h"

namespace aura {

using WindowTreeHostMusTest = test::AuraMusClientTestBase;

TEST_F(WindowTreeHostMusTest, UpdateClientArea) {
  std::unique_ptr<WindowTreeHostMus> window_tree_host_mus =
      std::make_unique<WindowTreeHostMus>(
          CreateInitParamsForTopLevel(window_tree_client_impl()));

  gfx::Insets new_insets(10, 11, 12, 13);
  window_tree_host_mus->SetClientArea(new_insets, std::vector<gfx::Rect>());
  EXPECT_EQ(new_insets, window_tree()->last_client_area());
}

TEST_F(WindowTreeHostMusTest, DontGenerateNewSurfaceIdOnMove) {
  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      std::make_unique<WindowTreeHostMus>(
          CreateInitParamsForTopLevel(window_tree_client_impl()));
  window_tree_host->InitHost();
  Window* window = window_tree_host->window();
  ws::mojom::WindowDataPtr data = ws::mojom::WindowData::New();
  data->window_id = WindowMus::Get(window)->server_id();
  data->visible = true;
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator;
  parent_local_surface_id_allocator.GenerateId();
  uint32_t change_id = 0;
  ASSERT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
      WindowTreeChangeType::NEW_TOP_LEVEL, &change_id));
  window_tree_client()->OnTopLevelCreated(
      change_id, std::move(data), 1u, true,
      parent_local_surface_id_allocator.GetCurrentLocalSurfaceIdAllocation());
  window_tree_host->SetBounds(gfx::Rect(1, 2, 3, 4),
                              viz::LocalSurfaceIdAllocation());
  window_tree()->AckAllChanges();
  EXPECT_EQ(0u,
            window_tree()->GetChangeCountForType(WindowTreeChangeType::BOUNDS));
  const viz::LocalSurfaceIdAllocation lsia =
      window_tree_host->compositor()->GetLocalSurfaceIdAllocation();

  // Setting the bounds to the same size, but different location should not
  // generate a new id.
  window_tree_host->SetBounds(gfx::Rect(1, 21, 3, 4),
                              viz::LocalSurfaceIdAllocation());
  EXPECT_EQ(lsia,
            window_tree_host->compositor()->GetLocalSurfaceIdAllocation());
  EXPECT_EQ(1u, window_tree()->number_of_changes());
  EXPECT_EQ(1u,
            window_tree()->GetChangeCountForType(WindowTreeChangeType::BOUNDS));
  ASSERT_TRUE(window_tree()->last_local_surface_id());
  EXPECT_EQ(lsia.local_surface_id(), window_tree()->last_local_surface_id());
  window_tree()->AckAllChanges();
}

}  // namespace aura
