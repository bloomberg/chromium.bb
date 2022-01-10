// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_INSTANT_APPS_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_INSTANT_APPS_INFOBAR_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/instantapps/instant_apps_infobar_delegate.h"
#include "components/infobars/android/confirm_infobar.h"

class InstantAppsInfoBar : public infobars::ConfirmInfoBar {
 public:
  InstantAppsInfoBar(
      std::unique_ptr<InstantAppsInfoBarDelegate> delegate);

  InstantAppsInfoBar(const InstantAppsInfoBar&) = delete;
  InstantAppsInfoBar& operator=(const InstantAppsInfoBar&) = delete;

  ~InstantAppsInfoBar() override;

 private:
  // ConfimInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  base::android::ScopedJavaGlobalRef<jobject> java_infobar_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_INSTANT_APPS_INFOBAR_H_
