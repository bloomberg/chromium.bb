// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_LAYOUT_MANAGER_H_
#define UI_AURA_LAYOUT_MANAGER_H_
#pragma once

namespace aura {

// An interface implemented by an object that places child windows.
class LayoutManager {
 public:
  virtual ~LayoutManager() {}

  // Called when the window is resized.
  virtual void OnWindowResized() = 0;
};

}  // namespace aura

#endif  // UI_AURA_LAYOUT_MANAGER_H_
