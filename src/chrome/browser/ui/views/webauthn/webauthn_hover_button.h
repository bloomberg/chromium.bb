// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_HOVER_BUTTON_H_

#include <memory>

#include "base/strings/string16.h"
#include "chrome/browser/ui/views/hover_button.h"

namespace views {
class ButtonListener;
class ImageView;
class Label;
class View;
}  // namespace views

// WebauthnHoverButton is a hoverable button with a primary left-hand icon, a
// title and subtitle, and a secondary right-hand icon (usually a submenu
// arrow). Icons and subtitle are optional.
class WebAuthnHoverButton : public HoverButton {
 public:
  // Creates a hoverable button with the given elements, like so:
  //
  // +-------------------------------------------------------------------+
  // |      |    title                                            |second|
  // | icon |                                                     |ary_ic|
  // |      |    subtitle                                         |  on  |
  // +-------------------------------------------------------------------+
  //
  // If |subtitle| is omitted and |force_two_line| is false, the button shrinks
  // to a single-line automatically; if |force_two_line| is true, |title| is
  // horizontally centered over the two-line height.
  //
  // |icon| and |secondary_icon| are also optional. If either is null, the
  // middle column resizes to fill the space.
  WebAuthnHoverButton(views::ButtonListener* listener,
                      std::unique_ptr<views::ImageView> icon,
                      const base::string16& title,
                      const base::string16& subtitle,
                      std::unique_ptr<views::View> secondary_icon,
                      bool force_two_line = false);
  WebAuthnHoverButton(const WebAuthnHoverButton&) = delete;
  WebAuthnHoverButton& operator=(const WebAuthnHoverButton&) = delete;
  ~WebAuthnHoverButton() override = default;

 private:
  views::Label* title_ = nullptr;
  views::Label* subtitle_ = nullptr;
  views::View* icon_view_ = nullptr;
  views::View* secondary_icon_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_HOVER_BUTTON_H_
