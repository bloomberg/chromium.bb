// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_util.h"

#include <memory>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

namespace ash {
namespace holding_space_util {

namespace {

// CirclePainter ---------------------------------------------------------------

class CirclePainter : public views::Painter {
 public:
  CirclePainter(SkColor color, size_t fixed_size)
      : color_(color), fixed_size_(fixed_size) {}

  CirclePainter(SkColor color, const gfx::InsetsF& insets)
      : color_(color), insets_(insets) {}

  CirclePainter(const CirclePainter&) = delete;
  CirclePainter& operator=(const CirclePainter&) = delete;
  ~CirclePainter() override = default;

 private:
  // views::Painter:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    gfx::RectF bounds{gfx::SizeF(size)};

    if (insets_.has_value())
      bounds.Inset(insets_.value());

    const float radius =
        fixed_size_.has_value()
            ? fixed_size_.value() / 2.f
            : std::min(bounds.size().width(), bounds.size().height()) / 2.f;

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_);

    canvas->DrawCircle(bounds.CenterPoint(), radius, flags);
  }

  const SkColor color_;
  const absl::optional<size_t> fixed_size_;
  const absl::optional<gfx::InsetsF> insets_;
};

// Helpers ---------------------------------------------------------------------

// Creates a `ui::LayerAnimationSequence` for the specified `element` with
// optional `delay`, observed by the specified `observer`.
std::unique_ptr<ui::LayerAnimationSequence> CreateObservedSequence(
    std::unique_ptr<ui::LayerAnimationElement> element,
    base::TimeDelta delay,
    ui::LayerAnimationObserver* observer) {
  auto sequence = std::make_unique<ui::LayerAnimationSequence>();
  if (!delay.is_zero()) {
    sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
        element->properties(), delay));
  }
  sequence->AddElement(std::move(element));
  sequence->AddObserver(observer);
  return sequence;
}

// Animates the specified `view` to a target `opacity` with the specified
// `duration` and optional `delay`, associating `observer` with the created
// animation sequences.
void AnimateTo(views::View* view,
               float opacity,
               base::TimeDelta duration,
               base::TimeDelta delay,
               ui::LayerAnimationObserver* observer) {
  // Opacity animation.
  auto opacity_element =
      ui::LayerAnimationElement::CreateOpacityElement(opacity, duration);
  opacity_element->set_tween_type(gfx::Tween::Type::LINEAR);

  // Note that the `ui::LayerAnimator` takes ownership of any animation
  // sequences so they need to be released.
  view->layer()->GetAnimator()->StartAnimation(
      CreateObservedSequence(std::move(opacity_element), delay, observer)
          .release());
}

}  // namespace

// Animates in the specified `view` with the specified `duration` and optional
// `delay`, associating `observer` with the created animation sequences.
void AnimateIn(views::View* view,
               base::TimeDelta duration,
               base::TimeDelta delay,
               ui::LayerAnimationObserver* observer) {
  view->layer()->SetOpacity(0.f);
  AnimateTo(view, /*opacity=*/1.f, duration, delay, observer);
}

// Animates out the specified `view` with the specified `duration, associating
// `observer` with the created animation sequences.
void AnimateOut(views::View* view,
                base::TimeDelta duration,
                ui::LayerAnimationObserver* observer) {
  AnimateTo(view, /*opacity=*/0.f, duration, /*delay=*/base::TimeDelta(),
            observer);
}

std::unique_ptr<views::Background> CreateCircleBackground(SkColor color,
                                                          size_t fixed_size) {
  return views::CreateBackgroundFromPainter(
      std::make_unique<CirclePainter>(color, fixed_size));
}

std::unique_ptr<views::Background> CreateCircleBackground(
    SkColor color,
    const gfx::InsetsF& insets) {
  return views::CreateBackgroundFromPainter(
      std::make_unique<CirclePainter>(color, insets));
}

}  // namespace holding_space_util
}  // namespace ash
