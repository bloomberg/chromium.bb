// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_COOKIE_MANAGER_IMPL_H_
#define WEBLAYER_BROWSER_COOKIE_MANAGER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "weblayer/public/cookie_manager.h"

#if defined(OS_ANDROID)
#include <jni.h>
#include "base/android/scoped_java_ref.h"
#endif

namespace content {
class BrowserContext;
}

namespace weblayer {

class CookieManagerImpl : public CookieManager {
 public:
  explicit CookieManagerImpl(content::BrowserContext* browser_context);
  ~CookieManagerImpl() override;

  CookieManagerImpl(const CookieManagerImpl&) = delete;
  CookieManagerImpl& operator=(const CookieManagerImpl&) = delete;

  // CookieManager implementation:
  void SetCookie(const GURL& url,
                 const std::string& value,
                 SetCookieCallback callback) override;
  void GetCookie(const GURL& url, GetCookieCallback callback) override;
  std::unique_ptr<CookieChangedSubscription> AddCookieChangedCallback(
      const GURL& url,
      const std::string* name,
      CookieChangedCallback callback) override;

#if defined(OS_ANDROID)
  bool SetCookie(JNIEnv* env,
                 const base::android::JavaParamRef<jstring>& url,
                 const base::android::JavaParamRef<jstring>& value,
                 const base::android::JavaParamRef<jobject>& callback);
  void GetCookie(JNIEnv* env,
                 const base::android::JavaParamRef<jstring>& url,
                 const base::android::JavaParamRef<jobject>& callback);
  int AddCookieChangedCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jstring>& name,
      const base::android::JavaParamRef<jobject>& callback);
  void RemoveCookieChangedCallback(JNIEnv* env, int id);
#endif

 private:
  bool SetCookieInternal(const GURL& url,
                         const std::string& value,
                         SetCookieCallback callback);
  int AddCookieChangedCallbackInternal(const GURL& url,
                                       const std::string* name,
                                       CookieChangedCallback callback);
  void RemoveCookieChangedCallbackInternal(int id);

  content::BrowserContext* browser_context_;
  mojo::ReceiverSet<network::mojom::CookieChangeListener,
                    std::unique_ptr<network::mojom::CookieChangeListener>>
      cookie_change_receivers_;
  base::WeakPtrFactory<CookieManagerImpl> weak_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_COOKIE_MANAGER_IMPL_H_
