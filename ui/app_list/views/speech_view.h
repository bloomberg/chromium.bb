// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SPEECH_VIEW_H_
#define UI_APP_LIST_VIEWS_SPEECH_VIEW_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/speech_ui_model_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class BoundsAnimator;
class ImageButton;
class ImageView;
class Label;
}

namespace app_list {

class AppListViewDelegate;

// SpeechView provides the card-like UI for the search-by-speech.
class APP_LIST_EXPORT SpeechView : public views::View,
                                   public views::ButtonListener,
                                   public SpeechUIModelObserver {
 public:
  explicit SpeechView(AppListViewDelegate* delegate);
  ~SpeechView() override;

  // Reset to the original state.
  void Reset();

  // Overridden from views::View:
  void Layout() override;
  gfx::Size GetPreferredSize() const override;

  views::ImageButton* mic_button() { return mic_button_; }

 private:
  int GetIndicatorRadius(uint8_t level);

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from SpeechUIModelObserver:
  void OnSpeechSoundLevelChanged(uint8_t level) override;
  void OnSpeechResult(const base::string16& result, bool is_final) override;
  void OnSpeechRecognitionStateChanged(
      SpeechRecognitionState new_state) override;

  AppListViewDelegate* delegate_;

  views::ImageView* logo_;
  views::View* indicator_;
  views::ImageButton* mic_button_;
  views::Label* speech_result_;
  std::unique_ptr<views::BoundsAnimator> indicator_animator_;

  DISALLOW_COPY_AND_ASSIGN(SpeechView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SPEECH_VIEW_H_
