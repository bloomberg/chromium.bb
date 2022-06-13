// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_attributes.h"

#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace content {

void PrerenderAttributes::WriteIntoTrace(
    perfetto::TracedValue trace_context) const {
  auto dict = std::move(trace_context).WriteDictionary();
  dict.Add("url", prerendering_url);
  dict.Add("trigger_type", trigger_type);
}

PrerenderAttributes::PrerenderAttributes(
    const GURL& prerendering_url,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix,
    Referrer referrer,
    absl::optional<url::Origin> initiator_origin,
    const GURL& initiator_url,
    int initiator_process_id,
    absl::optional<blink::LocalFrameToken> initiator_frame_token,
    ukm::SourceId initiator_ukm_id,
    ui::PageTransition transition_type)
    : prerendering_url(prerendering_url),
      trigger_type(trigger_type),
      embedder_histogram_suffix(embedder_histogram_suffix),
      referrer(referrer),
      initiator_origin(std::move(initiator_origin)),
      initiator_url(initiator_url),
      initiator_process_id(initiator_process_id),
      initiator_frame_token(std::move(initiator_frame_token)),
      initiator_ukm_id(initiator_ukm_id),
      transition_type(transition_type) {}

PrerenderAttributes::~PrerenderAttributes() = default;

PrerenderAttributes::PrerenderAttributes(
    const PrerenderAttributes& attributes) = default;

PrerenderAttributes::PrerenderAttributes(PrerenderAttributes&& attributes)
    : prerendering_url(attributes.prerendering_url),
      trigger_type(attributes.trigger_type),
      embedder_histogram_suffix(attributes.embedder_histogram_suffix),
      referrer(attributes.referrer),
      initiator_origin(attributes.initiator_origin),
      initiator_url(attributes.initiator_url),
      initiator_process_id(attributes.initiator_process_id),
      initiator_frame_token(attributes.initiator_frame_token),
      initiator_ukm_id(attributes.initiator_ukm_id),
      transition_type(attributes.transition_type) {}

}  // namespace content
