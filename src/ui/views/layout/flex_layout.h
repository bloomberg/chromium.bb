// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_FLEX_LAYOUT_H_
#define UI_VIEWS_LAYOUT_FLEX_LAYOUT_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_manager.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace views {

namespace internal {
struct ChildLayoutParams;
class FlexLayoutInternal;
}  // namespace internal

class View;

// Provides CSS-like layout for a one-dimensional (vertical or horizontal)
// arrangement of child views. Independent alignment can be specified for the
// main and cross axes.
//
// Per-View margins (provided by view property kMarginsKey) specify how much
// space to leave around each child view. The |interior_margin| says how much
// empty space to leave at the edges of the parent view. If |collapse_margins|
// is false, these values are additive; if true, the greater of the two values
// is used. The |default_child_margins| provides a fallback for views without
// kMarginsKey set.
//
// collapse_margins = false:
//
// | interior margin>                      <margin [view]...
// |                 <margin [view] margin>
//
// collapse_margins = true:
//
// | interior margin>      <margin [view]
// |         <margin [view] margin>       ...
//
// Views can have their own internal padding, using the kInternalPaddingKey
// property, which is subtracted from the margin space between child views.
//
// Calling SetVisible(false) on a child view outside of the FlexLayout will
// result in the child view being hidden until SetVisible(true) is called. This
// is irrespective of whether the FlexLayout has set the child view to be
// visible or not based on, for example, flex rules.
//
// If you want the host view to maintain control over a child view, you can
// exclude it from the layout. Excluded views are completely ignored during
// layout and do not have their properties modified.
//
// FlexSpecification objects determine how child views are sized. You can set
// individual flex rules for each child view, or a default for any child views
// without individual flex rules set. If you don't set anything, each view will
// take up its preferred size in the layout.
//
// The core function of this class is contained in
// GetPreferredSize(maximum_size) and Layout(host). In both cases, a layout will
// be cached and typically not recalculated as long as none of the layout's
// properties or the preferred size or visibility of any of its children has
// changed.
class VIEWS_EXPORT FlexLayout : public LayoutManager {
 public:
  FlexLayout();
  ~FlexLayout() override;

  // Note: setters provide a Builder-style interface, so you can type:
  // layout.SetMainAxisAlignment()
  //       .SetCrossAxisAlignment()
  //       .SetDefaultFlex(...);
  FlexLayout& SetOrientation(LayoutOrientation orientation);
  FlexLayout& SetCollapseMargins(bool collapse_margins);
  FlexLayout& SetMainAxisAlignment(LayoutAlignment main_axis_alignment);
  FlexLayout& SetCrossAxisAlignment(LayoutAlignment cross_axis_alignment);
  FlexLayout& SetInteriorMargin(const gfx::Insets& interior_margin);
  FlexLayout& SetMinimumCrossAxisSize(int size);
  FlexLayout& SetDefaultChildMargins(const gfx::Insets& margins);
  FlexLayout& SetFlexForView(const View* view,
                             const FlexSpecification& flex_specification);
  FlexLayout& ClearFlexForView(const View* view);
  FlexLayout& SetDefaultFlex(const FlexSpecification& flex_specification);

  // Set whether a view should be excluded from the layout. Excluded views will
  // be completely ignored and must be explicitly placed by the host view.
  FlexLayout& SetViewExcluded(const View* view, bool excluded);

  View* host() { return host_; }
  const View* host() const { return host_; }
  LayoutOrientation orientation() const { return orientation_; }
  bool collapse_margins() const { return collapse_margins_; }
  LayoutAlignment main_axis_alignment() const { return main_axis_alignment_; }
  LayoutAlignment cross_axis_alignment() const { return cross_axis_alignment_; }
  const gfx::Insets& interior_margin() const { return interior_margin_; }
  int minimum_cross_axis_size() const { return minimum_cross_axis_size_; }
  const gfx::Insets& default_child_margins() const {
    return default_child_margins_;
  }
  const FlexSpecification& default_flex() const { return default_flex_; }

  const FlexSpecification& GetFlexForView(const View* view) const;
  bool IsViewExcluded(const View* view) const;
  bool IsHiddenByOwner(const View* view) const;

  // Retrieve the preferred size for the control in the given bounds.
  gfx::Size GetPreferredSize(const SizeBounds& maximum_size) const;

 protected:
  // views::LayoutManager:
  void Installed(View* host) override;
  void InvalidateLayout() override;
  void ViewAdded(View* host, View* view) override;
  void ViewRemoved(View* host, View* view) override;
  void ViewVisibilitySet(View* host, View* view, bool visible) override;
  void Layout(View* host) override;
  gfx::Size GetPreferredSize(const View* host) const override;
  gfx::Size GetMinimumSize(const View* host) const override;
  int GetPreferredHeightForWidth(const View* host, int width) const override;

 private:
  friend class internal::FlexLayoutInternal;

  LayoutOrientation orientation_ = LayoutOrientation::kHorizontal;

  // Adjacent view margins should be collapsed.
  bool collapse_margins_ = false;

  // Spacing between child views and host view border.
  gfx::Insets interior_margin_;

  // Default spacing to put around child views.
  gfx::Insets default_child_margins_;

  // Child-view-specific details (e.g. hidden status, flex rules, etc.)
  std::map<const View*, internal::ChildLayoutParams> child_params_;

  // The alignment of children in the main axis. This is start by default.
  LayoutAlignment main_axis_alignment_ = LayoutAlignment::kStart;

  // The alignment of children in the cross axis. This is stretch by default.
  LayoutAlignment cross_axis_alignment_ = LayoutAlignment::kStretch;

  // The minimum cross axis size for the layout.
  int minimum_cross_axis_size_ = 0;

  // Flex specification for components with no flex set.
  FlexSpecification default_flex_;

  // The view that this FlexLayout is managing the layout for.
  views::View* host_ = nullptr;

  // Internal data used to cache layout information, etc. All definitions and
  // data are module-private.
  std::unique_ptr<internal::FlexLayoutInternal> internal_;

  DISALLOW_COPY_AND_ASSIGN(FlexLayout);
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_FLEX_LAYOUT_H_
