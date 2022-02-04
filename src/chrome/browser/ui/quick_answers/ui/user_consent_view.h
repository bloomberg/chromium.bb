// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_

#include <memory>

#include "chrome/browser/ui/quick_answers/ui/quick_answers_focus_search.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_pre_target_handler.h"
#include "ui/views/view.h"

namespace views {
class LabelButton;
}  // namespace views

class QuickAnswersUiController;

namespace quick_answers {

// |intent_type| and |intent_text| are used to generate the consent title
// including predicted intent information. Fallback to title without intent
// information if any of these two strings are empty.
class UserConsentView : public views::View {
 public:
  UserConsentView(const gfx::Rect& anchor_view_bounds,
                  const std::u16string& intent_type,
                  const std::u16string& intent_text,
                  QuickAnswersUiController* ui_controller);

  // Disallow copy and assign.
  UserConsentView(const UserConsentView&) = delete;
  UserConsentView& operator=(const UserConsentView&) = delete;

  ~UserConsentView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void OnFocus() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  void UpdateAnchorViewBounds(const gfx::Rect& anchor_view_bounds);

 private:
  void InitLayout();
  void InitContent();
  void InitButtonBar();
  void InitWidget();
  void UpdateWidgetBounds();

  // QuickAnswersFocusSearch::GetFocusableViewsCallback to poll currently
  // focusable views.
  std::vector<views::View*> GetFocusableViews();

  // Cached bounds of the anchor this view is tied to.
  gfx::Rect anchor_view_bounds_;
  // Cached title text.
  std::u16string title_;

  QuickAnswersPreTargetHandler event_handler_;
  QuickAnswersUiController* const ui_controller_;
  QuickAnswersFocusSearch focus_search_;

  // Owned by view hierarchy.
  views::View* main_view_ = nullptr;
  views::View* content_ = nullptr;
  views::LabelButton* no_thanks_button_ = nullptr;
  views::LabelButton* allow_button_ = nullptr;
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_
