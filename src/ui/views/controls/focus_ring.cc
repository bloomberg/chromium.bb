// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focus_ring.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(views::FocusRing*)

namespace views {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kDrawFocusRingBackgroundOutline, false)

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(FocusRing*, kFocusRingIdKey, nullptr)

constexpr float kOutlineThickness = 1.0f;

bool IsPathUsable(const SkPath& path) {
  return !path.isEmpty() && (path.isRect(nullptr) || path.isOval(nullptr) ||
                             path.isRRect(nullptr));
}

SkColor GetColor(View* focus_ring, bool valid) {
  if (!valid) {
    return focus_ring->GetColorProvider()->GetColor(
        ui::kColorAlertHighSeverity);
  }
  return GetCascadingAccentColor(focus_ring);
}

double GetCornerRadius(float halo_thickness) {
  const double thickness = halo_thickness / 2.f;
  return FocusableBorder::kCornerRadiusDp + thickness;
}

SkPath GetHighlightPathInternal(const View* view, float halo_thickness) {
  HighlightPathGenerator* path_generator =
      view->GetProperty(kHighlightPathGeneratorKey);

  if (path_generator) {
    SkPath highlight_path = path_generator->GetHighlightPath(view);
    // The generated path might be empty or otherwise unusable. If that's the
    // case we should fall back on the default path.
    if (IsPathUsable(highlight_path))
      return highlight_path;
  }

  gfx::Rect client_rect = view->GetLocalBounds();
  const double corner_radius = GetCornerRadius(halo_thickness);
  // Make sure the path is large enough to contain the corners. This covers
  // narrow views and the case where view->GetLocalBounds() are empty. Doing so
  // prevents DCHECK(IsPathUsable(path)) from failing in GetRingRoundRect()
  // because the resulting path is empty.
  if (client_rect.width() < 2 * corner_radius ||
      client_rect.height() < 2 * corner_radius) {
    client_rect.Outset(corner_radius);
  }
  return SkPath().addRRect(SkRRect::MakeRectXY(RectToSkRect(client_rect),
                                               corner_radius, corner_radius));
}

}  // namespace

constexpr float FocusRing::kDefaultHaloThickness;
constexpr float FocusRing::kDefaultHaloInset;

// static
void FocusRing::Install(View* host) {
  FocusRing::Remove(host);
  auto ring = base::WrapUnique<FocusRing>(new FocusRing());
  ring->InvalidateLayout();
  ring->SchedulePaint();
  host->SetProperty(kFocusRingIdKey, host->AddChildView(std::move(ring)));
}

FocusRing* FocusRing::Get(View* host) {
  return host->GetProperty(kFocusRingIdKey);
}

const FocusRing* FocusRing::Get(const View* host) {
  return host->GetProperty(kFocusRingIdKey);
}

void FocusRing::Remove(View* host) {
  // Note that the FocusRing is owned by the View hierarchy, so we can't just
  // clear the key.
  FocusRing* const focus_ring = FocusRing::Get(host);
  if (!focus_ring)
    return;
  host->RemoveChildViewT(focus_ring);
  host->ClearProperty(kFocusRingIdKey);
}

FocusRing::~FocusRing() = default;

void FocusRing::SetPathGenerator(
    std::unique_ptr<HighlightPathGenerator> generator) {
  path_generator_ = std::move(generator);
  InvalidateLayout();
  SchedulePaint();
}

void FocusRing::SetInvalid(bool invalid) {
  invalid_ = invalid;
  SchedulePaint();
}

void FocusRing::SetHasFocusPredicate(const ViewPredicate& predicate) {
  has_focus_predicate_ = predicate;
  RefreshLayer();
}

void FocusRing::SetColor(absl::optional<SkColor> color) {
  color_ = color;
  SchedulePaint();
}

void FocusRing::SetHaloThickness(float halo_thickness) {
  halo_thickness_ = halo_thickness;
  SchedulePaint();
}

void FocusRing::SetHaloInset(float halo_inset) {
  halo_inset_ = halo_inset;
  SchedulePaint();
}

void FocusRing::Layout() {
  // The focus ring handles its own sizing, which is simply to fill the parent
  // and extend a little beyond its borders.
  gfx::Rect focus_bounds = parent()->GetLocalBounds();

  // Make sure the focus-ring path fits.
  // TODO(pbos): Chase down use cases where this path is not in a usable state
  // by the time layout happens. This may be due to synchronous Layout() calls.
  const SkPath path = GetPath();
  if (IsPathUsable(path)) {
    const gfx::Rect path_bounds =
        gfx::ToEnclosingRect(gfx::SkRectToRectF(path.getBounds()));
    const gfx::Rect expanded_bounds =
        gfx::UnionRects(focus_bounds, path_bounds);
    // These insets are how much we need to inset `focus_bounds` to enclose the
    // path as well. They'll be either zero or negative (we're effectively
    // outsetting).
    gfx::Insets expansion_insets = focus_bounds.InsetsFrom(expanded_bounds);
    // Make sure we extend the focus-ring bounds symmetrically on the X axis to
    // retain the shared center point with parent(). This is required for canvas
    // flipping to position the focus-ring path correctly after the RTL flip.
    const int min_x_inset =
        std::min(expansion_insets.left(), expansion_insets.right());
    expansion_insets.set_left(min_x_inset);
    expansion_insets.set_right(min_x_inset);
    focus_bounds.Inset(expansion_insets);
  }

  focus_bounds.Inset(gfx::Insets(halo_inset_));

  if (parent()->GetProperty(kDrawFocusRingBackgroundOutline))
    focus_bounds.Inset(gfx::Insets(-2 * kOutlineThickness));

  SetBoundsRect(focus_bounds);

  // Need to match canvas direction with the parent. This is required to ensure
  // asymmetric focus ring shapes match their respective buttons in RTL mode.
  SetFlipCanvasOnPaintForRTLUI(parent()->GetFlipCanvasOnPaintForRTLUI());
}

void FocusRing::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.child != this)
    return;

  if (details.is_add) {
    // Need to start observing the parent.
    view_observation_.Observe(details.parent);
    RefreshLayer();
  } else if (view_observation_.IsObservingSource(details.parent)) {
    // This view is being removed from its parent. It needs to remove itself
    // from its parent's observer list in the case where the FocusView is
    // removed from its parent but not deleted.
    view_observation_.Reset();
  }
}

void FocusRing::OnPaint(gfx::Canvas* canvas) {
  // TODO(pbos): Reevaluate if this can turn into a DCHECK, e.g. we should
  // never paint if there's no parent focus.
  if (has_focus_predicate_) {
    if (!(*has_focus_predicate_)(parent()))
      return;
  } else if (!parent()->HasFocus()) {
    return;
  }

  const SkRRect ring_rect = GetRingRoundRect();
  cc::PaintFlags paint;
  paint.setAntiAlias(true);
  paint.setStyle(cc::PaintFlags::kStroke_Style);

  if (parent()->GetProperty(kDrawFocusRingBackgroundOutline)) {
    // Draw with full stroke width + 2x outline thickness to effectively paint
    // the outline thickness on both sides of the FocusRing.
    paint.setStrokeWidth(halo_thickness_ + 2 * kOutlineThickness);
    paint.setColor(GetCascadingBackgroundColor(this));
    canvas->sk_canvas()->drawRRect(ring_rect, paint);
  }

  paint.setColor(color_.value_or(GetColor(this, !invalid_)));
  paint.setStrokeWidth(halo_thickness_);
  canvas->sk_canvas()->drawRRect(ring_rect, paint);
}

SkRRect FocusRing::GetRingRoundRect() const {
  const SkPath path = GetPath();

  DCHECK(IsPathUsable(path));
  DCHECK_EQ(GetFlipCanvasOnPaintForRTLUI(),
            parent()->GetFlipCanvasOnPaintForRTLUI());

  SkRect bounds;
  SkRRect rbounds;
  if (path.isRect(&bounds))
    return RingRectFromPathRect(bounds);

  if (path.isOval(&bounds)) {
    gfx::RectF rect = gfx::SkRectToRectF(bounds);
    View::ConvertRectToTarget(parent(), this, &rect);
    return SkRRect::MakeOval(gfx::RectFToSkRect(rect));
  }

  if (path.isRRect(&rbounds))
    return RingRectFromPathRect(rbounds);

  NOTREACHED();
  return SkRRect();
}

void FocusRing::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Mark the focus ring in the accessibility tree as invisible so that it will
  // not be accessed by assistive technologies.
  node_data->AddState(ax::mojom::State::kInvisible);
}

void FocusRing::OnViewFocused(View* view) {
  RefreshLayer();
}

void FocusRing::OnViewBlurred(View* view) {
  RefreshLayer();
}

FocusRing::FocusRing() {
  // Don't allow the view to process events.
  SetCanProcessEventsWithinSubtree(false);
}

SkPath FocusRing::GetPath() const {
  SkPath path;
  if (path_generator_) {
    path = path_generator_->GetHighlightPath(parent());
    if (IsPathUsable(path))
      return path;
  }

  // If there's no path generator or the generated path is unusable, fall back
  // to the default.
  return GetHighlightPathInternal(parent(), halo_thickness_);
}

void FocusRing::RefreshLayer() {
  // TODO(pbos): This always keeps the layer alive if |has_focus_predicate_| is
  // set. This is done because we're not notified when the predicate might
  // return a different result and there are call sites that call SchedulePaint
  // on FocusRings and expect that to be sufficient.
  // The cleanup would be to always call has_focus_predicate_ here and make sure
  // that RefreshLayer gets called somehow whenever |has_focused_predicate_|
  // returns a new value.
  const bool should_paint =
      has_focus_predicate_.has_value() || (parent() && parent()->HasFocus());
  SetVisible(should_paint);
  if (should_paint) {
    // A layer is necessary to paint beyond the parent's bounds.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  } else {
    DestroyLayer();
  }
}

SkRRect FocusRing::RingRectFromPathRect(const SkRect& rect) const {
  const double corner_radius = GetCornerRadius(halo_thickness_);
  return RingRectFromPathRect(
      SkRRect::MakeRectXY(rect, corner_radius, corner_radius));
}

SkRRect FocusRing::RingRectFromPathRect(const SkRRect& rrect) const {
  const double thickness = halo_thickness_ / 2.f;
  gfx::RectF r = gfx::SkRectToRectF(rrect.rect());
  View::ConvertRectToTarget(parent(), this, &r);

  SkRRect skr =
      rrect.makeOffset(r.x() - rrect.rect().x(), r.y() - rrect.rect().y());

  // The focus indicator should hug the normal border, when present (as in the
  // case of text buttons). Since it's drawn outside the parent view, increase
  // the rounding slightly by adding half the ring thickness.
  skr.inset(halo_inset_, halo_inset_);
  skr.inset(thickness, thickness);

  return skr;
}

SkPath GetHighlightPath(const View* view, float halo_thickness) {
  SkPath path = GetHighlightPathInternal(view, halo_thickness);
  if (view->GetFlipCanvasOnPaintForRTLUI() && base::i18n::IsRTL()) {
    gfx::Point center = view->GetLocalBounds().CenterPoint();
    SkMatrix flip;
    flip.setScale(-1, 1, center.x(), center.y());
    path.transform(flip);
  }
  return path;
}

BEGIN_METADATA(FocusRing, View)
END_METADATA

}  // namespace views
