// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_FLEX_LAYOUT_TYPES_H_
#define UI_VIEWS_LAYOUT_FLEX_LAYOUT_TYPES_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "ui/views/views_export.h"

namespace gfx {
class Size;
}

namespace views {

class View;

// Whether a layout is oriented horizontally or vertically.
enum class LayoutOrientation {
  kHorizontal,
  kVertical,
};

// Describes how elements should be aligned within a layout.
enum class LayoutAlignment { kStart, kCenter, kEnd, kStretch };

// Stores an optional width and height upper bound. Used when calculating the
// preferred size of a layout pursuant to a maximum available size.
class VIEWS_EXPORT SizeBounds {
 public:
  SizeBounds();
  SizeBounds(const base::Optional<int>& width,
             const base::Optional<int>& height);
  explicit SizeBounds(const gfx::Size& size);
  SizeBounds(const SizeBounds& other);

  const base::Optional<int>& width() const { return width_; }
  void set_width(const base::Optional<int>& width) { width_ = width; }

  const base::Optional<int>& height() const { return height_; }
  void set_height(const base::Optional<int>& height) { height_ = height; }

  // Enlarges (or shrinks, if negative) each upper bound that is present by the
  // specified amounts.
  void Enlarge(int width, int height);

  bool operator==(const SizeBounds& other) const;
  bool operator!=(const SizeBounds& other) const;
  bool operator<(const SizeBounds& other) const;

  std::string ToString() const;

 private:
  base::Optional<int> width_;
  base::Optional<int> height_;
};

// Callback used to specify the size of a child view based on its size bounds.
// Create your own custom rules, or use the Minimum|MaximumFlexSizeRule
// constants below for common behaviors.
//
// This callback takes two parameters: a child view, and a set of size bounds
// representing the available space for that child view to occupy. The function
// returns the preferred size of the view within those bounds, which may exceed
// them if the child is not capable of shrinking to the specified size. The
// callback may also return an empty size, which means the child view can drop
// out of the layout. Not specifying either bound means there is an unlimited
// amount of room for the child view in that dimension (and the child view
// should probably use its preferred size).
//
// We provide the ability to use an arbitrary function here because some views
// have complex sizing behavior; for example, they may shrink stepwise as their
// internal elements drop out due to lack of space.
using FlexRule =
    base::RepeatingCallback<gfx::Size(const View*, const SizeBounds&)>;

// Describes a simple rule for how a child view should shrink in a layout when
// the available size for that view decreases.
enum class MinimumFlexSizeRule {
  kScaleToZero,               // Ignore minimum size and scale all the way down.
  kScaleToMinimumSnapToZero,  // Scale to minimum then snap to zero.
  kPreferredSnapToZero,       // Use preferred, then snap to zero.
  kScaleToMinimum,            // Resize down to minimum then stop.
  kPreferredSnapToMinimum,    // Use preferred, then snap to minimum.
  kPreferred                  // Always use preferred size.
};

// Describes a simple rule for how a child view should grow in a layout when
// there is extra size avaialble for that view to occupy.
enum MaximumFlexSizeRule {
  kPreferred,  // Don't resize above preferred size.
  kUnbounded   // Allow resize to arbitrary size.
};

// Specifies how a view should flex (i.e. grow or shrink) within its parent as
// the available space changes. Flex specifications have three components:
//  - A |rule| which tells the layout manager how the child view resizes with
//    available space.
//  - A |weight| which specifies how much of the available space is allocated to
//    a child view that can flex. The percentage of space allocated is the
//    weight divided by the total weight of all views at this order (see below).
//  - An |order| which specifies the priority with which available space is
//    allocated. All available space is offered to child views at order 1, then
//    any remaining space is offered to order 2, and so forth.
//
// For example, say there are three child controls in a horizontal layout, each
// of which has a flex rule that allows it to be between 0 and 20 DIPs wide.
// Child A is at order 2 with weight 2, child B is at order 1 with weight 1, and
// child C is at order 2 and weight 1. The parent control is 50 DIPs across and
// has no margins.
//
// All 50 DIPs are offered to child B, since it is first in order. It consumes
// 20 DIPs, its maximum size. Of the remaining 30, 20 are offered to child A and
// 10 are offered to child C, each of which they take - the 2:1 ratio is due to
// the different weights. (Also note that, if there were another child at order
// 3, it would be offered zero DIPs and might choose not to display itself.)
class VIEWS_EXPORT FlexSpecification {
 public:
  // Creates a flex specification with the default rule (no flex, always use the
  // view's preferred size).
  FlexSpecification();

  FlexSpecification(const FlexSpecification& other);
  FlexSpecification& operator=(const FlexSpecification& other);

  ~FlexSpecification();

  // Creates a flex specification with a custom flex rule. Note that any copies
  // or mutations of this specification will also inherit the rule.
  static FlexSpecification ForCustomRule(FlexRule rule);

  // Creates a flex specification using the specififed minimum size and size
  // bounds rules.
  static FlexSpecification ForSizeRule(MinimumFlexSizeRule minimum_size_rule,
                                       MaximumFlexSizeRule maximum_size_rule);

  // Makes a copy of this specification with a different order.
  FlexSpecification WithOrder(int order) const;

  // Makes a copy of this specification with a different weight.
  // Specifying |weight| of zero means the view will take as much space as it
  // needs.
  FlexSpecification WithWeight(int weight) const;

  const FlexRule& rule() const { return rule_; }
  int weight() const { return weight_; }
  int order() const { return order_; }

 private:
  FlexSpecification(FlexRule rule, int order, int weight);

  FlexRule rule_;
  int order_ = 1;
  int weight_ = 0;
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_FLEX_LAYOUT_TYPES_H_
