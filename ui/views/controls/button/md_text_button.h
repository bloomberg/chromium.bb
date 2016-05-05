// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_

#include <memory>

#include "ui/views/animation/button_ink_drop_delegate.h"
#include "ui/views/controls/button/label_button.h"

namespace views {

// A button class that implements the Material Design text button spec.
class VIEWS_EXPORT MdTextButton : public LabelButton {
 public:
  // Describes the presentation of a button. A stronger call to action draws
  // more attention.
  enum CallToAction {
    NO_CALL_TO_ACTION,  // Default.
    WEAK_CALL_TO_ACTION,
    STRONG_CALL_TO_ACTION,
  };

  // Creates a normal STYLE_BUTTON LabelButton in pre-MD, or an MdTextButton
  // in MD mode.
  static LabelButton* CreateStandardButton(ButtonListener* listener,
                                           const base::string16& text);
  // As above, but only creates an MdTextButton if MD is enabled in the
  // secondary UI (as opposed to just "top chrome"/"primary" UI).
  static LabelButton* CreateSecondaryUiButton(ButtonListener* listener,
                                              const base::string16& text);
  static MdTextButton* CreateMdButton(ButtonListener* listener,
                                      const base::string16& text);

  void SetCallToAction(CallToAction cta);

  // LabelButton:
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;
  SkColor GetInkDropBaseColor() const override;
  void SetText(const base::string16& text) override;
  void UpdateStyleToIndicateDefaultStatus() override;

 private:
  MdTextButton(ButtonListener* listener);
  ~MdTextButton() override;

  void UpdateColorsFromNativeTheme();

  ButtonInkDropDelegate ink_drop_delegate_;

  // The call to action style for this button.
  CallToAction cta_;

  DISALLOW_COPY_AND_ASSIGN(MdTextButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_H_
