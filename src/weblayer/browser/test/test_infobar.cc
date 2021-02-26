// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/test/test_infobar.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "components/infobars/core/infobar_delegate.h"
#include "weblayer/browser/android/resource_mapper.h"
#include "weblayer/browser/infobar_service.h"
#include "weblayer/browser/java/test_jni/TestInfoBar_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

class TestInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  TestInfoBarDelegate() {}

  ~TestInfoBarDelegate() override {}

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  }
};

TestInfoBar::TestInfoBar(std::unique_ptr<TestInfoBarDelegate> delegate)
    : infobars::InfoBarAndroid(std::move(delegate),
                               base::BindRepeating(&MapToJavaDrawableId)) {}

TestInfoBar::~TestInfoBar() {}

void TestInfoBar::ProcessButton(int action) {}

ScopedJavaLocalRef<jobject> TestInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  return Java_TestInfoBar_create(env);
}

// static
void TestInfoBar::Show(content::WebContents* web_contents) {
  InfoBarService* service = InfoBarService::FromWebContents(web_contents);
  service->AddInfoBar(
      std::make_unique<TestInfoBar>(std::make_unique<TestInfoBarDelegate>()));
}

static void JNI_TestInfoBar_Show(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  auto* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  TestInfoBar::Show(web_contents);
}

}  // namespace weblayer
