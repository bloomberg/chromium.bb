// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_VIEWS_UTILS_H_
#define ASH_LOGIN_UI_VIEWS_UTILS_H_

#include "ash/ash_export.h"
#include "ui/views/controls/label.h"

namespace views {
class FocusRing;
class View;
class Widget;
}  // namespace views

namespace ash {

namespace login_views_utils {

// Wraps view in another view so the original view is sized to it's preferred
// size, regardless of the view's parent's layout manager.
ASH_EXPORT std::unique_ptr<views::View> WrapViewForPreferredSize(
    std::unique_ptr<views::View> view);

// Returns true if landscape constants should be used for UI shown in |widget|.
ASH_EXPORT bool ShouldShowLandscape(const views::Widget* widget);

// Returns true if |view| or any of its descendant views HasFocus.
ASH_EXPORT bool HasFocusInAnyChildView(views::View* view);

// Creates a standard text label for use in the login bubbles.
// If |view_defining_max_width| is set, we allow the label to have multiple
// lines and we set its maximum width to the preferred width of
// |view_defining_max_width|. |font_size_delta| is the size in pixels to add to
// the default font size.
views::Label* CreateBubbleLabel(
    const base::string16& message,
    SkColor color,
    views::View* view_defining_max_width = nullptr,
    int font_size_delta = 0,
    gfx::Font::Weight font_weight = gfx::Font::Weight::NORMAL);

// Get the bubble container for |view| to place a LoginBaseBubbleView.
views::View* GetBubbleContainer(views::View* view);

ASH_EXPORT gfx::Point CalculateBubblePositionLeftRightStrategy(
    gfx::Rect anchor,
    gfx::Size bubble,
    gfx::Rect bounds);

ASH_EXPORT gfx::Point CalculateBubblePositionRightLeftStrategy(
    gfx::Rect anchor,
    gfx::Size bubble,
    gfx::Rect bounds);

// Applies a rectangular focus ring to |focus_ring| and round ink drop to
// |view|. |focus_ring| may not be the ring associated with |view|. If |radius|
// is passed the ink drop will be a circle with radius |radius| otherwise its
// radius will be determined by the view's bounds.
void ConfigureRectFocusRingCircleInkDrop(views::View* view,
                                         views::FocusRing* focus_ring,
                                         base::Optional<int> radius);

}  // namespace login_views_utils

}  // namespace ash

#endif  // ASH_LOGIN_UI_VIEWS_UTILS_H_
