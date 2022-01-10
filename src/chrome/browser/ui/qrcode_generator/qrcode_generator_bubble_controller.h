// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;

namespace content {
class WebContents;
}

namespace qrcode_generator {

class QRCodeGeneratorBubbleView;

// Controller component of the QR Code Generator dialog bubble.
// Responsible for showing and hiding an owned bubble.
class QRCodeGeneratorBubbleController
    : public content::WebContentsUserData<QRCodeGeneratorBubbleController> {
 public:
  QRCodeGeneratorBubbleController(const QRCodeGeneratorBubbleController&) =
      delete;
  QRCodeGeneratorBubbleController& operator=(
      const QRCodeGeneratorBubbleController&) = delete;

  ~QRCodeGeneratorBubbleController() override;

  // Returns whether the generator is available for a given page.
  static bool IsGeneratorAvailable(const GURL& url);

  static QRCodeGeneratorBubbleController* Get(
      content::WebContents* web_contents);

  // Displays the QR Code Generator bubble.
  void ShowBubble(const GURL& url, bool show_back_button = false);

  // Hides the QR Code Generator bubble.
  void HideBubble();

  bool IsBubbleShown() { return bubble_shown_; }

  // Returns nullptr if no bubble is currently shown.
  QRCodeGeneratorBubbleView* qrcode_generator_bubble_view() const;

  // Handler for when the bubble is dismissed.
  void OnBubbleClosed();

  void OnBackButtonPressed();

 protected:
  explicit QRCodeGeneratorBubbleController(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<QRCodeGeneratorBubbleController>;

  void UpdateIcon();

  // Will be nullptr if no bubble is currently shown.
  raw_ptr<QRCodeGeneratorBubbleView> qrcode_generator_bubble_ = nullptr;

  // True if the bubble is currently shown.
  bool bubble_shown_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace qrcode_generator

#endif  // CHROME_BROWSER_UI_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_CONTROLLER_H_
