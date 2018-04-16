// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focus_ring.h"

#include "ui/gfx/canvas.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/style/platform_style.h"

namespace views {

namespace {

FocusRing* GetFocusRing(View* parent) {
  for (int i = 0; i < parent->child_count(); ++i) {
    if (parent->child_at(i)->GetClassName() == FocusRing::kViewClassName)
      return static_cast<FocusRing*>(parent->child_at(i));
  }
  return nullptr;
}

}  // namespace

const char FocusRing::kViewClassName[] = "FocusRing";

// static
FocusRing* FocusRing::Install(View* parent,
                              SkColor color,
                              float corner_radius) {
  FocusRing* ring = GetFocusRing(parent);
  if (!ring) {
    ring = new FocusRing(color, corner_radius);
    parent->AddChildView(ring);
  }
  ring->Layout();
  ring->SchedulePaint();
  return ring;
}

// static
FocusRing* FocusRing::Install(View* parent,
                              ui::NativeTheme::ColorId override_color_id) {
  SkColor ring_color = parent->GetNativeTheme()->GetSystemColor(
      override_color_id == ui::NativeTheme::kColorId_NumColors
          ? ui::NativeTheme::kColorId_FocusedBorderColor
          : override_color_id);
  FocusRing* ring = Install(parent, ring_color);
  DCHECK(ring);
  ring->override_color_id_ = override_color_id;
  if (!ring->view_observer_.IsObserving(parent))
    ring->view_observer_.Add(parent);
  return ring;
}

// static
void FocusRing::Uninstall(View* parent) {
  delete GetFocusRing(parent);
}

// static
void FocusRing::InitFocusRing(View* view) {
  // A layer is necessary to paint beyond the parent's bounds.
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
  // Don't allow the view to process events.
  view->set_can_process_events_within_subtree(false);
}

const char* FocusRing::GetClassName() const {
  return kViewClassName;
}

void FocusRing::Layout() {
  // The focus ring handles its own sizing, which is simply to fill the parent
  // and extend a little beyond its borders.
  gfx::Rect focus_bounds = parent()->GetLocalBounds();
  focus_bounds.Inset(gfx::Insets(PlatformStyle::kFocusHaloInset));
  SetBoundsRect(focus_bounds);
}

void FocusRing::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(SkColorSetA(color_, 0x66));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(PlatformStyle::kFocusHaloThickness);
  gfx::RectF rect(GetLocalBounds());
  rect.Inset(gfx::InsetsF(PlatformStyle::kFocusHaloThickness / 2.f));
  // The focus indicator should hug the normal border, when present (as in the
  // case of text buttons). Since it's drawn outside the parent view, increase
  // the rounding slightly by adding half the ring thickness.
  canvas->DrawRoundRect(
      rect, corner_radius_ + PlatformStyle::kFocusHaloThickness / 2.f, flags);
}

void FocusRing::OnViewNativeThemeChanged(View* observed_view) {
  if (override_color_id_) {
    color_ = observed_view->GetNativeTheme()->GetSystemColor(
        override_color_id_.value());
  }
}

FocusRing::FocusRing(SkColor color, float corner_radius)
    : color_(color), corner_radius_(corner_radius), view_observer_(this) {
  InitFocusRing(this);
}

FocusRing::~FocusRing() {}

}  // namespace views
