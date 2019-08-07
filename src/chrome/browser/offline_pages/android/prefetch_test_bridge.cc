// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher_impl.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "jni/PrefetchTestBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// Below is the native implementation of PrefetchTestBridge.java.

namespace offline_pages {
namespace prefetch {

JNI_EXPORT void JNI_PrefetchTestBridge_EnableLimitlessPrefetching(
    JNIEnv* env,
    jboolean enable) {
  prefetch_prefs::SetLimitlessPrefetchingEnabled(
      ProfileManager::GetLastUsedProfile()->GetPrefs(), enable != 0);
}

JNI_EXPORT jboolean
JNI_PrefetchTestBridge_IsLimitlessPrefetchingEnabled(JNIEnv* env) {
  return static_cast<jboolean>(prefetch_prefs::IsLimitlessPrefetchingEnabled(
      ProfileManager::GetLastUsedProfile()->GetPrefs()));
}

JNI_EXPORT void JNI_PrefetchTestBridge_SkipNTPSuggestionsAPIKeyCheck(
    JNIEnv* env) {
  ntp_snippets::RemoteSuggestionsFetcherImpl::
      set_skip_api_key_check_for_testing();
}

JNI_EXPORT void JNI_PrefetchTestBridge_InsertIntoCachedImageFetcher(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jbyteArray>& j_image_data) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  DCHECK(profile);
  image_fetcher::ImageFetcherService* service =
      ImageFetcherServiceFactory::GetForKey(profile->GetProfileKey());
  DCHECK(service);
  scoped_refptr<image_fetcher::ImageCache> cache =
      service->ImageCacheForTesting();
  std::string url = base::android::ConvertJavaStringToUTF8(env, j_url);
  std::string image_data;
  base::android::JavaByteArrayToString(env, j_image_data, &image_data);

  cache->SaveImage(url, image_data);
}

}  // namespace prefetch
}  // namespace offline_pages
