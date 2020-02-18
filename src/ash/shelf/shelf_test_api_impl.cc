// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_test_api_impl.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"

namespace ash {

ShelfTestApiImpl::ShelfTestApiImpl() = default;
ShelfTestApiImpl::~ShelfTestApiImpl() = default;

bool ShelfTestApiImpl::IsVisible() {
  Shelf* shelf = Shell::Get()->GetPrimaryRootWindowController()->shelf();
  return shelf->shelf_layout_manager()->IsVisible();
}

bool ShelfTestApiImpl::IsAlignmentBottomLocked() {
  Shelf* shelf = Shell::Get()->GetPrimaryRootWindowController()->shelf();
  return shelf->alignment() == SHELF_ALIGNMENT_BOTTOM_LOCKED;
}

// static
std::unique_ptr<ShelfTestApi> ShelfTestApi::Create() {
  return std::make_unique<ShelfTestApiImpl>();
}

}  // namespace ash
