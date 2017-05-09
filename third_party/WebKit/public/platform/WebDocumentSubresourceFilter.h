// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDocumentSubresourceFilter_h
#define WebDocumentSubresourceFilter_h

#include "public/platform/WebURLRequest.h"

namespace blink {

class WebURL;

class WebDocumentSubresourceFilter {
 public:
  enum LoadPolicy { kAllow, kDisallow, kWouldDisallow };

  virtual ~WebDocumentSubresourceFilter() {}
  virtual LoadPolicy GetLoadPolicy(const WebURL& resource_url,
                                   WebURLRequest::RequestContext) = 0;
  virtual LoadPolicy GetLoadPolicyForWebSocketConnect(const WebURL&) = 0;

  // Report that a resource loaded by the document (not a preload) was
  // disallowed.
  virtual void ReportDisallowedLoad() = 0;

  // Returns true if disallowed resource loads should be logged to the devtools
  // console.
  virtual bool ShouldLogToConsole() = 0;
};

}  // namespace blink

#endif  // WebDocumentSubresourceFilter_h
