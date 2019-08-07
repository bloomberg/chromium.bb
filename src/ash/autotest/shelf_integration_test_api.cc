// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autotest/shelf_integration_test_api.h"

#include <memory>
#include <utility>

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace ash {

namespace {

// Returns the Shelf instance for the display with the given |display_id|.
Shelf* GetShelfForDisplay(int64_t display_id) {
  // The controller may be null for invalid ids or for displays being removed.
  RootWindowController* root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(display_id);
  return root_window_controller ? root_window_controller->shelf() : nullptr;
}
}  // namespace

ShelfIntegrationTestApi::ShelfIntegrationTestApi() = default;
ShelfIntegrationTestApi::~ShelfIntegrationTestApi() = default;

// static
void ShelfIntegrationTestApi::BindRequest(
    mojom::ShelfIntegrationTestApiRequest request) {
  mojo::MakeStrongBinding(std::make_unique<ShelfIntegrationTestApi>(),
                          std::move(request));
}

void ShelfIntegrationTestApi::GetAutoHideBehavior(
    int64_t display_id,
    GetAutoHideBehaviorCallback callback) {
  Shelf* shelf = GetShelfForDisplay(display_id);
  DCHECK(shelf);
  std::move(callback).Run(shelf->auto_hide_behavior());
}

void ShelfIntegrationTestApi::SetAutoHideBehavior(
    int64_t display_id,
    ShelfAutoHideBehavior behavior,
    SetAutoHideBehaviorCallback callback) {
  Shelf* shelf = GetShelfForDisplay(display_id);
  DCHECK(shelf);
  shelf->SetAutoHideBehavior(behavior);
  std::move(callback).Run();
}

void ShelfIntegrationTestApi::GetAlignment(int64_t display_id,
                                           GetAlignmentCallback callback) {
  Shelf* shelf = GetShelfForDisplay(display_id);
  DCHECK(shelf);
  std::move(callback).Run(shelf->alignment());
}

void ShelfIntegrationTestApi::SetAlignment(int64_t display_id,
                                           ShelfAlignment alignment,
                                           SetAlignmentCallback callback) {
  Shelf* shelf = GetShelfForDisplay(display_id);
  DCHECK(shelf);
  shelf->SetAlignment(alignment);
  std::move(callback).Run();
}

}  // namespace ash
