// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_item_view.h"

#include "ash/shell.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_preview_view.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// The min and max width for preview size are in relation to the fixed height.
constexpr int kMinPreviewWidthDp =
    WindowCycleItemView::kFixedPreviewHeightDp / 2;
constexpr int kMaxPreviewWidthDp =
    WindowCycleItemView::kFixedPreviewHeightDp * 2;

}  // namespace

WindowCycleItemView::WindowCycleItemView(aura::Window* window)
    : WindowMiniView(window) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetNotifyEnterExitOnChild(true);

  // TODO(crbug.com/1218186): Remove this, this is in place temporarily to be
  // able to submit accessibility checks, but this focusable View needs to
  // add a name so that the screen reader knows what to announce.
  SetProperty(views::kSkipAccessibilityPaintChecks, true);
}

void WindowCycleItemView::ShowPreview() {
  DCHECK(!preview_view());

  UpdateIconView();
  SetShowPreview(/*show=*/true);
  UpdatePreviewRoundedCorners(/*show=*/true);
}

void WindowCycleItemView::OnMouseEntered(const ui::MouseEvent& event) {
  Shell::Get()->window_cycle_controller()->SetFocusedWindow(source_window());
}

bool WindowCycleItemView::OnMousePressed(const ui::MouseEvent& event) {
  Shell::Get()->window_cycle_controller()->SetFocusedWindow(source_window());
  Shell::Get()->window_cycle_controller()->CompleteCycling();
  return true;
}

gfx::Size WindowCycleItemView::GetPreviewViewSize() const {
  // When the preview is not shown, do an estimate of the expected size.
  // |this| will not be visible anyways, and will get corrected once
  // ShowPreview() is called.
  if (!preview_view()) {
    gfx::SizeF source_size(source_window()->bounds().size());
    // Windows may have no size in tests.
    if (source_size.IsEmpty())
      return gfx::Size();
    const float aspect_ratio = source_size.width() / source_size.height();
    return gfx::Size(kFixedPreviewHeightDp * aspect_ratio,
                     kFixedPreviewHeightDp);
  }

  // Returns the size for the preview view, scaled to fit within the max
  // bounds. Scaling is always 1:1 and we only scale down, never up.
  gfx::Size preview_pref_size = preview_view()->GetPreferredSize();
  if (preview_pref_size.width() > kMaxPreviewWidthDp ||
      preview_pref_size.height() > kFixedPreviewHeightDp) {
    const float scale = std::min(
        kMaxPreviewWidthDp / static_cast<float>(preview_pref_size.width()),
        kFixedPreviewHeightDp / static_cast<float>(preview_pref_size.height()));
    preview_pref_size =
        gfx::ScaleToFlooredSize(preview_pref_size, scale, scale);
  }

  return preview_pref_size;
}

void WindowCycleItemView::Layout() {
  WindowMiniView::Layout();

  if (!preview_view())
    return;

  // Show the backdrop if the preview view does not take up all the bounds
  // allocated for it.
  gfx::Rect preview_max_bounds = GetContentsBounds();
  preview_max_bounds.Subtract(GetHeaderBounds());
  const gfx::Rect preview_area_bounds = preview_view()->bounds();
  SetBackdropVisibility(preview_max_bounds.size() !=
                        preview_area_bounds.size());
}

gfx::Size WindowCycleItemView::CalculatePreferredSize() const {
  // Previews can range in width from half to double of
  // |kFixedPreviewHeightDp|. Padding will be added to the
  // sides to achieve this if the preview is too narrow.
  gfx::Size preview_size = GetPreviewViewSize();

  // All previews are the same height (this may add padding on top and
  // bottom).
  preview_size.set_height(kFixedPreviewHeightDp);

  // Previews should never be narrower than half or wider than double their
  // fixed height.
  preview_size.set_width(base::clamp(preview_size.width(), kMinPreviewWidthDp,
                                     kMaxPreviewWidthDp));

  const int margin = GetInsets().width();
  preview_size.Enlarge(margin, margin + WindowMiniView::kHeaderHeightDp);
  return preview_size;
}

BEGIN_METADATA(WindowCycleItemView, WindowMiniView)
END_METADATA

}  // namespace ash
