// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_NEW_BADGE_LABEL_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_NEW_BADGE_LABEL_H_

#include <memory>

#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/new_badge.h"
#include "ui/views/style/typography.h"

namespace views {
class Border;
}

// Extends views::Label to optionally display a "New" badge next to the text,
// drawing attention to a new feature in Chrome.
//
// When |display_new_badge| is set to false, behaves exactly as a normal Label,
// with the caveat that the following are explicitly disallowed:
//  * Calling SetDisplayNewBadge() when the label is visible to the user.
//  * Calling SetBorder() from external code, as the border is used to create
//    space to render the badge.
class NewBadgeLabel : public views::Label {
 public:
  // Determines how the badge is placed relative to the label text if the label
  // is wider than its preferred size (has no effect otherwise).
  enum class BadgePlacement {
    // Places the "New" badge immediately after the label text (default).
    kImmediatelyAfterText,
    // Places the "New" badge all the way at the trailing edge of the control,
    // which is the right edge for LTR and the left edge for RTL.
    kTrailingEdge
  };

  METADATA_HEADER(NewBadgeLabel);

  // Constructs a new badge label. Designed to be argument-compatible with the
  // views::Label constructor so they can be substituted.
  explicit NewBadgeLabel(const std::u16string& text = std::u16string(),
                         int text_context = views::style::CONTEXT_LABEL,
                         int text_style = views::style::STYLE_PRIMARY,
                         gfx::DirectionalityMode directionality_mode =
                             gfx::DirectionalityMode::DIRECTIONALITY_FROM_TEXT);
  NewBadgeLabel(const std::u16string& text, const CustomFont& font);
  ~NewBadgeLabel() override;

  // Sets whether the New badge is shown on this label.
  // Should only be called before the label is shown.
  void SetDisplayNewBadge(bool display_new_badge);
  bool GetDisplayNewBadge() const { return display_new_badge_; }

  void SetPadAfterNewBadge(bool pad_after_new_badge);
  bool GetPadAfterNewBadge() const { return pad_after_new_badge_; }

  void SetBadgePlacement(BadgePlacement badge_placement);
  BadgePlacement GetBadgePlacement() const { return badge_placement_; }

  // Label:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  int GetHeightForWidth(int w) const override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  // Hide the SetBorder() method so that external callers can't use it since we
  // rely on it to add padding. This won't prevent access via downcast, however.
  void SetBorder(std::unique_ptr<views::Border> b) override;

  // Specifies whether the badge should be displayed. Defaults to true, but we
  // allow the badge to be selectively disabled during experiments/feature
  // rollouts without having to swap this object with a vanilla Label.
  bool display_new_badge_ = true;

  // Add the required internal padding to the label so that there is room to
  // display the new badge.
  void UpdatePaddingForNewBadge();

  // Specifies the placement of the "New" badge when the label is wider than its
  // preferred size.
  BadgePlacement badge_placement_ = BadgePlacement::kImmediatelyAfterText;

  // Determines whether there is additional internal margin to the right of the
  // "New" badge. When set to true, the space will be allocated, and
  // kInternalPaddingKey will be set so that layouts know this space is empty.
  bool pad_after_new_badge_ = true;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_NEW_BADGE_LABEL_H_
