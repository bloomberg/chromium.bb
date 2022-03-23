// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_host_utils.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "url/origin.h"

namespace content {

namespace {

proto::AttributionAggregatableSource ConvertToProto(
    const blink::mojom::AttributionAggregatableSource& aggregatable_source) {
  proto::AttributionAggregatableSource result;

  for (const auto& [key_id, key_ptr] : aggregatable_source.keys) {
    DCHECK(key_ptr);
    proto::AttributionAggregatableKey key;
    key.set_high_bits(key_ptr->high_bits);
    key.set_low_bits(key_ptr->low_bits);

    (*result.mutable_keys())[key_id] = std::move(key);
  }

  return result;
}

}  // namespace

AttributionDataHostManagerImpl::AttributionDataHostManagerImpl(
    AttributionManager* attribution_manager)
    : attribution_manager_(attribution_manager) {
  DCHECK(attribution_manager_);

  // It's safe to use `base::Unretained()` as `receivers_` is owned by `this`
  // and will be deleted before `this`.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &AttributionDataHostManagerImpl::OnDataHostDisconnected,
      base::Unretained(this)));
}

AttributionDataHostManagerImpl::~AttributionDataHostManagerImpl() = default;

void AttributionDataHostManagerImpl::RegisterDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    url::Origin context_origin) {
  receivers_.Add(this, std::move(data_host),
                 FrozenContext{.context_origin = std::move(context_origin),
                               .source_type = AttributionSourceType::kEvent});
}

void AttributionDataHostManagerImpl::SourceDataAvailable(
    blink::mojom::AttributionSourceDataPtr data) {
  // TODO(linnan): Log metrics for early returns.

  if (data->destination.opaque())
    return;

  auto [it, inserted] =
      receiver_data_.emplace(receivers_.current_receiver(), data->destination);
  if (!inserted && data->destination != it->second)
    return;

  const FrozenContext& context = receivers_.current_context();
  base::Time source_time = base::Time::Now();
  const url::Origin& reporting_origin = data->reporting_origin;

  // The API is only allowed in secure contexts.
  if (!attribution_host_utils::IsOriginTrustworthyForAttributions(
          context.context_origin) ||
      !attribution_host_utils::IsOriginTrustworthyForAttributions(
          reporting_origin) ||
      !attribution_host_utils::IsOriginTrustworthyForAttributions(
          data->destination)) {
    return;
  }

  absl::optional<AttributionFilterData> filter_data =
      AttributionFilterData::FromSourceFilterValues(
          std::move(data->filter_data->filter_values));
  if (!filter_data.has_value())
    return;

  absl::optional<AttributionAggregatableSource> aggregatable_source =
      AttributionAggregatableSource::Create(
          ConvertToProto(*data->aggregatable_source));
  if (!aggregatable_source.has_value())
    return;

  StorableSource storable_source(CommonSourceInfo(
      data->source_event_id, context.context_origin, data->destination,
      reporting_origin, source_time,
      CommonSourceInfo::GetExpiryTime(data->expiry, source_time,
                                      context.source_type),
      context.source_type, data->priority, std::move(*filter_data),
      data->debug_key ? absl::make_optional(data->debug_key->value)
                      : absl::nullopt,
      std::move(*aggregatable_source)));

  attribution_manager_->HandleSource(std::move(storable_source));
}

void AttributionDataHostManagerImpl::TriggerDataAvailable(
    blink::mojom::AttributionTriggerDataPtr data) {
  // TODO(linnan): Log metrics for early returns.

  auto [it, inserted] =
      receiver_data_.emplace(receivers_.current_receiver(), url::Origin());
  if (!it->second.opaque())
    return;

  const FrozenContext& context = receivers_.current_context();
  const url::Origin& reporting_origin = data->reporting_origin;

  // The API is only allowed in secure contexts.
  if (!attribution_host_utils::IsOriginTrustworthyForAttributions(
          context.context_origin) ||
      !attribution_host_utils::IsOriginTrustworthyForAttributions(
          reporting_origin)) {
    return;
  }

  absl::optional<AttributionFilterData> filters =
      AttributionFilterData::FromTriggerFilterValues(
          std::move(data->filters->filter_values));
  if (!filters.has_value())
    return;

  if (data->event_triggers.size() > blink::kMaxAttributionEventTriggerData)
    return;

  std::vector<AttributionTrigger::EventTriggerData> event_triggers;
  event_triggers.reserve(data->event_triggers.size());

  for (const auto& event_trigger : data->event_triggers) {
    absl::optional<AttributionFilterData> filters =
        AttributionFilterData::FromTriggerFilterValues(
            std::move(event_trigger->filters->filter_values));
    if (!filters.has_value())
      return;

    absl::optional<AttributionFilterData> not_filters =
        AttributionFilterData::FromTriggerFilterValues(
            std::move(event_trigger->not_filters->filter_values));
    if (!not_filters.has_value())
      return;

    event_triggers.emplace_back(
        event_trigger->data, event_trigger->priority,
        event_trigger->dedup_key
            ? absl::make_optional(event_trigger->dedup_key->value)
            : absl::nullopt,
        std::move(*filters), std::move(*not_filters));
  }

  AttributionTrigger trigger(
      /*destination_origin=*/context.context_origin, reporting_origin,
      std::move(*filters),
      data->debug_key ? absl::make_optional(data->debug_key->value)
                      : absl::nullopt,
      std::move(event_triggers));

  attribution_manager_->HandleTrigger(std::move(trigger));
}

void AttributionDataHostManagerImpl::OnDataHostDisconnected() {
  receiver_data_.erase(receivers_.current_receiver());
}

}  // namespace content
