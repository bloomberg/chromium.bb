// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_
#define UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_

#include <memory>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_delegate.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/view.h"

namespace views {

class InkDropRipple;
class InkDropHover;

// A view that provides InkDropHost functionality.
class VIEWS_EXPORT InkDropHostView : public View, public InkDropHost {
 public:
  // Overridden from views::InkDropHost:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<InkDropHover> CreateInkDropHover() const override;

  void set_ink_drop_size(const gfx::Size& size) { ink_drop_size_ = size; }

  InkDropDelegate* ink_drop_delegate() { return ink_drop_delegate_.get(); }

 protected:
  InkDropHostView();
  ~InkDropHostView() override;

  static const int kInkDropSmallCornerRadius;

  // View
  void OnFocus() override;
  void OnBlur() override;

  // Overrideable methods to allow views to provide minor tweaks to the default
  // ink drop.
  virtual gfx::Point GetInkDropCenter() const;
  virtual SkColor GetInkDropBaseColor() const;

  // Should return true if the ink drop is also used to depict focus.
  virtual bool ShouldShowInkDropForFocus() const;

  void set_ink_drop_delegate(std::unique_ptr<InkDropDelegate> delegate) {
    ink_drop_delegate_ = std::move(delegate);
  }

 private:
  std::unique_ptr<InkDropDelegate> ink_drop_delegate_;
  gfx::Size ink_drop_size_;
  bool destroying_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHostView);
};
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_HOST_VIEW_H_
