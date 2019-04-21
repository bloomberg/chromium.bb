// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_UPDATE_PASSWORD_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_UPDATE_PASSWORD_INFOBAR_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

class UpdatePasswordInfoBarDelegate;

// The infobar to be used with UpdatePasswordInfoBarDelegate.
class UpdatePasswordInfoBar : public ConfirmInfoBar {
 public:
  explicit UpdatePasswordInfoBar(
      std::unique_ptr<UpdatePasswordInfoBarDelegate> delegate);
  ~UpdatePasswordInfoBar() override;

  int GetIdOfSelectedUsername() const;

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;
  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj) override;

  base::android::ScopedJavaGlobalRef<jobject> java_infobar_;

  DISALLOW_COPY_AND_ASSIGN(UpdatePasswordInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_UPDATE_PASSWORD_INFOBAR_H_
