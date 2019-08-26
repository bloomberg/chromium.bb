// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_CONTROLLER_IMPL_H_
#define WEBLAYER_BROWSER_BROWSER_CONTROLLER_IMPL_H_

#include "build/build_config.h"
#include "weblayer/public/browser_controller.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace content {
class WebContents;
}

namespace weblayer {
class NavigationControllerImpl;

class BrowserControllerImpl : public BrowserController {
 public:
  BrowserControllerImpl(Profile* profile, const gfx::Size& initial_size);
  ~BrowserControllerImpl() override;

  // BrowserController implementation:
  NavigationController* GetNavigationController() override;
#if !defined(OS_ANDROID)
  void AttachToView(views::WebView* web_view) override;
#endif

  content::WebContents* web_contents() const { return web_contents_.get(); }

#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void Navigate(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                const base::android::JavaParamRef<jstring>& path);
#endif

 private:
  Profile* profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NavigationControllerImpl> navigation_controller_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_CONTROLLER_IMPL_H_
