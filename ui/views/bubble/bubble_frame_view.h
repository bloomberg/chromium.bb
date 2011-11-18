// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
#define UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "ui/views/window/non_client_view.h"
#include "views/bubble/bubble_border.h"

namespace views {

class BorderContentsView;

//  BubbleFrameView to render BubbleBorder.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BubbleFrameView : public NonClientFrameView {
 public:
  BubbleFrameView(BubbleBorder::ArrowLocation location,
                  const gfx::Size& client_size,
                  SkColor color,
                  bool allow_bubble_offscreen);
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
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Accessor for bubble border inside border contents.
  BubbleBorder* bubble_border() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewBasicTest, GetBoundsForClientView);

  BorderContentsView* border_contents_;
  BubbleBorder::ArrowLocation location_;
  bool allow_bubble_offscreen_;

  DISALLOW_COPY_AND_ASSIGN(BubbleFrameView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
