// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubresourceFilter_h
#define SubresourceFilter_h

#include <memory>

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class DocumentLoader;
class KURL;
class WebDocumentSubresourceFilter;

// Wrapper around a WebDocumentSubresourceFilter. This class will make it easier
// to extend the subresource filter with optimizations only possible using blink
// types (e.g. a caching layer using StringImpl).
class CORE_EXPORT SubresourceFilter final
    : public GarbageCollectedFinalized<SubresourceFilter> {
 public:
  static SubresourceFilter* create(
      DocumentLoader*,
      std::unique_ptr<WebDocumentSubresourceFilter>);
  ~SubresourceFilter();

  bool allowLoad(const KURL& resourceUrl,
                 WebURLRequest::RequestContext,
                 SecurityViolationReportingPolicy);

  DEFINE_INLINE_TRACE() { visitor->trace(m_documentLoader); }

 private:
  SubresourceFilter(DocumentLoader*,
                    std::unique_ptr<WebDocumentSubresourceFilter>);

  Member<DocumentLoader> m_documentLoader;
  std::unique_ptr<WebDocumentSubresourceFilter> m_subresourceFilter;
};

}  // namespace blink

#endif  // SubresourceFilter_h
