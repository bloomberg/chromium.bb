// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_navigation_params.h"

#include "third_party/blink/renderer/core/exported/web_document_loader_impl.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"

namespace blink {

WebNavigationParams::WebNavigationParams()
    : http_method(http_names::kGET),
      devtools_navigation_token(base::UnguessableToken::Create()) {}

WebNavigationParams::~WebNavigationParams() = default;

WebNavigationParams::WebNavigationParams(
    const base::UnguessableToken& devtools_navigation_token)
    : http_method(http_names::kGET),
      devtools_navigation_token(devtools_navigation_token) {}

// static
std::unique_ptr<WebNavigationParams> WebNavigationParams::CreateFromInfo(
    const WebNavigationInfo& info) {
  auto result = std::make_unique<WebNavigationParams>();
  result->url = info.url_request.Url();
  result->http_method = info.url_request.HttpMethod();
  result->cache_mode = info.url_request.GetCacheMode();
  result->referrer = info.url_request.HttpHeaderField(http_names::kReferer);
  result->referrer_policy = info.url_request.GetReferrerPolicy();
  result->http_body = info.url_request.HttpBody();
  result->http_content_type =
      info.url_request.HttpHeaderField(http_names::kContentType);
  result->previews_state = info.url_request.GetPreviewsState();
  result->requestor_origin = info.url_request.RequestorOrigin();
  result->frame_load_type = info.frame_load_type;
  result->is_client_redirect = info.is_client_redirect;
  result->navigation_timings.input_start = info.input_start;
  return result;
};

// static
std::unique_ptr<WebNavigationParams> WebNavigationParams::CreateWithHTMLString(
    base::span<const char> html,
    const WebURL& base_url) {
  auto result = std::make_unique<WebNavigationParams>();
  result->url = base_url;
  result->data = WebData(html.data(), html.size());
  result->mime_type = "text/html";
  result->text_encoding = "UTF-8";
  return result;
}

// static
std::unique_ptr<WebNavigationParams> WebNavigationParams::CreateForErrorPage(
    WebDocumentLoader* failed_document_loader,
    base::span<const char> html,
    const WebURL& base_url,
    const WebURL& unreachable_url) {
  auto result = WebNavigationParams::CreateWithHTMLString(html, base_url);
  result->unreachable_url = unreachable_url;
  // Locally generated error pages should not be cached.
  result->cache_mode = blink::mojom::FetchCacheMode::kNoStore;
  static_cast<WebDocumentLoaderImpl*>(failed_document_loader)
      ->FillNavigationParamsForErrorPage(result.get());
  return result;
}

#if INSIDE_BLINK
// static
std::unique_ptr<WebNavigationParams> WebNavigationParams::CreateWithHTMLBuffer(
    scoped_refptr<SharedBuffer> buffer,
    const KURL& base_url) {
  auto result = std::make_unique<WebNavigationParams>();
  result->url = base_url;
  result->data = WebData(std::move(buffer));
  result->mime_type = "text/html";
  result->text_encoding = "UTF-8";
  return result;
}
#endif

}  // namespace blink
