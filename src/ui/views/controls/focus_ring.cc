// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focus_ring.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace views {

namespace {

bool IsPathUsable(const SkPath& path) {
  return !path.isEmpty() && (path.isRect(nullptr) || path.isOval(nullptr) ||
                             path.isRRect(nullptr));
}

ui::NativeTheme::ColorId ColorIdForValidity(bool valid) {
  return valid ? ui::NativeTheme::kColorId_FocusedBorderColor
               : ui::NativeTheme::kColorId_AlertSeverityHigh;
}

double GetCornerRadius() {
  double thickness = PlatformStyle::kFocusHaloThickness / 2.f;
  return FocusableBorder::kCornerRadiusDp + thickness;
}

SkPath GetHighlightPathInternal(const View* view) {
  HighlightPathGenerator* path_generator =
      view->GetProperty(kHighlightPathGeneratorKey);

  if (path_generator) {
    SkPath highlight_path = path_generator->GetHighlightPath(view);
    // The generated path might be empty or otherwise unusable. If that's the
    // case we should fall back on the default path.
    if (IsPathUsable(highlight_path))
      return highlight_path;
  }

  const double corner_radius = GetCornerRadius();
  return SkPath().addRRect(SkRRect::MakeRectXY(
      RectToSkRect(view->GetLocalBounds()), corner_radius, corner_radius));
}

}  // namespace

// static
FocusRing* FocusRing::Install(View* parent) {
  if (IsViewClass<Button>(parent)) {
    // Ensure we don't install dual focus rings on a button.
    Button* button = static_cast<Button*>(parent);
    if (button->GetInstallFocusRingOnFocus())
      button->SetInstallFocusRingOnFocus(false);
  }
  auto ring = base::WrapUnique<FocusRing>(new FocusRing());
  ring->InvalidateLayout();
  ring->SchedulePaint();
  return parent->AddChildView(std::move(ring));
}

FocusRing::~FocusRing() = default;

void FocusRing::SetPathGenerator(
    std::unique_ptr<HighlightPathGenerator> generator) {
  path_generator_ = std::move(generator);
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

void FocusRing::SetColor(base::Optional<SkColor> color) {
  color_ = color;
  SchedulePaint();
}

void FocusRing::Layout() {
  // The focus ring handles its own sizing, which is simply to fill the parent
  // and extend a little beyond its borders.
  gfx::Rect focus_bounds = parent()->GetLocalBounds();
  focus_bounds.Inset(gfx::Insets(PlatformStyle::kFocusHaloInset));
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

  cc::PaintFlags paint;
  paint.setAntiAlias(true);
  paint.setColor(color_.value_or(
      GetNativeTheme()->GetSystemColor(ColorIdForValidity(!invalid_))));
  paint.setStyle(cc::PaintFlags::kStroke_Style);
  paint.setStrokeWidth(PlatformStyle::kFocusHaloThickness);

  SkPath path;
  if (path_generator_)
    path = path_generator_->GetHighlightPath(parent());

  // If there's no path generator or the generated path is unusable, fall back
  // to the default.
  if (!IsPathUsable(path))
    path = GetHighlightPathInternal(parent());

  DCHECK(IsPathUsable(path));
  DCHECK_EQ(GetFlipCanvasOnPaintForRTLUI(),
            parent()->GetFlipCanvasOnPaintForRTLUI());
  SkRect bounds;
  SkRRect rbounds;
  if (path.isRect(&bounds)) {
    canvas->sk_canvas()->drawRRect(RingRectFromPathRect(bounds), paint);
  } else if (path.isOval(&bounds)) {
    gfx::RectF rect = gfx::SkRectToRectF(bounds);
    View::ConvertRectToTarget(parent(), this, &rect);
    canvas->sk_canvas()->drawRRect(SkRRect::MakeOval(gfx::RectFToSkRect(rect)),
                                   paint);
  } else if (path.isRRect(&rbounds)) {
    canvas->sk_canvas()->drawRRect(RingRectFromPathRect(rbounds), paint);
  }
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
  const double corner_radius = GetCornerRadius();
  return RingRectFromPathRect(
      SkRRect::MakeRectXY(rect, corner_radius, corner_radius));
}

SkRRect FocusRing::RingRectFromPathRect(const SkRRect& rrect) const {
  double thickness = PlatformStyle::kFocusHaloThickness / 2.f;
  gfx::RectF r = gfx::SkRectToRectF(rrect.rect());
  View::ConvertRectToTarget(parent(), this, &r);

  SkRRect skr =
      rrect.makeOffset(r.x() - rrect.rect().x(), r.y() - rrect.rect().y());

  // The focus indicator should hug the normal border, when present (as in the
  // case of text buttons). Since it's drawn outside the parent view, increase
  // the rounding slightly by adding half the ring thickness.
  skr.inset(PlatformStyle::kFocusHaloInset, PlatformStyle::kFocusHaloInset);
  skr.inset(thickness, thickness);

  return skr;
}

SkPath GetHighlightPath(const View* view) {
  SkPath path = GetHighlightPathInternal(view);
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
