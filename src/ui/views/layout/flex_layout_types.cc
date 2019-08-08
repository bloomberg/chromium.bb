// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/flex_layout_types.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace views {

namespace {

std::string OptionalToString(const base::Optional<int>& opt) {
  if (!opt.has_value())
    return "_";
  return base::StringPrintf("%d", opt.value());
}

// Default Flex Rules ----------------------------------------------------------

// Interpolates a size between minimum, preferred size, and upper bound based on
// sizing rules, returning the resulting ideal size.
int InterpolateSize(MinimumFlexSizeRule minimum_size_rule,
                    MaximumFlexSizeRule maximum_size_rule,
                    int minimum_size,
                    int preferred_size,
                    int available_size) {
  if (available_size < minimum_size) {
    switch (minimum_size_rule) {
      case MinimumFlexSizeRule::kScaleToZero:
        return available_size;
      case MinimumFlexSizeRule::kPreferred:
        return preferred_size;
      case MinimumFlexSizeRule::kScaleToMinimum:
      case MinimumFlexSizeRule::kPreferredSnapToMinimum:
        return minimum_size;
      case MinimumFlexSizeRule::kScaleToMinimumSnapToZero:
      case MinimumFlexSizeRule::kPreferredSnapToZero:
        return 0;
    }
  } else if (available_size < preferred_size) {
    switch (minimum_size_rule) {
      case MinimumFlexSizeRule::kPreferred:
        return preferred_size;
      case MinimumFlexSizeRule::kScaleToZero:
      case MinimumFlexSizeRule::kScaleToMinimum:
      case MinimumFlexSizeRule::kScaleToMinimumSnapToZero:
        return available_size;
      case MinimumFlexSizeRule::kPreferredSnapToMinimum:
        return minimum_size;
      case MinimumFlexSizeRule::kPreferredSnapToZero:
        return 0;
    }
  } else {
    switch (maximum_size_rule) {
      case MaximumFlexSizeRule::kPreferred:
        return preferred_size;
      case MaximumFlexSizeRule::kUnbounded:
        return available_size;
    }
  }
}

gfx::Size GetPreferredSize(MinimumFlexSizeRule minimum_size_rule,
                           MaximumFlexSizeRule maximum_size_rule,
                           const views::View* view,
                           const views::SizeBounds& maximum_size) {
  gfx::Size min = view->GetMinimumSize();
  gfx::Size preferred = view->GetPreferredSize();

  int width, height;

  if (!maximum_size.width()) {
    // Not having a maximum size is different from having a large available
    // size; a view can't grow infinitely, so we go with its preferred size.
    width = preferred.width();
  } else {
    width = InterpolateSize(minimum_size_rule, maximum_size_rule, min.width(),
                            preferred.width(), *maximum_size.width());
  }

  // Allow views that need to grow vertically when they're compressed
  // horizontally to do so.
  //
  // If we just went with GetHeightForWidth() we would have situations where an
  // empty text control wanted no (or very little) height which could cause a
  // layout to shrink vertically; we will always try to allocate at least the
  // view's reported preferred height.
  //
  // Note that this is an adjustment made for practical considerations, and may
  // not be "correct" in some absolute sense. Let's revisit at some point.
  const int preferred_height =
      std::max(preferred.height(), view->GetHeightForWidth(width));

  if (!maximum_size.height()) {
    // Not having a maximum size is different from having a large available
    // size; a view can't grow infinitely, so we go with its preferred size.
    height = preferred_height;
  } else {
    height = InterpolateSize(minimum_size_rule, maximum_size_rule, min.height(),
                             preferred_height, *maximum_size.height());
  }

  return gfx::Size(width, height);
}

FlexRule GetDefaultFlexRule(
    MinimumFlexSizeRule minimum_size_rule = MinimumFlexSizeRule::kPreferred,
    MaximumFlexSizeRule maximum_size_rule = MaximumFlexSizeRule::kPreferred) {
  return base::BindRepeating(&GetPreferredSize, minimum_size_rule,
                             maximum_size_rule);
}

}  // namespace

// SizeBounds ------------------------------------------------------------------

SizeBounds::SizeBounds() = default;

SizeBounds::SizeBounds(const base::Optional<int>& width,
                       const base::Optional<int>& height)
    : width_(width), height_(height) {}

SizeBounds::SizeBounds(const SizeBounds& other)
    : width_(other.width()), height_(other.height()) {}

SizeBounds::SizeBounds(const gfx::Size& other)
    : width_(other.width()), height_(other.height()) {}

void SizeBounds::Enlarge(int width, int height) {
  if (width_)
    width_ = std::max(0, *width_ + width);
  if (height_)
    height_ = std::max(0, *height_ + height);
}

bool SizeBounds::operator==(const SizeBounds& other) const {
  return width_ == other.width_ && height_ == other.height_;
}

bool SizeBounds::operator!=(const SizeBounds& other) const {
  return !(*this == other);
}

bool SizeBounds::operator<(const SizeBounds& other) const {
  return std::tie(height_, width_) < std::tie(other.height_, other.width_);
}

std::string SizeBounds::ToString() const {
  return base::StringPrintf("%s x %s", OptionalToString(width()).c_str(),
                            OptionalToString(height()).c_str());
}

// FlexSpecification -----------------------------------------------------------

FlexSpecification::FlexSpecification() : rule_(GetDefaultFlexRule()) {}

FlexSpecification FlexSpecification::ForCustomRule(FlexRule rule) {
  return FlexSpecification(std::move(rule), 1, 1);
}

FlexSpecification FlexSpecification::ForSizeRule(
    MinimumFlexSizeRule minimum_size_rule,
    MaximumFlexSizeRule maximum_size_rule) {
  return FlexSpecification(
      GetDefaultFlexRule(minimum_size_rule, maximum_size_rule), 1, 1);
}

FlexSpecification::FlexSpecification(FlexRule rule, int order, int weight)
    : rule_(std::move(rule)), order_(order), weight_(weight) {}

FlexSpecification::FlexSpecification(const FlexSpecification& other) = default;

FlexSpecification& FlexSpecification::operator=(
    const FlexSpecification& other) = default;

FlexSpecification::~FlexSpecification() = default;

FlexSpecification FlexSpecification::WithWeight(int weight) const {
  DCHECK_GE(weight, 0);
  return FlexSpecification(rule_, order_, weight);
}

FlexSpecification FlexSpecification::WithOrder(int order) const {
  DCHECK_GE(order, 1);
  return FlexSpecification(rule_, order, weight_);
}

}  // namespace views
