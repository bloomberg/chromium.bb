// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"

namespace blink {

WebNavigationParams::WebNavigationParams()
    : devtools_navigation_token(base::UnguessableToken::Create()) {}

WebNavigationParams::~WebNavigationParams() = default;

WebNavigationParams::WebNavigationParams(
    const base::UnguessableToken& devtools_navigation_token)
    : devtools_navigation_token(devtools_navigation_token) {}

// static
std::unique_ptr<WebNavigationParams> WebNavigationParams::CreateFromInfo(
    const WebNavigationInfo& info) {
  auto result = std::make_unique<WebNavigationParams>();
  result->request = info.url_request;
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
  result->request = WebURLRequest(base_url);
  result->data = WebData(html.data(), html.size());
  result->mime_type = "text/html";
  result->text_encoding = "UTF-8";
  return result;
}

#if INSIDE_BLINK
// static
std::unique_ptr<WebNavigationParams> WebNavigationParams::CreateWithHTMLBuffer(
    scoped_refptr<SharedBuffer> buffer,
    const KURL& base_url) {
  auto result = std::make_unique<WebNavigationParams>();
  result->request = WebURLRequest(base_url);
  result->data = WebData(std::move(buffer));
  result->mime_type = "text/html";
  result->text_encoding = "UTF-8";
  return result;
}
#endif

}  // namespace blink
