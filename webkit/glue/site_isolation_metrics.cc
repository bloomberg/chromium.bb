// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/site_isolation_metrics.h"

#include <set>

#include "base/hash_tables.h"
#include "base/metrics/histogram.h"
#include "net/base/mime_sniffer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"

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
  "multipart/x-mixed-replace",
  "(NONE)"  // Keep track of missing MIME types as well
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
      "SiteIsolation.CrossSiteNonFrameResponse_verified_texthtml_BLOCK", 1);
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

// Check whether the given response is allowed due to access control headers.
// This is basically a copy of the logic of passesAccessControlCheck() in
// WebCore/loader/CrossOriginAccessControl.cpp.
bool SiteIsolationMetrics::AllowedByAccessControlHeader(
    WebFrame* frame, const WebURLResponse& response) {
  WebString access_control_origin = response.httpHeaderField(
      WebString::fromUTF8("Access-Control-Allow-Origin"));
  WebSecurityOrigin security_origin =
      WebSecurityOrigin::createFromString(access_control_origin);
  return access_control_origin == WebString::fromUTF8("*") ||
         frame->securityOrigin().canAccess(security_origin);
}

// We want to log any cross-site request that we don't think a renderer should
// be allowed to make. We can safely ignore frame requests (since we'd like
// those to be in a separate renderer) and plugin requests, even if they are
// cross-origin.
//
// For comparison, we keep counts of:
//  - All requests made by a renderer
//  - All cross-site requests
//
// Then, for cross-site non-frame/plugin requests, we keep track of:
//  - Counts for MIME types of interest
//  - Counts of those MIME types that carry CORS headers
//  - Counts of mislabeled text/html responses (without CORS)
// As well as those we would block:
//  - Counts of verified text/html responses (without CORS)
//  - Counts of XML/JSON responses (without CORS)
//
// This will let us say what percentage of requests we would end up blocking.
void SiteIsolationMetrics::LogMimeTypeForCrossOriginRequest(
    WebFrame* frame, unsigned identifier, const WebURLResponse& response) {
  UMA_HISTOGRAM_COUNTS("SiteIsolation.Requests", 1);

  TargetTypeMap& target_type_map = *GetTargetTypeMap();
  TargetTypeMap::iterator iter  = target_type_map.find(identifier);
  if (iter != target_type_map.end()) {
    WebURLRequest::TargetType target_type = iter->second;
    target_type_map.erase(iter);

    // Focus on cross-site requests.
    if (!frame->securityOrigin().canAccess(
            WebSecurityOrigin::create(response.url()))) {
      UMA_HISTOGRAM_COUNTS("SiteIsolation.CrossSiteRequests", 1);

      // Now focus on non-frame, non-plugin requests.
      if (target_type != WebURLRequest::TargetIsMainFrame &&
          target_type != WebURLRequest::TargetIsSubframe &&
          target_type != WebURLRequest::TargetIsObject) {
        // If it is part of a MIME type we might block, log the MIME type.
        std::string mime_type = response.mimeType().utf8();
        MimeTypeMap mime_type_map = *GetMimeTypeMap();
        // Also track it if it lacks a MIME type.
        // TODO(creis): 304 responses have no MIME type, so we don't handle
        // them correctly.  Can we look up their MIME type from the cache?
        if (mime_type == "")
          mime_type = "(NONE)";
        MimeTypeMap::iterator mime_type_iter = mime_type_map.find(mime_type);
        if (mime_type_iter != mime_type_map.end()) {
          UMA_HISTOGRAM_ENUMERATION(
              "SiteIsolation.CrossSiteNonFrameResponse_MIME_Type",
              mime_type_iter->second,
              arraysize(kCrossOriginMimeTypesToLog));

          // We also check access control headers, in case this
          // cross-origin request has been explicitly permitted.
          if (AllowedByAccessControlHeader(frame, response)) {
            UMA_HISTOGRAM_ENUMERATION(
                "SiteIsolation.CrossSiteNonFrameResponse_With_CORS_MIME_Type",
                mime_type_iter->second,
                arraysize(kCrossOriginMimeTypesToLog));
          } else {
            // Without access control headers, we might block this request.
            // Sometimes resources are mislabled as text/html, though, and we
            // should only block them if we can verify that.  To do so, we sniff
            // the content once we have some of the payload.
            if (mime_type == "text/html") {
              // Remember the response until we can sniff its contents.
              GetCrossOriginTextHtmlResponseSet()->insert(
                  response.url().spec());
            } else if (mime_type == "text/xml" ||
                       mime_type == "text/xsl" ||
                       mime_type == "application/xml" ||
                       mime_type == "application/xhtml+xml" ||
                       mime_type == "application/rss+xml" ||
                       mime_type == "application/atom+xml" ||
                       mime_type == "application/json") {
              // We will also block XML and JSON MIME types for cross-site
              // non-frame requests without CORS headers.
              UMA_HISTOGRAM_COUNTS(
                  "SiteIsolation.CrossSiteNonFrameResponse_xml_or_json_BLOCK",
                  1);
            }
          }
        }
      }
    }
  }
}

void SiteIsolationMetrics::SniffCrossOriginHTML(const WebURL& response_url,
                                                const char* data,
                                                int len) {
  if (!response_url.isValid())
    return;

  // Look up the URL to see if it is a text/html request we are tracking.
  CrossOriginTextHtmlResponseSet& cross_origin_text_html_response_set =
      *GetCrossOriginTextHtmlResponseSet();
  CrossOriginTextHtmlResponseSet::iterator request_iter =
      cross_origin_text_html_response_set.find(response_url.spec());
  if (request_iter != cross_origin_text_html_response_set.end()) {
    // Log whether it actually looks like HTML.
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
