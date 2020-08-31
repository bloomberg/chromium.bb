// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_PASSWORD_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_PASSWORD_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

class IOSChromeSavePasswordInfoBarDelegate;

// Helper object that updates the model layer for interaction events with the
// passwords infobar banner UI.
class PasswordInfobarBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  PasswordInfobarBannerInteractionHandler();
  ~PasswordInfobarBannerInteractionHandler() override;

  // InfobarBannerInteractionHandler:
  void BannerVisibilityChanged(InfoBarIOS* infobar, bool visible) override;
  void MainButtonTapped(InfoBarIOS* infobar) override;
  void ShowModalButtonTapped(InfoBarIOS* infobar,
                             web::WebState* web_state) override;
  void BannerDismissedByUser(InfoBarIOS* infobar) override;

 private:
  // Returns the password delegate from |infobar|.
  IOSChromeSavePasswordInfoBarDelegate* GetInfobarDelegate(InfoBarIOS* infobar);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_PASSWORD_INFOBAR_BANNER_INTERACTION_HANDLER_H_
