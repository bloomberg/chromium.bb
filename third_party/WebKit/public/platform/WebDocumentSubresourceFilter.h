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
  enum LoadPolicy { Allow, Disallow, WouldDisallow };
  virtual ~WebDocumentSubresourceFilter() {}
  virtual LoadPolicy getLoadPolicy(const WebURL& resourceUrl,
                                   WebURLRequest::RequestContext) = 0;

  // Report that a resource loaded by the document (not a preload) was
  // disallowed.
  virtual void reportDisallowedLoad() = 0;
};

}  // namespace blink

#endif  // WebDocumentSubresourceFilter_h
