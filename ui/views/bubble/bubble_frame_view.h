// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
#define UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/window/non_client_view.h"

namespace views {

class BorderContentsView;

//  BubbleFrameView to render BubbleBorder.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BubbleFrameView : public NonClientFrameView {
 public:
  BubbleFrameView(BubbleBorder::ArrowLocation arrow_location,
                  SkColor color,
                  int margin);
  virtual ~BubbleFrameView();

  // NonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE {}
  virtual void ResetWindowControls() OVERRIDE {}
  virtual void UpdateWindowIcon() OVERRIDE {}

  // View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  BubbleBorder* bubble_border() const { return bubble_border_; }

  gfx::Insets content_margins() const { return content_margins_; }

  // Given the size of the contents and the rect to point at, returns the bounds
  // of the bubble window. The bubble's arrow location may change if the bubble
  // does not fit on the monitor and |try_mirroring_arrow| is true.
  gfx::Rect GetUpdatedWindowBounds(const gfx::Rect& anchor_rect,
                                   gfx::Size client_size,
                                   bool try_mirroring_arrow);

  void SetBubbleBorder(BubbleBorder* border);

 protected:
  // Returns the bounds for the monitor showing the specified |rect|.
  // This function is virtual to support testing environments.
  virtual gfx::Rect GetMonitorBounds(const gfx::Rect& rect);

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, GetBoundsForClientView);

  // Mirrors the bubble's arrow location on the |vertical| or horizontal axis,
  // if the generated window bounds don't fit in the monitor bounds.
  void MirrorArrowIfOffScreen(bool vertical,
                              const gfx::Rect& anchor_rect,
                              const gfx::Size& client_size);

  // The bubble border.
  BubbleBorder* bubble_border_;

  // Margins between the content and the inside of the border, in pixels.
  gfx::Insets content_margins_;

  DISALLOW_COPY_AND_ASSIGN(BubbleFrameView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_FRAME_VIEW_H_
