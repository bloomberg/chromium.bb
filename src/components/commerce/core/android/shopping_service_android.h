// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_ANDROID_SHOPPING_SERVICE_ANDROID_H_
#define COMPONENTS_COMMERCE_CORE_ANDROID_SHOPPING_SERVICE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/commerce/core/shopping_service.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;

class GURL;

namespace commerce {

class ShoppingService;

class ShoppingServiceAndroid : public base::SupportsUserData::Data {
 public:
  ShoppingServiceAndroid(const ShoppingServiceAndroid&) = delete;
  ShoppingServiceAndroid& operator=(const ShoppingServiceAndroid&) = delete;

  ShoppingServiceAndroid(ShoppingService* service);
  ~ShoppingServiceAndroid() override;

  void GetProductInfoForUrl(JNIEnv* env,
                            const JavaParamRef<jobject>& obj,
                            const JavaParamRef<jobject>& j_gurl,
                            const JavaParamRef<jobject>& j_callback);

  ScopedJavaGlobalRef<jobject> java_ref() { return java_ref_; }

 private:
  void HandleProductInfoCallback(JNIEnv* env,
                                 const ScopedJavaGlobalRef<jobject>& callback,
                                 const GURL& url,
                                 const absl::optional<ProductInfo>& info);

  // A handle to the backing shopping service. This is held as a raw pointer
  // since this object's lifecycle is tied to the service itself. This object
  // will always be destroyed before the service is.
  raw_ptr<ShoppingService> shopping_service_;

  // A handle to the java side of this object.
  ScopedJavaGlobalRef<jobject> java_ref_;

  base::WeakPtrFactory<ShoppingServiceAndroid> weak_ptr_factory_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_ANDROID_SHOPPING_SERVICE_ANDROID_H_
