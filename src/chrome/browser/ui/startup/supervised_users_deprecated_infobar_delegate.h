// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_SUPERVISED_USERS_DEPRECATED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_SUPERVISED_USERS_DEPRECATED_INFOBAR_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

class InfoBarService;

// An infobar that displays a message saying that supervised users is obsolete,
// along with a "Learn More" link.
class SupervisedUsersDeprecatedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static void Create(InfoBarService* infobar_service);

 private:
  SupervisedUsersDeprecatedInfoBarDelegate();
  ~SupervisedUsersDeprecatedInfoBarDelegate() override;

  // ConfirmInfoBarDelegate implementation.
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;
  bool ShouldExpire(const NavigationDetails& navigation_details) const override;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUsersDeprecatedInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_STARTUP_SUPERVISED_USERS_DEPRECATED_INFOBAR_DELEGATE_H_
