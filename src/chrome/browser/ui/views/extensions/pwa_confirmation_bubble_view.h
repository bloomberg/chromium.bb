// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PWA_CONFIRMATION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PWA_CONFIRMATION_BUBBLE_VIEW_H_

#include "chrome/browser/ui/views/extensions/pwa_confirmation.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/web_contents.h"

// PWAConfirmationBubbleView provides a bubble dialog for accepting or rejecting
// the installation of a PWA (Progressive Web App) anchored off the PWA install
// icon in the omnibox.
class PWAConfirmationBubbleView : public LocationBarBubbleDelegateView {
 public:
  PWAConfirmationBubbleView(views::View* anchor_view,
                            views::Button* highlight_button,
                            std::unique_ptr<WebApplicationInfo> web_app_info,
                            chrome::AppInstallationAcceptanceCallback callback);

  // LocationBarBubbleDelegateView:
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;
  void WindowClosing() override;
  bool Accept() override;

 private:
  PWAConfirmation pwa_confirmation_;

  DISALLOW_COPY_AND_ASSIGN(PWAConfirmationBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PWA_CONFIRMATION_BUBBLE_VIEW_H_
