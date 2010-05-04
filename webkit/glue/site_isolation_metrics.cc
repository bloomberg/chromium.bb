// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/site_isolation_metrics.h"

#include <set>

#include "base/hash_tables.h"
#include "base/histogram.h"
#include "net/base/mime_sniffer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLResponse.h"

using WebKit::WebFrame;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

namespace webkit_glue {

typedef base::hash_map<unsigned, WebURLRequest::TargetType> TargetTypeMap;
typedef base::hash_map<std::string, int> MimeTypeMap;
typedef std::set<std::string> CrossOriginTextHtmlResponseSet;

static TargetTypeMap* GetTargetTypeMap() {
  static TargetTypeMap target_type_map_;
  return &target_type_map_;
}

// Copied from net/base/mime_util.cc, supported_non_image_types[]
static const char* const kCrossOriginMimeTypesToLog[] = {
  "text/cache-manifest",
  "text/html",
  "text/xml",
  "text/xsl",
  "text/plain",
  "text/vnd.chromium.ftp-dir",
  "text/",
  "text/css",
  "image/svg+xml",
  "application/xml",
  "application/xhtml+xml",
  "application/rss+xml",
  "application/atom+xml",
  "application/json",
  "application/x-x509-user-cert",
  "multipart/x-mixed-replace"
};

static MimeTypeMap* GetMimeTypeMap() {
  static MimeTypeMap mime_type_map_;
  if (!mime_type_map_.size()) {
    for (size_t i = 0; i < arraysize(kCrossOriginMimeTypesToLog); ++i)
      mime_type_map_[kCrossOriginMimeTypesToLog[i]] = i;
  }
  return &mime_type_map_;
}

// This is set is used to keep track of the response urls that we want to
// sniff, since we will have to wait for the payload to arrive.
static CrossOriginTextHtmlResponseSet* GetCrossOriginTextHtmlResponseSet() {
  static CrossOriginTextHtmlResponseSet cross_origin_text_html_response_set_;
  return &cross_origin_text_html_response_set_;
}

static void LogVerifiedTextHtmlResponse() {
  UMA_HISTOGRAM_COUNTS(
      "SiteIsolation.CrossSiteNonFrameResponse_verified_texthtml", 1);
}

static void LogMislabeledTextHtmlResponse() {
  UMA_HISTOGRAM_COUNTS(
      "SiteIsolation.CrossSiteNonFrameResponse_mislabeled_texthtml", 1);
}

void SiteIsolationMetrics::AddRequest(unsigned identifier,
    WebURLRequest::TargetType target_type) {
  TargetTypeMap& target_type_map = *GetTargetTypeMap();
  target_type_map[identifier] = target_type;
}

void SiteIsolationMetrics::LogMimeTypeForCrossOriginRequest(
    WebFrame* frame, unsigned identifier, const WebURLResponse& response) {
  TargetTypeMap& target_type_map = *GetTargetTypeMap();
  TargetTypeMap::iterator iter  = target_type_map.find(identifier);
  if (iter != target_type_map.end()) {
    WebURLRequest::TargetType target_type = iter->second;
    target_type_map.erase(iter);
    // We want to log any cross-site request that we don't think a renderer
    // should be allowed to make. We can safely ignore frame requests (since
    // we'd like those to be in a separate renderer) and plugin requests, even
    // if they are cross-origin.
    if (target_type != WebURLRequest::TargetIsMainFrame &&
        target_type != WebURLRequest::TargetIsSubFrame &&
        target_type != WebURLRequest::TargetIsObject &&
        !frame->securityOrigin().canAccess(
            WebSecurityOrigin::create(response.url()))) {
      std::string mime_type = response.mimeType().utf8();
      MimeTypeMap mime_type_map = *GetMimeTypeMap();
      MimeTypeMap::iterator mime_type_iter = mime_type_map.find(mime_type);
      if (mime_type_iter != mime_type_map.end()) {
        UMA_HISTOGRAM_ENUMERATION(
            "SiteIsolation.CrossSiteNonFrameResponse_MIME_Type",
            mime_type_iter->second,
            arraysize(kCrossOriginMimeTypesToLog));

        // We also should check access control headers, in case this
        // cross-origin request has been explicitly permitted.
        // This is basically a copy of the logic of passesAccessControlCheck()
        // in WebCore/loader/CrossOriginAccessControl.cpp.
        WebString access_control_origin = response.httpHeaderField(
            WebString::fromUTF8("Access-Control-Allow-Origin"));
        if (access_control_origin != WebString::fromUTF8("*")
            && !frame->securityOrigin().canAccess(
                WebSecurityOrigin::createFromString(access_control_origin))) {
          UMA_HISTOGRAM_ENUMERATION(
              "SiteIsolation.CrossSiteNonFrameResponse_With_CORS_MIME_Type",
              mime_type_iter->second,
              arraysize(kCrossOriginMimeTypesToLog));
        }

        // Sometimes resources are mislabeled as text/html. Once we have some
        // of the payload, we want to determine if this was actually text/html.
        if (mime_type == "text/html")
          GetCrossOriginTextHtmlResponseSet()->insert(response.url().spec());
      }
    }
  }
}

void SiteIsolationMetrics::SniffCrossOriginHTML(const WebURL& response_url,
                                                const char* data,
                                                int len) {
  if (!response_url.isValid())
    return;

  CrossOriginTextHtmlResponseSet& cross_origin_text_html_response_set =
      *GetCrossOriginTextHtmlResponseSet();
  CrossOriginTextHtmlResponseSet::iterator request_iter =
      cross_origin_text_html_response_set.find(response_url.spec());
  if (request_iter != cross_origin_text_html_response_set.end()) {
    std::string sniffed_mime_type;
    bool successful = net::SniffMimeType(data, len, response_url,
                                         "", &sniffed_mime_type);
    if (successful && sniffed_mime_type == "text/html")
      LogVerifiedTextHtmlResponse();
    else
      LogMislabeledTextHtmlResponse();
    cross_origin_text_html_response_set.erase(request_iter);
  }
}

void SiteIsolationMetrics::RemoveCompletedResponse(
    const WebURL& response_url) {
  if (!response_url.isValid())
    return;

  // Ensure we don't leave responses in the set after they've completed.
  CrossOriginTextHtmlResponseSet& cross_origin_text_html_response_set =
      *GetCrossOriginTextHtmlResponseSet();
  CrossOriginTextHtmlResponseSet::iterator request_iter =
      cross_origin_text_html_response_set.find(response_url.spec());
  if (request_iter != cross_origin_text_html_response_set.end()) {
    LogMislabeledTextHtmlResponse();
    cross_origin_text_html_response_set.erase(request_iter);
  }
}

}  // namespace webkit_glue
