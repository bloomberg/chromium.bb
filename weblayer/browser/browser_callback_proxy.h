// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_CALLBACK_PROXY_H_
#define WEBLAYER_BROWSER_BROWSER_CALLBACK_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "weblayer/public/browser_observer.h"

namespace weblayer {

class BrowserController;

// BrowserCallbackProxy forwards all BrowserObserver functions to the Java
// side. There is one BrowserCallbackProxy per BrowserController.
class BrowserCallbackProxy : public BrowserObserver {
 public:
  BrowserCallbackProxy(JNIEnv* env,
                       jobject obj,
                       BrowserController* browser_controller);
  ~BrowserCallbackProxy() override;

  // BrowserObserver:
  void DisplayedUrlChanged(const GURL& url) override;

 private:
  BrowserController* browser_controller_;
  base::android::ScopedJavaGlobalRef<jobject> java_observer_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCallbackProxy);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_CALLBACK_PROXY_H_
