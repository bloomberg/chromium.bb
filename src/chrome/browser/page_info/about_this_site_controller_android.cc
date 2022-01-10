// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/chrome_jni_headers/PageInfoAboutThisSiteController_jni.h"

#include <jni.h>
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/page_info/about_this_site_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"

static base::android::ScopedJavaLocalRef<jbyteArray>
JNI_PageInfoAboutThisSiteController_GetSiteInfo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_browserContext,
    const base::android::JavaParamRef<jobject>& j_url,
    const base::android::JavaParamRef<jobject>& j_webContents) {
  Profile* profile = Profile::FromBrowserContext(
      content::BrowserContextFromJavaHandle(j_browserContext));
  auto* service = AboutThisSiteServiceFactory::GetForProfile(profile);
  if (!service)
    return nullptr;
  auto url = url::GURLAndroid::ToNativeGURL(env, j_url);
  auto source_id = ukm::GetSourceIdForWebContentsDocument(
      content::WebContents::FromJavaWebContents(j_webContents));
  auto info = service->GetAboutThisSiteInfo(*url, source_id);
  if (!info)
    return nullptr;

  // Serialize the proto to pass it to Java. This will copy the whole object
  // but it only contains a few strings and ints and this method is called only
  // when PageInfo is opened.
  int size = info->ByteSize();
  std::vector<uint8_t> data(size);
  info->SerializeToArray(data.data(), size);
  return base::android::ToJavaByteArray(env, data.data(), size);
}
