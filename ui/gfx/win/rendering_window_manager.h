// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_RENDERING_WINDOW_MANAGER_H_
#define UI_GFX_WIN_RENDERING_WINDOW_MANAGER_H_

#include <windows.h>

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/gfx_export.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace gfx {

// This keeps track of whether a given HWND has a child window which the GPU
// process renders into.
class GFX_EXPORT RenderingWindowManager {
 public:
  static RenderingWindowManager* GetInstance();

  void RegisterParent(HWND parent);
  // Regiters |child| as child window for |parent| with ::SetParent() so the GPU
  // process can draw into |child|. RegisterParent() must have already been
  // called for |parent| otherwise this will fail. RegisterChild() also cannot
  // already have been called for |parent| otherwise this will fail.
  bool RegisterChild(HWND parent, HWND child);
  void UnregisterParent(HWND parent);
  bool HasValidChildWindow(HWND parent);

 private:
  friend class base::NoDestructor<RenderingWindowManager>;

  RenderingWindowManager();
  ~RenderingWindowManager();

  base::Lock lock_;
  // Map from registered parent HWND to child HWND.
  base::flat_map<HWND, HWND> registered_hwnds_;

  DISALLOW_COPY_AND_ASSIGN(RenderingWindowManager);
};

}  // namespace gfx

#endif  // UI_GFX_WIN_RENDERING_WINDOW_MANAGER_H_
