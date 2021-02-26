// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/lite_service_bridge.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AutofillAssistantLiteService_jni.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service/api_key_fetcher.h"
#include "components/autofill_assistant/browser/service/lite_service.h"
#include "components/autofill_assistant/browser/service/server_url_fetcher.h"
#include "components/autofill_assistant/browser/service/service_request_sender_impl.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

void OnFinished(base::android::ScopedJavaGlobalRef<jobject> java_lite_service,
                Metrics::LiteScriptFinishedState state) {
  VLOG(1) << "Lite service finished with state " << static_cast<int>(state);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillAssistantLiteService_onFinished(env, java_lite_service,
                                               static_cast<int>(state));
}

void OnScriptRunning(
    base::android::ScopedJavaGlobalRef<jobject> java_lite_service,
    bool ui_shown) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillAssistantLiteService_onScriptRunning(env, java_lite_service,
                                                    ui_shown);
}

// static
jlong JNI_AutofillAssistantLiteService_CreateLiteService(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_lite_service,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& jtrigger_script_path) {
  if (!java_lite_service || !jweb_contents || !jtrigger_script_path) {
    return 0;
  }

  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents) {
    return 0;
  }

  ServerUrlFetcher url_fetcher{ServerUrlFetcher::GetDefaultServerUrl()};
  return reinterpret_cast<jlong>(new LiteService(
      std::make_unique<ServiceRequestSenderImpl>(
          web_contents->GetBrowserContext(),
          /* access_token_fetcher = */ nullptr,
          std::make_unique<NativeURLLoaderFactory>(),
          ApiKeyFetcher().GetAPIKey(chrome::GetChannel()),
          /* auth_enabled = */ false,
          /* disable_auth_if_no_access_token = */ true),
      url_fetcher.GetNextActionsEndpoint(),
      base::android::ConvertJavaStringToUTF8(env, jtrigger_script_path),
      base::BindOnce(&OnFinished, base::android::ScopedJavaGlobalRef<jobject>(
                                      java_lite_service)),
      base::BindRepeating(
          &OnScriptRunning,
          base::android::ScopedJavaGlobalRef<jobject>(java_lite_service))));
}

}  // namespace autofill_assistant
