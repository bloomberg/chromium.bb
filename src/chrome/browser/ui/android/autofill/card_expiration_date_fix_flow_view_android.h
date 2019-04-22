// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_EXPIRATION_DATE_FIX_FLOW_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_EXPIRATION_DATE_FIX_FLOW_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "memory"

namespace content {
class WebContents;
}

namespace autofill {

class CardExpirationDateFixFlowViewDelegateMobile;

class CardExpirationDateFixFlowViewAndroid {
 public:
  CardExpirationDateFixFlowViewAndroid(
      std::unique_ptr<CardExpirationDateFixFlowViewDelegateMobile> delegate,
      content::WebContents* web_contents);

  ~CardExpirationDateFixFlowViewAndroid();

  void OnUserAccept(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& month,
                    const base::android::JavaParamRef<jstring>& year);
  void PromptDismissed(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

  void Show();

 private:
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  std::unique_ptr<CardExpirationDateFixFlowViewDelegateMobile> delegate_;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(CardExpirationDateFixFlowViewAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_EXPIRATION_DATE_FIX_FLOW_VIEW_ANDROID_H_
