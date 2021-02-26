// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_SMS_ANDROID_SMS_INFOBAR_H_
#define COMPONENTS_BROWSER_UI_SMS_ANDROID_SMS_INFOBAR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/infobars/android/confirm_infobar.h"

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace sms {

class SmsInfoBarDelegate;

class SmsInfoBar : public infobars::ConfirmInfoBar {
 public:
  SmsInfoBar(content::WebContents* web_contents,
             const ResourceIdMapper& resource_mapper,
             std::unique_ptr<SmsInfoBarDelegate> delegate);
  ~SmsInfoBar() override;

  // Creates an SMS receiver infobar and delegate and adds it to
  // |infobar_service|.
  static void Create(content::WebContents* web_contents,
                     infobars::InfoBarManager* manager,
                     const ResourceIdMapper& resource_mapper,
                     const url::Origin& origin,
                     const std::string& one_time_code,
                     base::OnceCallback<void()> on_confirm,
                     base::OnceCallback<void()> on_cancel);

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(SmsInfoBar);
};

}  // namespace sms

#endif  // COMPONENTS_BROWSER_UI_SMS_ANDROID_SMS_INFOBAR_H_
