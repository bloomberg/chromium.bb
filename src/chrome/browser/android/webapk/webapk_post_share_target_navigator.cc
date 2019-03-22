// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_post_share_target_navigator.h"

#include <sstream>

#include <jni.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "content/public/browser/web_contents.h"
#include "jni/WebApkPostShareTargetNavigator_jni.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

namespace webapk {

std::string PercentEscapeString(const std::string& unescaped_string) {
  std::ostringstream escaped_oss;
  for (size_t i = 0; i < unescaped_string.length(); ++i) {
    if (unescaped_string[i] == '"') {
      escaped_oss << "%22";
    } else if (unescaped_string[i] == 0x0a) {
      escaped_oss << "%0A";
    } else if (unescaped_string[i] == 0x0d) {
      escaped_oss << "%0D";
    } else {
      escaped_oss << unescaped_string[i];
    }
  }
  return escaped_oss.str();
}

std::string ComputeMultipartBody(const std::vector<std::string>& names,
                                 const std::vector<std::string>& values,
                                 const std::vector<std::string>& filenames,
                                 const std::vector<std::string>& types,
                                 const std::string& boundary) {
  size_t num_files = names.size();
  if (num_files != values.size() || num_files != filenames.size() ||
      num_files != types.size()) {
    // The length of |names|, |values|, |filenames|, and |types| should always
    // be the same for multipart POST. This should never happen.
    return "";
  }

  std::string body;
  for (size_t i = 0; i < num_files; i++)
    net::AddMultipartValueForUploadWithFileName(
        PercentEscapeString(names[i]), PercentEscapeString(filenames[i]),
        values[i], boundary, types[i], &body);
  net::AddMultipartFinalDelimiterForUpload(boundary, &body);
  return body;
}

std::string ComputeUrlEncodedBody(const std::vector<std::string>& names,
                                  const std::vector<std::string>& values) {
  if (names.size() != values.size() || names.size() == 0)
    return "";
  std::ostringstream application_body_oss;
  application_body_oss << net::EscapeUrlEncodedData(names[0], true) << "="
                       << net::EscapeUrlEncodedData(values[0], true);
  for (size_t i = 1; i < names.size(); i++)
    application_body_oss << "&" << net::EscapeUrlEncodedData(names[i], true)
                         << "=" << net::EscapeUrlEncodedData(values[i], true);

  return application_body_oss.str();
}

void NavigateShareTargetPost(const std::string& body,
                             const std::string& header_list,
                             const GURL& share_target_gurl,
                             content::WebContents* web_contents) {
  content::OpenURLParams open_url_params(
      share_target_gurl, content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      false /* is_renderer_initiated */);
  open_url_params.post_data = network::ResourceRequestBody::CreateFromBytes(
      body.c_str(), body.length());
  open_url_params.uses_post = true;
  open_url_params.extra_headers = header_list;
  web_contents->OpenURL(open_url_params);
}

}  // namespace webapk

void JNI_WebApkPostShareTargetNavigator_LoadViewForShareTargetPost(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const jboolean java_is_multipart_encoding,
    const JavaParamRef<jobjectArray>& java_names,
    const JavaParamRef<jobjectArray>& java_values,
    const JavaParamRef<jobjectArray>& java_filenames,
    const JavaParamRef<jobjectArray>& java_types,
    const JavaParamRef<jstring>& java_url,
    const JavaParamRef<jobject>& java_web_contents) {
  std::vector<std::string> names;
  std::vector<std::string> values;
  std::vector<std::string> filenames;
  std::vector<std::string> types;

  bool is_multipart_encoding = static_cast<bool>(java_is_multipart_encoding);
  base::android::AppendJavaStringArrayToStringVector(env, java_names, &names);
  base::android::JavaArrayOfByteArrayToStringVector(env, java_values, &values);
  base::android::AppendJavaStringArrayToStringVector(env, java_filenames,
                                                     &filenames);
  base::android::AppendJavaStringArrayToStringVector(env, java_types, &types);

  GURL share_target_gurl(base::android::ConvertJavaStringToUTF8(java_url));

  std::string body;
  std::string header_list;

  if (is_multipart_encoding) {
    std::string boundary = net::GenerateMimeMultipartBoundary();
    body =
        webapk::ComputeMultipartBody(names, values, filenames, types, boundary);
    header_list = base::StringPrintf(
        "Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
  } else {
    body = webapk::ComputeUrlEncodedBody(names, values);
    header_list = "Content-Type: application/x-www-form-urlencoded\r\n";
  }

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  webapk::NavigateShareTargetPost(body, header_list, share_target_gurl,
                                  web_contents);
}
