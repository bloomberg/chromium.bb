// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr SkColor kInstallableInkDropPlaceholderColor =
    SkColorSetA(SK_ColorBLACK, 0.08 * SK_AlphaOPAQUE);
}  // namespace

namespace views {

const base::Feature kInstallableInkDropFeature{
    "InstallableInkDrop", base::FEATURE_DISABLED_BY_DEFAULT};

InstallableInkDrop::InstallableInkDrop(View* view)
    : view_(view),
      event_handler_(view_, this),
      layer_(std::make_unique<ui::Layer>()) {
  // Catch if |view_| is destroyed out from under us.
  if (DCHECK_IS_ON())
    view_->AddObserver(this);

  layer_->set_delegate(this);
  layer_->SetFillsBoundsOpaquely(false);
  layer_->SetFillsBoundsCompletely(false);
  view_->AddLayerBeneathView(layer_.get());

  // AddLayerBeneathView() changes the location of |layer_| so this must be done
  // after.
  layer_->SetBounds(gfx::Rect(view_->size()) +
                    layer_->bounds().OffsetFromOrigin());
  layer_->SchedulePaint(gfx::Rect(layer_->size()));
}

InstallableInkDrop::~InstallableInkDrop() {
  view_->RemoveLayerBeneathView(layer_.get());
  if (DCHECK_IS_ON())
    view_->RemoveObserver(this);
}

void InstallableInkDrop::HostSizeChanged(const gfx::Size& new_size) {
  layer_->SetBounds(gfx::Rect(new_size) + layer_->bounds().OffsetFromOrigin());
  layer_->SchedulePaint(gfx::Rect(layer_->size()));
}

InkDropState InstallableInkDrop::GetTargetInkDropState() const {
  return current_state_;
}

void InstallableInkDrop::AnimateToState(InkDropState ink_drop_state) {
  current_state_ = ink_drop_state;
}

void InstallableInkDrop::SetHoverHighlightFadeDurationMs(int duration_ms) {
  NOTREACHED();
}

void InstallableInkDrop::UseDefaultHoverHighlightFadeDuration() {
  NOTREACHED();
}

void InstallableInkDrop::SnapToActivated() {}

void InstallableInkDrop::SnapToHidden() {}

void InstallableInkDrop::SetHovered(bool is_hovered) {}

void InstallableInkDrop::SetFocused(bool is_focused) {}

bool InstallableInkDrop::IsHighlightFadingInOrVisible() const {
  return false;
}

void InstallableInkDrop::SetShowHighlightOnHover(bool show_highlight_on_hover) {
  NOTREACHED();
}

void InstallableInkDrop::SetShowHighlightOnFocus(bool show_highlight_on_focus) {
  NOTREACHED();
}

InkDrop* InstallableInkDrop::GetInkDrop() {
  return this;
}

bool InstallableInkDrop::HasInkDrop() const {
  return true;
}

bool InstallableInkDrop::SupportsGestureEvents() const {
  return true;
}

void InstallableInkDrop::OnViewIsDeleting(View* observed_view) {
  DCHECK_EQ(view_, observed_view);
  NOTREACHED() << "|this| needs to outlive the view it's installed on";
}

void InstallableInkDrop::OnPaintLayer(const ui::PaintContext& context) {
  DCHECK_EQ(view_->size(), layer_->size());

  ui::PaintRecorder paint_recorder(context, layer_->size());
  gfx::Canvas* canvas = paint_recorder.canvas();

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(kInstallableInkDropPlaceholderColor);

  canvas->DrawPath(GetHighlightPathForView(view_), flags);
}

void InstallableInkDrop::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

// static
SkPath InstallableInkDrop::GetHighlightPathForView(const View* view) {
  // Use the provided highlight path if there is one.
  const SkPath* const path_property = view->GetProperty(kHighlightPathKey);
  if (path_property)
    return *path_property;

  // Otherwise, construct a default path. This will be pill shaped, or circular
  // when |view| is square.
  SkPath path;

  const float radius =
      std::min(view->size().width(), view->size().height()) / 2.0f;
  path.addRoundRect(gfx::RectToSkRect(view->GetLocalBounds()), radius, radius);

  return path;
}

}  // namespace views
