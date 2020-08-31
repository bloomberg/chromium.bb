// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/InstalledAppProviderImpl_jni.h"
#include "chrome/browser/installable/digital_asset_links/digital_asset_links_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace {

void DidGetResult(
    std::unique_ptr<digital_asset_links::DigitalAssetLinksHandler> handler,
    base::OnceCallback<void(bool)> callback,
    digital_asset_links::RelationshipCheckResult result) {
  std::move(callback).Run(
      result == digital_asset_links::RelationshipCheckResult::kSuccess);
}

}  // namespace

void JNI_InstalledAppProviderImpl_CheckDigitalAssetLinksRelationshipForWebApk(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile,
    const base::android::JavaParamRef<jstring>& jwebDomain,
    const base::android::JavaParamRef<jstring>& jmanifestUrl,
    const base::android::JavaParamRef<jobject>& jcallback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);

  std::string web_domain = ConvertJavaStringToUTF8(env, jwebDomain);
  std::string manifest_url = ConvertJavaStringToUTF8(env, jmanifestUrl);
  auto callback =
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback));

  auto handler =
      std::make_unique<digital_asset_links::DigitalAssetLinksHandler>(
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetURLLoaderFactoryForBrowserProcess());
  auto* handler_ptr = handler.get();

  // |handler| is owned by the callback, so it will be valid until the execution
  // is over.
  handler_ptr->CheckDigitalAssetLinkRelationshipForWebApk(
      web_domain, manifest_url,
      base::BindOnce(&DidGetResult, std::move(handler), std::move(callback)));
}
