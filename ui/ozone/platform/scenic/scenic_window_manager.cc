// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window_manager.h"

#include "base/fuchsia/component_context.h"

namespace ui {

ScenicWindowManager::ScenicWindowManager() = default;
ScenicWindowManager::~ScenicWindowManager() = default;

int32_t ScenicWindowManager::AddWindow(ScenicWindow* window) {
  return windows_.Add(window);
}

void ScenicWindowManager::RemoveWindow(int32_t window_id,
                                       ScenicWindow* window) {
  DCHECK_EQ(window, windows_.Lookup(window_id));
  windows_.Remove(window_id);
}

ScenicWindow* ScenicWindowManager::GetWindow(int32_t window_id) {
  return windows_.Lookup(window_id);
}

}  // namespace ui
