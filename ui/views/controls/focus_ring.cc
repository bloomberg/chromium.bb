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
    ring = new FocusRing(parent, color, corner_radius);
    parent->AddChildView(ring);
  } else {
    // Update color and corner radius.
    ring->color_ = color;
    ring->corner_radius_ = corner_radius;
  }
  ring->Layout();
  ring->SchedulePaint();
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
  flags.setColor(SkColorSetA(GetColor(), 0x66));
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

FocusRing::FocusRing(View* parent, SkColor color, float corner_radius)
    : view_(parent), color_(color), corner_radius_(corner_radius) {
  InitFocusRing(this);
}

FocusRing::~FocusRing() {}

SkColor FocusRing::GetColor() const {
  if (color_ != kInvalidColor)
    return color_;
  return view_->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_FocusedBorderColor);
}

}  // namespace views
