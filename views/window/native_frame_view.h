// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_
#define VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_
#pragma once

#include "views/window/non_client_view.h"

namespace views {

class WindowWin;

class NativeFrameView : public NonClientFrameView {
 public:
  explicit NativeFrameView(WindowWin* frame);
  virtual ~NativeFrameView();

  // NonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void EnableClose(bool enable) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;

  // View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  // Our containing frame.
  WindowWin* frame_;

  DISALLOW_COPY_AND_ASSIGN(NativeFrameView);
};

}  // namespace views

#endif  // #ifndef VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_
