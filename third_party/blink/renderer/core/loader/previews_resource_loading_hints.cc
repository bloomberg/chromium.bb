// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// static
PreviewsResourceLoadingHints* PreviewsResourceLoadingHints::Create(
    ExecutionContext& execution_context,
    const std::vector<WTF::String>& subresource_patterns_to_block) {
  return new PreviewsResourceLoadingHints(&execution_context,
                                          subresource_patterns_to_block);
}

PreviewsResourceLoadingHints::PreviewsResourceLoadingHints(
    ExecutionContext* execution_context,
    const std::vector<WTF::String>& subresource_patterns_to_block)
    : execution_context_(execution_context),
      subresource_patterns_to_block_(subresource_patterns_to_block) {}

PreviewsResourceLoadingHints::~PreviewsResourceLoadingHints() = default;

bool PreviewsResourceLoadingHints::AllowLoad(const KURL& resource_url) const {
  if (!resource_url.ProtocolIsInHTTPFamily())
    return true;

  WTF::String resource_url_string = resource_url.GetString();
  resource_url_string = resource_url_string.Left(resource_url.PathEnd());

  for (const WTF::String& subresource_pattern :
       subresource_patterns_to_block_) {
    // TODO(tbansal): https://crbug.com/856247. Add support for wildcard
    // matching.
    if (resource_url_string.Find(subresource_pattern) != kNotFound)
      return false;
  }

  return true;
}

void PreviewsResourceLoadingHints::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
}

}  // namespace blink
