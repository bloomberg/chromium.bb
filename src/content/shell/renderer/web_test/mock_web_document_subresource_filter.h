// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_MOCK_WEB_DOCUMENT_SUBRESOURCE_FILTER_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_MOCK_WEB_DOCUMENT_SUBRESOURCE_FILTER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace blink {
class WebURL;
}  // namespace blink

namespace content {

class MockWebDocumentSubresourceFilter
    : public blink::WebDocumentSubresourceFilter {
 public:
  explicit MockWebDocumentSubresourceFilter(
      const std::vector<std::string>& disallowed_path_suffixes,
      bool block_resources);
  ~MockWebDocumentSubresourceFilter() override;

  // blink::WebDocumentSubresourceFilter:
  LoadPolicy GetLoadPolicy(const blink::WebURL& resource_url,
                           blink::mojom::RequestContextType) override;
  LoadPolicy GetLoadPolicyForWebSocketConnect(
      const blink::WebURL& url) override;
  void ReportDisallowedLoad() override;
  bool ShouldLogToConsole() override;

 private:
  LoadPolicy getLoadPolicyImpl(const blink::WebURL& url);
  std::vector<std::string> disallowed_path_suffixes_;
  bool block_subresources_;

  DISALLOW_COPY_AND_ASSIGN(MockWebDocumentSubresourceFilter);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_MOCK_WEB_DOCUMENT_SUBRESOURCE_FILTER_H_
