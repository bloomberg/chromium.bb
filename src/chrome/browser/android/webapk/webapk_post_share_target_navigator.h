// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_POST_SHARE_TARGET_NAVIGATOR_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_POST_SHARE_TARGET_NAVIGATOR_H_

#include <string>

namespace content {
class WebContents;
}

class GURL;

namespace webapk {

// Return a string as a quoted value, escaping quotes and line breaks.
std::string PercentEscapeString(const std::string& unescaped_string);

// Compute and return multipart/form-data POST body for share target.
std::string ComputeMultipartBody(const std::vector<std::string>& names,
                                 const std::vector<std::string>& values,
                                 const std::vector<std::string>& filenames,
                                 const std::vector<std::string>& types,
                                 const std::string& boundary);

// Compute and return application/x-www-form-urlencoded POST body for share
// target.
std::string ComputeUrlEncodedBody(const std::vector<std::string>& names,
                                  const std::vector<std::string>& values);

// Navigate to share target gurl with post |body| and |header_list|.
void NavigateShareTargetPost(const std::string& body,
                             const std::string& header_list,
                             const GURL& share_target_gurl,
                             content::WebContents* web_contents);

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_POST_SHARE_TARGET_NAVIGATOR_H_
