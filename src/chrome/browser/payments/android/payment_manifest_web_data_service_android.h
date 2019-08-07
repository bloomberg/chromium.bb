// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_ANDROID_PAYMENT_MANIFEST_WEB_DATA_SERVICE_ANDROID_H_
#define CHROME_BROWSER_PAYMENTS_ANDROID_PAYMENT_MANIFEST_WEB_DATA_SERVICE_ANDROID_H_

#include <map>
#include <memory>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace payments {

// Android wrapper of the PaymentManifestWebDataService which provides access
// from the Java layer. Note that on Android, there's only a single profile, and
// therefore a single instance of this wrapper.
class PaymentManifestWebDataServiceAndroid : public WebDataServiceConsumer {
 public:
  PaymentManifestWebDataServiceAndroid(JNIEnv* env, jobject obj);
  ~PaymentManifestWebDataServiceAndroid() override;

  // Override WebDataServiceConsumer interface.
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  // Destroys this object.
  void Destroy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& unused_obj);

  // Adds the supported |japp_package_names| of the |jmethod_name| to the
  // cache.
  void AddPaymentMethodManifest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jmethod_name,
      const base::android::JavaParamRef<jobjectArray>& japp_package_names);

  // Adds the web app |jmanifest_sections|.
  void AddPaymentWebAppManifest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobjectArray>& jmanifest_sections);

  // Gets the payment |jmethod_name|'s manifest asynchronously from the web data
  // service. Return true if the result will be returned through |jcallback|.
  bool GetPaymentMethodManifest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jmethod_name,
      const base::android::JavaParamRef<jobject>& jcallback);

  // Gets the payment |japp_package_name|'s manifest asynchronously from the web
  // data service. Return true if the result will be returned through
  // |jcallback|.
  bool GetPaymentWebAppManifest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& japp_package_name,
      const base::android::JavaParamRef<jobject>& jcallback);

 private:
  void OnWebAppManifestRequestDone(JNIEnv* env,
                                   WebDataServiceBase::Handle h,
                                   WDTypedResult* result);
  void OnPaymentMethodManifestRequestDone(JNIEnv* env,
                                          WebDataServiceBase::Handle h,
                                          WDTypedResult* result);

  // Pointer to the java counterpart.
  JavaObjectWeakGlobalRef weak_java_obj_;

  // Map of request handle and its correspond callback.
  std::map<WebDataServiceBase::Handle,
           std::unique_ptr<base::android::ScopedJavaGlobalRef<jobject>>>
      web_data_service_requests_;

  DISALLOW_COPY_AND_ASSIGN(PaymentManifestWebDataServiceAndroid);
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_ANDROID_PAYMENT_MANIFEST_WEB_DATA_SERVICE_ANDROID_H_
