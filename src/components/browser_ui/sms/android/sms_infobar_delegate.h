// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_SMS_ANDROID_SMS_INFOBAR_DELEGATE_H_
#define COMPONENTS_BROWSER_UI_SMS_ANDROID_SMS_INFOBAR_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/origin.h"

namespace sms {

// This class configures an infobar shown when an SMS is received and the user
// is asked for confirmation that it should be shared with the site (WebOTP).
class SmsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SmsInfoBarDelegate(const url::Origin& origin,
                     const std::string& one_time_code,
                     base::OnceClosure on_confirm,
                     base::OnceClosure on_cancel);
  ~SmsInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  void InfoBarDismissed() override;

  base::string16 GetTitle() const;

 private:
  const url::Origin origin_;
  const std::string one_time_code_;
  base::OnceClosure on_confirm_;
  base::OnceClosure on_cancel_;
  DISALLOW_COPY_AND_ASSIGN(SmsInfoBarDelegate);
};

}  // namespace sms

#endif  // COMPONENTS_BROWSER_UI_SMS_ANDROID_SMS_INFOBAR_DELEGATE_H_
