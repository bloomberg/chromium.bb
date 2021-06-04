// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/generated_password_saved_infobar.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/android/chrome_jni_headers/GeneratedPasswordSavedInfoBarDelegate_jni.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "content/public/browser/web_contents.h"

using base::android::JavaParamRef;

// static
void GeneratedPasswordSavedInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents) {
  infobars::ContentInfoBarManager::FromWebContents(web_contents)
      ->AddInfoBar(
          std::make_unique<GeneratedPasswordSavedInfoBar>(base::WrapUnique(
              new GeneratedPasswordSavedInfoBarDelegateAndroid())));
}

GeneratedPasswordSavedInfoBar::GeneratedPasswordSavedInfoBar(
    std::unique_ptr<GeneratedPasswordSavedInfoBarDelegateAndroid> delegate)
    : infobars::InfoBarAndroid(std::move(delegate)) {}

GeneratedPasswordSavedInfoBar::~GeneratedPasswordSavedInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject>
GeneratedPasswordSavedInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  GeneratedPasswordSavedInfoBarDelegateAndroid* infobar_delegate =
      static_cast<GeneratedPasswordSavedInfoBarDelegateAndroid*>(delegate());

  return Java_GeneratedPasswordSavedInfoBarDelegate_show(
      env, resource_id_mapper.Run(delegate()->GetIconId()),
      base::android::ConvertUTF16ToJavaString(env,
                                              infobar_delegate->message_text()),
      base::android::ConvertUTF16ToJavaString(
          env, infobar_delegate->details_message_text()),
      infobar_delegate->inline_link_range().start(),
      infobar_delegate->inline_link_range().end(),
      base::android::ConvertUTF16ToJavaString(
          env, infobar_delegate->button_label()));
}

void GeneratedPasswordSavedInfoBar::OnLinkClicked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!owner())
    return;

  static_cast<GeneratedPasswordSavedInfoBarDelegateAndroid*>(delegate())
      ->OnInlineLinkClicked();
  RemoveSelf();
}

void GeneratedPasswordSavedInfoBar::ProcessButton(int action) {
  if (!owner())
    return;

  RemoveSelf();
}
