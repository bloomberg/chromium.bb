// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_POD_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_POD_BUTTON_H_

#include "ash/ash_export.h"
#include "base/bind.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}

namespace ash {

class FeaturePodControllerBase;

// ImageButon internally used in FeaturePodButton. Should not be used directly.
class FeaturePodIconButton : public views::ImageButton {
 public:
  explicit FeaturePodIconButton(views::ButtonListener* listener);
  ~FeaturePodIconButton() override;

  // Change the toggle state. See FeaturePodButton::SetToggled.
  void SetToggled(bool toggled);

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  const char* GetClassName() const override;

  bool toggled() const { return toggled_; }

 private:
  // Ture if the button is currently toggled.
  bool toggled_ = false;

  DISALLOW_COPY_AND_ASSIGN(FeaturePodIconButton);
};

// Buton internally used in FeaturePodButton. Should not be used directly.
class FeaturePodLabelButton : public views::Button {
 public:
  explicit FeaturePodLabelButton(views::ButtonListener* listener);
  ~FeaturePodLabelButton() override;

  // Set the text of label shown below the icon. See FeaturePodButton::SetLabel.
  void SetLabel(const base::string16& label);

  // Set the text of sub-label shown below the label.
  // See FeaturePodButton::SetSubLabel.
  void SetSubLabel(const base::string16& sub_label);

  // Show arrow to indicate that the feature has a detailed view.
  // See FeaturePodButton::ShowDetailedViewArrow.
  void ShowDetailedViewArrow();

  // views::Button:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  const char* GetClassName() const override;

 private:
  // Layout |child| in horizontal center with its vertical origin set to |y|.
  void LayoutInCenter(views::View* child, int y);

  void OnEnabledChanged();

  // Owned by views hierarchy.
  views::Label* const label_;
  views::Label* const sub_label_;
  views::ImageView* const detailed_view_arrow_;
  views::PropertyChangedSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&FeaturePodLabelButton::OnEnabledChanged,
                              base::Unretained(this)));

  DISALLOW_COPY_AND_ASSIGN(FeaturePodLabelButton);
};

// A button in FeaturePodsView. These buttons are main entry points of features
// in UnifiedSystemTray. Each button has its icon, label, and sub-label placed
// vertically. They are also togglable and the background color indicates the
// current state.
// See the comment in FeaturePodsView for detail.
class ASH_EXPORT FeaturePodButton : public views::View,
                                    public views::ButtonListener {
 public:
  explicit FeaturePodButton(FeaturePodControllerBase* controller);
  ~FeaturePodButton() override;

  // Set the vector icon shown in a circle.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Set the text of label shown below the icon.
  void SetLabel(const base::string16& label);

  // Set the text of sub-label shown below the label.
  void SetSubLabel(const base::string16& sub_label);

  // Set the tooltip text of the icon button.
  void SetIconTooltip(const base::string16& text);

  // Set the tooltip text of the label button.
  void SetLabelTooltip(const base::string16& text);

  // Convenience method to set both icon and label tooltip texts.
  void SetIconAndLabelTooltips(const base::string16& text);

  // Show arrow to indicate that the feature has a detailed view.
  void ShowDetailedViewArrow();

  // Remove the label button from keyboard focus chain. This is useful when
  // the icon button and the label button has the same action.
  void DisableLabelButtonFocus();

  // Change the toggled state. If toggled, the background color of the circle
  // will change.
  void SetToggled(bool toggled);
  bool IsToggled() const { return icon_button_->toggled(); }

  // Change the expanded state. 0.0 if collapsed, and 1.0 if expanded.
  // Otherwise, it shows intermediate state. In the collapsed state, the labels
  // are not shown.
  void SetExpandedAmount(double expanded_amount);

  // Only called by the container. Same as SetVisible but doesn't change
  // |visible_preferred_| flag.
  void SetVisibleByContainer(bool visible);

  // views::View:
  void SetVisible(bool visible) override;
  bool HasFocus() const override;
  void RequestFocus() override;
  const char* GetClassName() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  bool visible_preferred() const { return visible_preferred_; }

 protected:
  FeaturePodIconButton* icon_button() const { return icon_button_; }

 private:
  void OnEnabledChanged();

  // Unowned.
  FeaturePodControllerBase* const controller_;

  // Owned by views hierarchy.
  FeaturePodIconButton* const icon_button_;
  FeaturePodLabelButton* const label_button_;

  // If true, it is preferred by the FeaturePodController that the view is
  // visible. Usually, this should match visible(), but in case that the
  // container does not have enough space, it might not match.
  // In such case, the preferred visibility is reflected after the container is
  // expanded.
  bool visible_preferred_ = true;

  views::PropertyChangedSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&FeaturePodButton::OnEnabledChanged,
                              base::Unretained(this)));

  DISALLOW_COPY_AND_ASSIGN(FeaturePodButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_POD_BUTTON_H_
