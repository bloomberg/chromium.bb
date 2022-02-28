// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/download_progress_infobar.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/android/chrome_jni_headers/DownloadProgressInfoBar_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

class DownloadProgressInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  DownloadProgressInfoBarDelegate(const JavaParamRef<jobject>& jclient,
                                  const JavaParamRef<jobject>& jdata) {
    JNIEnv* env = base::android::AttachCurrentThread();
    client_.Reset(env, jclient);
    data_.Reset(env, jdata);
  }

  DownloadProgressInfoBarDelegate(const DownloadProgressInfoBarDelegate&) =
      delete;
  DownloadProgressInfoBarDelegate& operator=(
      const DownloadProgressInfoBarDelegate&) = delete;

  ~DownloadProgressInfoBarDelegate() override = default;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return InfoBarDelegate::InfoBarIdentifier::
        DOWNLOAD_PROGRESS_INFOBAR_ANDROID;
  }

  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override {
    return delegate->GetIdentifier() == GetIdentifier();
  }

  base::android::ScopedJavaGlobalRef<jobject> data() { return data_; }

  base::android::ScopedJavaGlobalRef<jobject> client() { return client_; }

 private:
  base::android::ScopedJavaGlobalRef<jobject> client_;
  base::android::ScopedJavaGlobalRef<jobject> data_;
};

DownloadProgressInfoBar::DownloadProgressInfoBar(
    std::unique_ptr<DownloadProgressInfoBarDelegate> delegate)
    : infobars::InfoBarAndroid(std::move(delegate)) {}

DownloadProgressInfoBar::~DownloadProgressInfoBar() = default;

infobars::InfoBarDelegate* DownloadProgressInfoBar::GetDelegate() {
  return delegate();
}

ScopedJavaLocalRef<jobject> DownloadProgressInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  DownloadProgressInfoBarDelegate* delegate =
      static_cast<DownloadProgressInfoBarDelegate*>(GetDelegate());

  return Java_DownloadProgressInfoBar_create(env, delegate->client(),
                                             delegate->data());
}

void DownloadProgressInfoBar::ProcessButton(int action) {}

base::android::ScopedJavaLocalRef<jobject> DownloadProgressInfoBar::GetTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  content::WebContents* web_contents =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(this);
  if (!web_contents)
    return nullptr;

  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  return tab_android ? tab_android->GetJavaObject() : nullptr;
}

void JNI_DownloadProgressInfoBar_Create(JNIEnv* env,
                                        const JavaParamRef<jobject>& j_client,
                                        const JavaParamRef<jobject>& j_tab,
                                        const JavaParamRef<jobject>& j_data) {
  infobars::ContentInfoBarManager* manager =
      infobars::ContentInfoBarManager::FromWebContents(
          TabAndroid::GetNativeTab(env, j_tab)->web_contents());

  manager->AddInfoBar(std::make_unique<DownloadProgressInfoBar>(
      std::make_unique<DownloadProgressInfoBarDelegate>(j_client, j_data)));
}
