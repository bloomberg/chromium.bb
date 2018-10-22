// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/renderer/content_previews_render_frame_observer.h"

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace data_reduction_proxy {

namespace {

bool HasEmptyImageDirective(const blink::WebURLResponse& web_url_response) {
  std::string chrome_proxy_value =
      web_url_response
          .HttpHeaderField(blink::WebString::FromUTF8(chrome_proxy_header()))
          .Utf8();
  for (const auto& directive :
       base::SplitStringPiece(chrome_proxy_value, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (!base::StartsWith(directive, page_policies_directive(),
                          base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }

    // Check policy directive for empty-image entry.
    base::StringPiece page_policies_value = base::StringPiece(directive).substr(
        strlen(page_policies_directive()) + 1);
    for (const auto& policy :
         base::SplitStringPiece(page_policies_value, "|", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY)) {
      if (base::LowerCaseEqualsASCII(policy, empty_image_directive())) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

ContentPreviewsRenderFrameObserver::ContentPreviewsRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

ContentPreviewsRenderFrameObserver::~ContentPreviewsRenderFrameObserver() =
    default;

// Static
bool ContentPreviewsRenderFrameObserver::ValidatePreviewsStateWithResponse(
    content::PreviewsState previews_state,
    const blink::WebURLResponse& web_url_response) {
  bool has_lite_page_state = previews_state & content::SERVER_LITE_PAGE_ON;
  bool has_lite_page_directive =
      web_url_response
          .HttpHeaderField(blink::WebString::FromUTF8(
              chrome_proxy_content_transform_header()))
          .Utf8() == lite_page_directive();
  DCHECK_EQ(has_lite_page_state, has_lite_page_directive)
      << "Inconsistent PreviewsState ServerLitePage:" << has_lite_page_state
      << " header:" << has_lite_page_directive;
  bool has_lofi_state = previews_state & content::SERVER_LOFI_ON;
  bool has_empty_image_directive = HasEmptyImageDirective(web_url_response);
  DCHECK_EQ(has_lofi_state, has_empty_image_directive)
      << "Inconsistent PreviewsState ServerLoFi:" << has_lofi_state
      << " header:" << has_empty_image_directive;

  return (has_lite_page_state == has_lite_page_directive) &&
         (has_lofi_state == has_empty_image_directive);
}

void ContentPreviewsRenderFrameObserver::OnDestruct() {
  delete this;
}

void ContentPreviewsRenderFrameObserver::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_document_navigation) {
  if (is_same_document_navigation)
    return;

  content::PreviewsState previews_state = render_frame()->GetPreviewsState();
  if (previews_state == 0 || previews_state == content::PREVIEWS_OFF)
    return;

  const blink::WebURLResponse& web_url_response =
      render_frame()->GetWebFrame()->GetDocumentLoader()->GetResponse();
  // TODO(dougarnett): Remove this once proven stable to complete
  // crbug.com/782922.
  DCHECK(ValidatePreviewsStateWithResponse(previews_state, web_url_response));
}

}  // namespace data_reduction_proxy
