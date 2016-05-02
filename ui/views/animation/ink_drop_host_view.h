// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_
#define UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_

#include <memory>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/view.h"

namespace views {

class InkDropRipple;
class InkDropHover;

// A view that provides InkDropHost functionality.
class VIEWS_EXPORT InkDropHostView : public views::View, public InkDropHost {
 public:
  InkDropHostView();
  ~InkDropHostView() override;

  // Overridden from views::InkDropHost:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<InkDropHover> CreateInkDropHover() const override;

  void set_ink_drop_size(const gfx::Size& size) { ink_drop_size_ = size; }

 protected:
  static const int kInkDropSmallCornerRadius;

  // Overrideable methods to allow views to provide minor tweaks to the default
  // ink drop.
  virtual gfx::Point GetInkDropCenter() const;
  virtual SkColor GetInkDropBaseColor() const;

 private:
  gfx::Size ink_drop_size_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHostView);
};
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_
