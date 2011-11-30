// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BORDER_CONTENTS_VIEW_H_
#define UI_VIEWS_BUBBLE_BORDER_CONTENTS_VIEW_H_
#pragma once

#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/view.h"

namespace views {

// This is used to paint the border and background of the Bubble.
class VIEWS_EXPORT BorderContentsView : public View {
 public:
  BorderContentsView();
  BorderContentsView(int top_margin,
                     int left_margin,
                     int bottom_margin,
                     int right_margin);

  // Must be called before this object can be used.
  void Init();

  // Sets the background color.
  void SetBackgroundColor(SkColor color);

  // Sets the bubble alignment.
  void SetAlignment(views::BubbleBorder::BubbleAlignment alignment);

  // Given the size of the contents and the rect to point at, returns the bounds
  // of both the border and the contents inside the bubble.
  // |arrow_location| specifies the preferred location for the arrow
  // anchor. If the bubble does not fit on the monitor and
  // |allow_bubble_offscreen| is false, the arrow location may change so the
  // bubble shows entirely.
  virtual void SizeAndGetBounds(
      const gfx::Rect& position_relative_to,  // In screen coordinates
      BubbleBorder::ArrowLocation arrow_location,
      bool allow_bubble_offscreen,
      const gfx::Size& contents_size,
      gfx::Rect* contents_bounds,             // Returned in window coordinates
      gfx::Rect* window_bounds);              // Returned in screen coordinates

  // Sets content margins.
  void set_content_margins(const gfx::Insets& margins) {
    content_margins_ = margins;
  }

  // Accessor for |content_margins_|.
  const gfx::Insets& content_margins() const {
    return content_margins_;
  }

 protected:
  virtual ~BorderContentsView();

  // Returns the bounds for the monitor showing the specified |rect|.
  virtual gfx::Rect GetMonitorBounds(const gfx::Rect& rect);

  BubbleBorder* bubble_border() const { return bubble_border_; }

 private:
  // Changes |arrow_location| to its mirrored version, vertically if |vertical|
  // is true, horizontally otherwise, if |window_bounds| don't fit in
  // |monitor_bounds|.
  void MirrorArrowIfOffScreen(
      bool vertical,
      const gfx::Rect& position_relative_to,
      const gfx::Rect& monitor_bounds,
      const gfx::Size& local_contents_size,
      BubbleBorder::ArrowLocation* arrow_location,
      gfx::Rect* window_bounds);

  // The bubble border.
  BubbleBorder* bubble_border_;

  // Margins between the content and the inside of the border, in pixels.
  gfx::Insets content_margins_;

  DISALLOW_COPY_AND_ASSIGN(BorderContentsView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BORDER_CONTENTS_VIEW_H_
