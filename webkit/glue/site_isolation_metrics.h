// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_SITE_ISOLATION_METRICS_H_
#define WEBKIT_GLUE_SITE_ISOLATION_METRICS_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"

namespace WebKit {
class WebFrame;
class WebURL;
class WebURLResponse;
}

namespace webkit_glue {

// Metrics to check the feasability of blocking cross-site requests that
// a renderer shouldn't be making (in case we try to move cross-site frames
// into their own process someday). We're erring on the side of counting more
// mime-types then we strictly need (we'd only consider blocking cross-site
// requests with types similar to HTML, XML, or JSON).
class SiteIsolationMetrics {
 public:
  static void AddRequest(unsigned identifier,
                         WebKit::WebURLRequest::TargetType target_type);
  static bool AllowedByAccessControlHeader(
      WebKit::WebFrame* frame, const WebKit::WebURLResponse& response);
  static void LogMimeTypeForCrossOriginRequest(
      WebKit::WebFrame* frame,
      unsigned identifier,
      const WebKit::WebURLResponse& response);
  static void SniffCrossOriginHTML(const WebKit::WebURL& response_url,
                                   const char* data,
                                   int len);
  static void RemoveCompletedResponse(const WebKit::WebURL& response_url);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_SITE_ISOLATION_METRICS_H_
