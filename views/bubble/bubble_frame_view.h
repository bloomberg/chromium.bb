// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
#define VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
#pragma once

#include "views/bubble/bubble_border.h"
#include "views/window/non_client_view.h"

namespace views {

//  BubbleFrameView to render BubbleBorder.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BubbleFrameView : public NonClientFrameView {
 public:
  BubbleFrameView(BubbleBorder::ArrowLocation location,
                  const gfx::Size& client_size,
                  SkColor color);
  virtual ~BubbleFrameView();

  // NonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE {}
  virtual void EnableClose(bool enable) OVERRIDE {}
  virtual void ResetWindowControls() OVERRIDE {}
  virtual void UpdateWindowIcon() OVERRIDE {}

  // View overrides:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BubbleFrameView);
};

}  // namespace views

#endif  // VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
