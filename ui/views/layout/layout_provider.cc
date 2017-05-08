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
      return gfx::Insets(kPanelVertMargin, kPanelHorizMargin);
    case InsetsMetric::INSETS_BUBBLE_TITLE:
      return gfx::Insets(kPanelVertMargin, kPanelHorizMargin, 0,
                         kPanelHorizMargin);
    case InsetsMetric::INSETS_DIALOG_BUTTON:
      return gfx::Insets(0, kButtonHEdgeMarginNew, kButtonVEdgeMarginNew,
                         kButtonHEdgeMarginNew);
    case InsetsMetric::INSETS_DIALOG_TITLE:
      return gfx::Insets(kPanelVertMargin, kButtonHEdgeMarginNew, 0,
                         kButtonHEdgeMarginNew);
    case InsetsMetric::INSETS_PANEL:
      return gfx::Insets(kPanelVertMargin, kButtonHEdgeMarginNew);
    case InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON:
      return gfx::Insets(kVectorButtonExtraTouchSize);
  }
  NOTREACHED();
  return gfx::Insets();
}

int LayoutProvider::GetDistanceMetric(int metric) const {
  DCHECK_GE(metric, VIEWS_INSETS_MAX);
  switch (metric) {
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
    case DistanceMetric::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH:
      return kDialogMinimumButtonWidth;
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
