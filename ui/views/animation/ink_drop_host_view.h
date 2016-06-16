// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_
#define UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_

#include <memory>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/view.h"

namespace views {

namespace test {
class InkDropHostViewTestApi;
}  // namespace test

class InkDropRipple;
class InkDropHighlight;

// A view that provides InkDropHost functionality.
class VIEWS_EXPORT InkDropHostView : public View, public InkDropHost {
 public:
  // Overridden from InkDropHost:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<InkDropHighlight> CreateInkDropHighlight() const override;

  void set_ink_drop_size(const gfx::Size& size) { ink_drop_size_ = size; }

 protected:
  InkDropHostView();
  ~InkDropHostView() override;

  static const int kInkDropSmallCornerRadius;

  // View:
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Overrideable methods to allow views to provide minor tweaks to the default
  // ink drop.
  virtual gfx::Point GetInkDropCenter() const;
  virtual SkColor GetInkDropBaseColor() const;

  // Should return true if the ink drop is also used to depict focus.
  virtual bool ShouldShowInkDropForFocus() const;

  InkDrop* ink_drop() { return ink_drop_.get(); }

  // Toggle to enable/disable an InkDrop on this View.  Descendants can override
  // CreateInkDropHighlight() and CreateInkDropRipple() to change the look/feel
  // of the InkDrop.
  void SetHasInkDrop(bool has_an_ink_drop);

  // Animates |ink_drop_| to the desired |ink_drop_state|.
  //
  // TODO(bruthig): Enhance to accept a ui::Event parameter so InkDrops can be
  // centered (see https://crbug.com/597273) and disabled for touch events on
  // Windows (see https://crbug.com/595315).
  void AnimateInkDrop(InkDropState ink_drop_state);

 private:
  class InkDropGestureHandler;
  friend class test::InkDropHostViewTestApi;

  std::unique_ptr<InkDrop> ink_drop_;

  // Intentionally declared after |ink_drop_| so that it doesn't access a
  // destroyed |ink_drop_| during destruction.
  std::unique_ptr<InkDropGestureHandler> gesture_handler_;

  gfx::Size ink_drop_size_;

  // Determines whether the view was already painting to layer before adding ink
  // drop layer.
  bool old_paint_to_layer_;

  bool destroying_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHostView);
};
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_
