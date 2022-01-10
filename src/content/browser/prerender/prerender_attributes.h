// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_ATTRIBUTES_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_ATTRIBUTES_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/browser/prerender_trigger_type.h"
#include "content/public/common/referrer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/base/page_transition_types.h"

namespace content {

// Records the basic attributes of a prerender request.
struct CONTENT_EXPORT PrerenderAttributes {
  PrerenderAttributes(
      const GURL& prerendering_url,
      PrerenderTriggerType trigger_type,
      const std::string& embedder_histogram_suffix,
      Referrer referrer,
      absl::optional<url::Origin> initiator_origin,
      const GURL& initiator_url,
      int initiator_process_id,
      absl::optional<blink::LocalFrameToken> initiator_frame_token,
      ukm::SourceId initiator_ukm_id,
      ui::PageTransition transition_type);
  ~PrerenderAttributes();
  PrerenderAttributes(const PrerenderAttributes&);
  PrerenderAttributes& operator=(const PrerenderAttributes&) = delete;
  PrerenderAttributes(PrerenderAttributes&&);
  PrerenderAttributes& operator=(PrerenderAttributes&&) = delete;

  bool IsBrowserInitiated() const { return !initiator_origin.has_value(); }

  GURL prerendering_url;

  PrerenderTriggerType trigger_type;

  // Used for kEmbedder trigger type to avoid exposing information of embedders
  // to content/. Only used for metrics.
  std::string embedder_histogram_suffix;

  Referrer referrer;

  // This is absl::nullopt when prerendering is initiated by the browser
  // (not by a renderer using Speculation Rules API).
  absl::optional<url::Origin> initiator_origin;

  GURL initiator_url;

  // This is ChildProcessHost::kInvalidUniqueID when prerendering is initiated
  // by the browser.
  int initiator_process_id;

  // This is absl::nullopt when prerendering is initiated by the browser.
  absl::optional<blink::LocalFrameToken> initiator_frame_token;

  // This is ukm::kInvalidSourceId when prerendering is initiated by the
  // browser.
  ukm::SourceId initiator_ukm_id;

  ui::PageTransition transition_type;

  // Serialises this struct into a trace.
  void WriteIntoTrace(perfetto::TracedValue trace_context) const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_ATTRIBUTES_H_
