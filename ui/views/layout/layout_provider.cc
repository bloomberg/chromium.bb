// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_provider.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/views_delegate.h"

namespace views {

namespace {
LayoutProvider* g_layout_delegate = nullptr;
}

LayoutProvider::LayoutProvider() {
  g_layout_delegate = this;
}

LayoutProvider::~LayoutProvider() {
  if (this == g_layout_delegate)
    g_layout_delegate = nullptr;
}

// static
LayoutProvider* LayoutProvider::Get() {
  return g_layout_delegate;
}

gfx::Insets LayoutProvider::GetInsetsMetric(int metric) const {
  DCHECK_LT(metric, VIEWS_INSETS_MAX);
  switch (metric) {
    case InsetsMetric::INSETS_BUBBLE_CONTENTS:
      return gfx::Insets(
          GetDistanceMetric(DISTANCE_BUBBLE_CONTENTS_VERTICAL_MARGIN),
          GetDistanceMetric(DISTANCE_BUBBLE_CONTENTS_HORIZONTAL_MARGIN));
    case InsetsMetric::INSETS_BUBBLE_TITLE: {
      const gfx::Insets bubble_contents =
          GetInsetsMetric(INSETS_BUBBLE_CONTENTS);
      return gfx::Insets(bubble_contents.top(), bubble_contents.left(), 0,
                         bubble_contents.right());
    }
    case InsetsMetric::INSETS_DIALOG_BUTTON_ROW: {
      const gfx::Insets dialog_contents =
          GetInsetsMetric(INSETS_DIALOG_CONTENTS);
      return gfx::Insets(
          0, dialog_contents.left(),
          GetDistanceMetric(DISTANCE_DIALOG_BUTTON_BOTTOM_MARGIN),
          dialog_contents.right());
    }
    case InsetsMetric::INSETS_DIALOG_CONTENTS:
      return gfx::Insets(
          GetDistanceMetric(DISTANCE_DIALOG_CONTENTS_VERTICAL_MARGIN),
          GetDistanceMetric(DISTANCE_DIALOG_CONTENTS_HORIZONTAL_MARGIN));
    case InsetsMetric::INSETS_DIALOG_TITLE: {
      const gfx::Insets dialog_contents =
          GetInsetsMetric(INSETS_DIALOG_CONTENTS);
      return gfx::Insets(dialog_contents.top(), dialog_contents.left(), 0,
                         dialog_contents.right());
    }
    case InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON:
      return gfx::Insets(kVectorButtonExtraTouchSize);
  }
  NOTREACHED();
  return gfx::Insets();
}

int LayoutProvider::GetDistanceMetric(int metric) const {
  DCHECK_GE(metric, VIEWS_INSETS_MAX);
  switch (metric) {
    case DISTANCE_BUBBLE_CONTENTS_HORIZONTAL_MARGIN:
      return kPanelHorizMargin;
    case DISTANCE_BUBBLE_CONTENTS_VERTICAL_MARGIN:
    case DISTANCE_DIALOG_CONTENTS_VERTICAL_MARGIN:
      return kPanelVertMargin;
    case DistanceMetric::DISTANCE_BUTTON_HORIZONTAL_PADDING:
      return kButtonHorizontalPadding;
    case DistanceMetric::DISTANCE_BUTTON_MAX_LINKABLE_WIDTH:
      return 0;
    case DistanceMetric::DISTANCE_CLOSE_BUTTON_MARGIN:
      return kCloseButtonMargin;
    case DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL:
      return kRelatedButtonHSpacing;
    case DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL:
      return kRelatedControlHorizontalSpacing;
    case DistanceMetric::DISTANCE_RELATED_CONTROL_VERTICAL:
      return kRelatedControlVerticalSpacing;
    case DISTANCE_DIALOG_BUTTON_BOTTOM_MARGIN:
      return views::kButtonVEdgeMarginNew;
    case DistanceMetric::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH:
      return kDialogMinimumButtonWidth;
    case DISTANCE_DIALOG_CONTENTS_HORIZONTAL_MARGIN:
      return kButtonHEdgeMarginNew;
  }
  NOTREACHED();
  return 0;
}

const TypographyProvider& LayoutProvider::GetTypographyProvider() const {
  return typography_provider_;
}

int LayoutProvider::GetSnappedDialogWidth(int min_width) const {
  return min_width;
}

}  // namespace views
