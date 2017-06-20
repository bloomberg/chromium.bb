// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_provider.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/views_delegate.h"

namespace views {

namespace {

LayoutProvider* g_layout_delegate = nullptr;

}  // namespace

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
      return gfx::Insets(13, 13);
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
      return gfx::Insets(13, 20);
    case InsetsMetric::INSETS_DIALOG_TITLE: {
      const gfx::Insets dialog_contents =
          GetInsetsMetric(INSETS_DIALOG_CONTENTS);
      return gfx::Insets(dialog_contents.top(), dialog_contents.left(), 0,
                         dialog_contents.right());
    }
    case InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON:
      return gfx::Insets(4);
  }
  NOTREACHED();
  return gfx::Insets();
}

int LayoutProvider::GetDistanceMetric(int metric) const {
  DCHECK_GE(metric, VIEWS_INSETS_MAX);
  switch (metric) {
    case DistanceMetric::DISTANCE_BUTTON_HORIZONTAL_PADDING:
      return 16;
    case DistanceMetric::DISTANCE_BUTTON_MAX_LINKABLE_WIDTH:
      return 0;
    case DistanceMetric::DISTANCE_CLOSE_BUTTON_MARGIN:
      return 7;
    case DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL:
      return 6;
    case DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL:
      return 8;
    case DistanceMetric::DISTANCE_BUBBLE_BUTTON_TOP_MARGIN:
    case DistanceMetric::DISTANCE_RELATED_CONTROL_VERTICAL:
      return 8;
    case DISTANCE_DIALOG_BUTTON_BOTTOM_MARGIN:
      return 20;
    case DistanceMetric::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH:
      return 75;
    case DISTANCE_UNRELATED_CONTROL_VERTICAL:
      return 20;
  }
  NOTREACHED();
  return 0;
}

const TypographyProvider& LayoutProvider::GetTypographyProvider() const {
  return typography_provider_;
}

int LayoutProvider::GetSnappedDialogWidth(int min_width) const {
  // This is an arbitrary value, but it's a good arbitrary value. Some dialogs
  // have very small widths for their contents views, which causes ugly
  // title-wrapping where a two-word title is split across multiple lines or
  // similar. To prevent that, forbid any snappable dialog from being narrower
  // than this value. In principle it's possible to factor in the title width
  // here, but it is not really worth the complexity.
  return std::max(min_width, 320);
}

}  // namespace views
