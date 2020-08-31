// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}

namespace views {
class ImageView;
class LabelButton;
class TooltipIcon;
class View;
}  // namespace views

namespace qrcode_generator {

class QRCodeGeneratorBubbleController;

// Dialog that displays a QR code used to share a page or image.
class QRCodeGeneratorBubble : public QRCodeGeneratorBubbleView,
                              public LocationBarBubbleDelegateView,
                              public views::TextfieldController,
                              public views::ButtonListener {
 public:
  QRCodeGeneratorBubble(views::View* anchor_view,
                        content::WebContents* web_contents,
                        QRCodeGeneratorBubbleController* controller,
                        const GURL& url);
  void Show();

  // QRCodeGeneratorBubbleView:
  void Hide() override;

  // Returns a suggested download filename for a given URL.
  // e.g.: www.foo.com may suggest qrcode_foo.png.
  static const base::string16 GetQRCodeFilenameForURL(const GURL& url);

 private:
  ~QRCodeGeneratorBubble() override;

  // Updates and formats QR code, text, and controls.
  void UpdateQRContent();

  // Updates the central QR code image with |qr_image|.
  void UpdateQRImage(gfx::ImageSkia qr_image);

  // Updates the central QR code image with a placeholder.
  void DisplayPlaceholderImage();

  // LocationBarBubbleDelegateView:
  View* GetInitiallyFocusedView() override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;
  const char* GetClassName() const override;

  // views::BubbleDialogDelegateView:
  void Init() override;

  // TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;

  // ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Callback for the request to the OOP service to generate a new image.
  void OnCodeGeneratorResponse(const mojom::GenerateQRCodeResponsePtr response);

  // Remote to service instance to generate QR code images.
  mojo::Remote<mojom::QRCodeGeneratorService> qr_code_service_remote_;

  // URL for which the QR code is being generated.
  // Used for validation.
  GURL url_;

  // Pointers to view widgets; weak.
  views::ImageView* qr_code_image_ = nullptr;
  views::Textfield* textfield_url_ = nullptr;
  views::LabelButton* download_button_ = nullptr;
  views::TooltipIcon* tooltip_icon_ = nullptr;

  QRCodeGeneratorBubbleController* controller_;  // weak.
  content::WebContents* web_contents_;           // weak.

  DISALLOW_COPY_AND_ASSIGN(QRCodeGeneratorBubble);
};

}  // namespace qrcode_generator

#endif  // CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_H_
