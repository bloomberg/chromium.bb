// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_types.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

String GetConsoleLogStringForBlockedLoad(const KURL& url) {
  return "[Intervention] Non-critical resource " + url.GetString() +
         " is blocked due to page load being slow. Learn more at "
         "https://www.chromestatus.com/feature/4510564810227712.";
}

}  // namespace

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

bool PreviewsResourceLoadingHints::AllowLoad(
    const KURL& resource_url,
    ResourceLoadPriority resource_load_priority) const {
  if (!resource_url.ProtocolIsInHTTPFamily())
    return true;

  WTF::String resource_url_string = resource_url.GetString();
  resource_url_string = resource_url_string.Left(resource_url.PathEnd());
  bool allow_load = true;

  for (const WTF::String& subresource_pattern :
       subresource_patterns_to_block_) {
    // TODO(tbansal): https://crbug.com/856247. Add support for wildcard
    // matching.
    if (resource_url_string.Find(subresource_pattern) != kNotFound) {
      allow_load = false;
      break;
    }
  }

  UMA_HISTOGRAM_BOOLEAN("ResourceLoadingHints.ResourceLoadingBlocked",
                        !allow_load);
  if (!allow_load) {
    ReportBlockedLoading(resource_url);
    UMA_HISTOGRAM_ENUMERATION(
        "ResourceLoadingHints.ResourceLoadingBlocked.ResourceLoadPriority."
        "Blocked",
        resource_load_priority,
        static_cast<int>(ResourceLoadPriority::kHighest) + 1);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "ResourceLoadingHints.ResourceLoadingBlocked.ResourceLoadPriority."
        "Allowed",
        resource_load_priority,
        static_cast<int>(ResourceLoadPriority::kHighest) + 1);
  }
  return allow_load;
}

void PreviewsResourceLoadingHints::ReportBlockedLoading(
    const KURL& resource_url) const {
  execution_context_->AddConsoleMessage(
      ConsoleMessage::Create(kOtherMessageSource, kWarningMessageLevel,
                             GetConsoleLogStringForBlockedLoad(resource_url)));
}

void PreviewsResourceLoadingHints::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
}

}  // namespace blink
