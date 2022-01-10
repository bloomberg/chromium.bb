// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace gfx {
class SlideAnimation;
}

namespace views {
class ImageView;
}

namespace ash {
class Shelf;

// Label view which can be given a different data from the visible label.
// IME icons like "US" (US keyboard) or "あ(Google Japanese Input)" are
// rendered as a label, but reading such text literally will not always be
// understandable.
class IconizedLabel : public views::Label {
 public:
  void SetCustomAccessibleName(const std::u16string& name) {
    custom_accessible_name_ = name;
  }

  std::u16string GetAccessibleNameString() const {
    return custom_accessible_name_;
  }

  // views::Label:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  std::u16string custom_accessible_name_;
};

// Base-class for items in the tray. It makes sure the widget is updated
// correctly when the visibility/size of the tray item changes. It also adds
// animation when showing/hiding the item in the tray.
class ASH_EXPORT TrayItemView : public views::View,
                                public views::AnimationDelegateViews {
 public:
  explicit TrayItemView(Shelf* shelf);

  TrayItemView(const TrayItemView&) = delete;
  TrayItemView& operator=(const TrayItemView&) = delete;

  ~TrayItemView() override;

  // Convenience function for creating a child Label or ImageView.
  // Only one of the two should be called.
  void CreateLabel();
  void CreateImageView();

  // Called when locale change is detected (which should not happen after the
  // user session starts). It should reload any strings the view is using.
  virtual void HandleLocaleChange() = 0;

  IconizedLabel* label() const { return label_; }
  views::ImageView* image_view() const { return image_view_; }

  // views::View.
  void SetVisible(bool visible) override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  const char* GetClassName() const override;

  void set_use_scale_in_animation(bool use_scale_in_animation) {
    use_scale_in_animation_ = use_scale_in_animation;
  }

 protected:
  // Returns whether the shelf is horizontal.
  bool IsHorizontalAlignment() const;

 private:
  // views::View.
  void ChildPreferredSizeChanged(View* child) override;

  // views::AnimationDelegateViews.
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // Return true if the animation is in resize animation stage, which
  // happens before item animating in and after item animating out.
  bool InResizeAnimation(double animation_value) const;

  // Converts the overall visibility animation progress to the progress for the
  // animation stage that resize the tray container.
  double GetResizeProgressFromAnimationProgress(double animation_value) const;

  // Converts the overall visibility animation progress to the progress for the
  // animation stage that fades and scales the tray item.
  double GetItemScaleProgressFromAnimationProgress(
      double animation_value) const;

  Shelf* const shelf_;

  // When showing the item in tray, the animation is executed with 2 stages:
  // 1. Resize: The size reserved for tray item view gradually increases.
  // 2. Item animation: After size has changed to the target size, the actual
  //    tray item starts appearing.
  // The steps reverse when hiding the item (the item disappears, then width
  // change animation).
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // The target visibility for the item when all the animation is done.
  bool target_visible_ = false;

  // Use scale in animating in the item to the tray.
  bool use_scale_in_animation_ = true;

  // Only one of |label_| and |image_view_| should be non-null.
  IconizedLabel* label_;
  views::ImageView* image_view_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_
