// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_NAME_FIX_FLOW_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_NAME_FIX_FLOW_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/strings/string16.h"

namespace content {
class WebContents;
}

namespace autofill {

class CardNameFixFlowViewDelegateMobile;

class CardNameFixFlowViewAndroid {
 public:
  CardNameFixFlowViewAndroid(
      std::unique_ptr<CardNameFixFlowViewDelegateMobile> delegate,
      content::WebContents* web_contents);

  ~CardNameFixFlowViewAndroid();

  void OnUserAccept(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& name);
  void PromptDismissed(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

  void Show();

 private:
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  std::unique_ptr<CardNameFixFlowViewDelegateMobile> delegate_;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(CardNameFixFlowViewAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_NAME_FIX_FLOW_VIEW_ANDROID_H_
