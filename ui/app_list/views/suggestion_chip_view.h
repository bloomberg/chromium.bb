// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SUGGESTION_CHIP_VIEW_H_
#define UI_APP_LIST_VIEWS_SUGGESTION_CHIP_VIEW_H_

#include "base/macros.h"
#include "ui/app_list/app_list_export.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace app_list {

class SuggestionChipView;

// Listener which receives notification of suggestion chip events.
class APP_LIST_EXPORT SuggestionChipListener {
 public:
  // Invoked when the specified |sender| is pressed.
  virtual void OnSuggestionChipPressed(SuggestionChipView* sender) = 0;

 protected:
  virtual ~SuggestionChipListener() = default;
};

// View representing a suggestion chip.
class APP_LIST_EXPORT SuggestionChipView : public views::View {
 public:
  SuggestionChipView(const base::string16& text,
                     SuggestionChipListener* listener = nullptr);
  ~SuggestionChipView() override = default;

  // views::View:
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;

  const base::string16& GetText() const;

 private:
  void InitLayout();

  views::Label* text_view_;  // Owned by view hierarchy.
  SuggestionChipListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionChipView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SUGGESTION_CHIP_VIEW_H_
