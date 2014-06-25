// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/box_layout.h"

#include "ui/gfx/rect.h"
#include "ui/views/view.h"

namespace views {

BoxLayout::BoxLayout(BoxLayout::Orientation orientation,
                     int inside_border_horizontal_spacing,
                     int inside_border_vertical_spacing,
                     int between_child_spacing)
    : orientation_(orientation),
      inside_border_insets_(inside_border_vertical_spacing,
                            inside_border_horizontal_spacing,
                            inside_border_vertical_spacing,
                            inside_border_horizontal_spacing),
      between_child_spacing_(between_child_spacing),
      main_axis_alignment_(MAIN_AXIS_ALIGNMENT_START),
      cross_axis_alignment_(CROSS_AXIS_ALIGNMENT_STRETCH) {
}

BoxLayout::~BoxLayout() {
}

void BoxLayout::Layout(View* host) {
  gfx::Rect child_area(host->GetLocalBounds());
  child_area.Inset(host->GetInsets());
  child_area.Inset(inside_border_insets_);

  int padding = 0;
  if (main_axis_alignment_ != MAIN_AXIS_ALIGNMENT_START) {
    int total_main_axis_size = 0;
    int num_visible = 0;
    for (int i = 0; i < host->child_count(); ++i) {
      View* child = host->child_at(i);
      if (!child->visible())
        continue;
      total_main_axis_size += MainAxisSizeForView(child, child_area.width()) +
                              between_child_spacing_;
      ++num_visible;
    }

    if (num_visible) {
      total_main_axis_size -= between_child_spacing_;
      int free_space = MainAxisSize(child_area) - total_main_axis_size;
      int position = MainAxisPosition(child_area);
      int size = MainAxisSize(child_area);
      switch (main_axis_alignment_) {
        case MAIN_AXIS_ALIGNMENT_FILL:
          padding = std::max(free_space / num_visible, 0);
          break;
        case MAIN_AXIS_ALIGNMENT_CENTER:
          position += free_space / 2;
          size = total_main_axis_size;
          break;
        case MAIN_AXIS_ALIGNMENT_END:
          position += free_space;
          size = total_main_axis_size;
          break;
        default:
          NOTREACHED();
          break;
      }
      gfx::Rect new_child_area(child_area);
      SetMainAxisPosition(position, &new_child_area);
      SetMainAxisSize(size, &new_child_area);
      child_area.Intersect(new_child_area);
    }
  }

  int main_position = MainAxisPosition(child_area);
  for (int i = 0; i < host->child_count(); ++i) {
    View* child = host->child_at(i);
    if (child->visible()) {
      gfx::Rect bounds(child_area);
      SetMainAxisPosition(main_position, &bounds);
      if (cross_axis_alignment_ != CROSS_AXIS_ALIGNMENT_STRETCH) {
        int free_space = CrossAxisSize(bounds) - CrossAxisSizeForView(child);
        int position = CrossAxisPosition(bounds);
        if (cross_axis_alignment_ == CROSS_AXIS_ALIGNMENT_CENTER) {
          position += free_space / 2;
        } else if (cross_axis_alignment_ == CROSS_AXIS_ALIGNMENT_END) {
          position += free_space;
        }
        SetCrossAxisPosition(position, &bounds);
        SetCrossAxisSize(CrossAxisSizeForView(child), &bounds);
      }
      int child_main_axis_size = MainAxisSizeForView(child, child_area.width());
      SetMainAxisSize(child_main_axis_size + padding, &bounds);
      if (MainAxisSize(bounds) > 0)
        main_position += MainAxisSize(bounds) + between_child_spacing_;

      // Clamp child view bounds to |child_area|.
      bounds.Intersect(child_area);
      child->SetBoundsRect(bounds);
    }
  }
}

gfx::Size BoxLayout::GetPreferredSize(const View* host) const {
  // Calculate the child views' preferred width.
  int width = 0;
  if (orientation_ == kVertical) {
    for (int i = 0; i < host->child_count(); ++i) {
      const View* child = host->child_at(i);
      if (!child->visible())
        continue;

      width = std::max(width, child->GetPreferredSize().width());
    }
  }

  return GetPreferredSizeForChildWidth(host, width);
}

int BoxLayout::GetPreferredHeightForWidth(const View* host, int width) const {
  int child_width = width - NonChildSize(host).width();
  return GetPreferredSizeForChildWidth(host, child_width).height();
}

int BoxLayout::MainAxisSize(const gfx::Rect& rect) const {
  return orientation_ == kHorizontal ? rect.width() : rect.height();
}

int BoxLayout::MainAxisPosition(const gfx::Rect& rect) const {
  return orientation_ == kHorizontal ? rect.x() : rect.y();
}

void BoxLayout::SetMainAxisSize(int size, gfx::Rect* rect) const {
  if (orientation_ == kHorizontal)
    rect->set_width(size);
  else
    rect->set_height(size);
}

void BoxLayout::SetMainAxisPosition(int position, gfx::Rect* rect) const {
  if (orientation_ == kHorizontal)
    rect->set_x(position);
  else
    rect->set_y(position);
}

int BoxLayout::CrossAxisSize(const gfx::Rect& rect) const {
  return orientation_ == kVertical ? rect.width() : rect.height();
}

int BoxLayout::CrossAxisPosition(const gfx::Rect& rect) const {
  return orientation_ == kVertical ? rect.x() : rect.y();
}

void BoxLayout::SetCrossAxisSize(int size, gfx::Rect* rect) const {
  if (orientation_ == kVertical)
    rect->set_width(size);
  else
    rect->set_height(size);
}

void BoxLayout::SetCrossAxisPosition(int position, gfx::Rect* rect) const {
  if (orientation_ == kVertical)
    rect->set_x(position);
  else
    rect->set_y(position);
}

int BoxLayout::MainAxisSizeForView(const View* view,
                                   int child_area_width) const {
  return orientation_ == kHorizontal
             ? view->GetPreferredSize().width()
             : view->GetHeightForWidth(cross_axis_alignment_ ==
                                               CROSS_AXIS_ALIGNMENT_STRETCH
                                           ? child_area_width
                                           : view->GetPreferredSize().width());
}

int BoxLayout::CrossAxisSizeForView(const View* view) const {
  return orientation_ == kVertical
             ? view->GetPreferredSize().width()
             : view->GetHeightForWidth(view->GetPreferredSize().width());
}

gfx::Size BoxLayout::GetPreferredSizeForChildWidth(const View* host,
                                                   int child_area_width) const {
  gfx::Rect child_area_bounds;

  if (orientation_ == kHorizontal) {
    // Horizontal layouts ignore |child_area_width|, meaning they mimic the
    // default behavior of GridLayout::GetPreferredHeightForWidth().
    // TODO(estade): fix this if it ever becomes a problem.
    int position = 0;
    for (int i = 0; i < host->child_count(); ++i) {
      const View* child = host->child_at(i);
      if (!child->visible())
        continue;

      gfx::Size size(child->GetPreferredSize());
      if (size.IsEmpty())
        continue;

      gfx::Rect child_bounds(position, 0, size.width(), size.height());
      child_area_bounds.Union(child_bounds);
      position += size.width() + between_child_spacing_;
    }
  } else {
    int height = 0;
    for (int i = 0; i < host->child_count(); ++i) {
      const View* child = host->child_at(i);
      if (!child->visible())
        continue;

      // Use the child area width for getting the height if the child is
      // supposed to stretch. Use its preferred size otherwise.
      int extra_height = MainAxisSizeForView(child, child_area_width);
      // Only add |between_child_spacing_| if this is not the only child.
      if (height != 0 && extra_height > 0)
        height += between_child_spacing_;
      height += extra_height;
    }

    child_area_bounds.set_width(child_area_width);
    child_area_bounds.set_height(height);
  }

  gfx::Size non_child_size = NonChildSize(host);
  return gfx::Size(child_area_bounds.width() + non_child_size.width(),
                   child_area_bounds.height() + non_child_size.height());
}

gfx::Size BoxLayout::NonChildSize(const View* host) const {
  gfx::Insets insets(host->GetInsets());
  return gfx::Size(insets.width() + inside_border_insets_.width(),
                   insets.height() + inside_border_insets_.height());
}

} // namespace views
