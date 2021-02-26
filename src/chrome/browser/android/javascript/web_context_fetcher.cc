// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/WebContextFetcher_jni.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

// The JS execution function returns the JSON object as a quoted string literal.
// Remove the surrounding quotes and the internal escaping, to convert it into a
// JSON object that can be parsed. E.g.:
// "{\"foo\":\"bar\"}" --> {"foo":"bar"}
static std::string ConvertJavascriptOutputToValidJson(std::string& json) {
  // Remove trailing and leading double quotation characters.
  std::string trimmed_json = json.substr(1, json.size() - 2);

  // Remove escape slash from before quotations.
  std::string substring_to_search = "\\\"";
  std::string replace_str = "\"";
  size_t pos = trimmed_json.find(substring_to_search);
  // Repeat till end is reached.
  while (pos != std::string::npos) {
    // Remove occurrence of the escape character before quotes.
    trimmed_json.replace(pos, substring_to_search.size(), replace_str);
    // Get the next occurrence from the current position.
    pos = trimmed_json.find(substring_to_search, pos + replace_str.size());
  }
  return trimmed_json;
}

static void OnContextFetchComplete(
    const ScopedJavaGlobalRef<jobject>& scoped_jcallback,
    base::TimeTicks javascript_start,
    base::Value result) {
  if (!javascript_start.is_null()) {
    base::TimeDelta javascript_time = base::TimeTicks::Now() - javascript_start;
    // TODO(benwgold): Update so that different js scripts can monitor execution
    // separately.
    base::UmaHistogramTimes("WebContextFetcher.JavaScriptRunner.ExecutionTime",
                            javascript_time);
    DVLOG(1) << "WebContextFetcher.JavaScriptRunner.ExecutionTime = "
             << javascript_time;
  }
  std::string json;
  base::JSONWriter::Write(result, &json);
  base::android::RunStringCallbackAndroid(
      scoped_jcallback, ConvertJavascriptOutputToValidJson(json));
}

// IMPORTANT: The output of this fetch should only be handled in memory safe
//      languages (Java) and should not be parsed in C++.
static void ExecuteFetch(const base::string16& script,
                         const ScopedJavaGlobalRef<jobject>& scoped_jcallback,
                         content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);
  // TODO(benwgold): Consider adding handling for cases when the document is not
  // yet ready.
  base::OnceCallback<void(base::Value)> callback = base::BindOnce(
      &OnContextFetchComplete, scoped_jcallback, base::TimeTicks::Now());
  render_frame_host->ExecuteJavaScriptInIsolatedWorld(
      script, std::move(callback), ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

static void JNI_WebContextFetcher_FetchContextWithJavascript(
    JNIEnv* env,
    const JavaParamRef<jstring>& jscript,
    const JavaParamRef<jobject>& jcallback,
    const JavaParamRef<jobject>& jrender_frame_host) {
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jrender_frame_host);
  base::string16 script = base::android::ConvertJavaStringToUTF16(env, jscript);
  ScopedJavaGlobalRef<jobject> scoped_jcallback(env, jcallback);
  ExecuteFetch(script, scoped_jcallback, render_frame_host);
}
