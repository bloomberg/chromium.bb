// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/privacy_filtering_check.h"

#include "base/logging.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

namespace tracing {
namespace {
using perfetto::protos::TracePacket;
}

PrivacyFilteringCheck::PrivacyFilteringCheck() = default;
PrivacyFilteringCheck::~PrivacyFilteringCheck() = default;

// static
void PrivacyFilteringCheck::CheckProtoForUnexpectedFields(
    const std::string& serialized_trace_proto) {
  auto proto = std::make_unique<perfetto::protos::Trace>();
  if (!proto->ParseFromArray(serialized_trace_proto.data(),
                             serialized_trace_proto.size())) {
    NOTREACHED();
    return;
  }

  for (auto& packet : *proto->mutable_packet()) {
    DCHECK(packet.unknown_fields().empty());
    // TODO(ssid): For each of these messages verify that only expected fields
    // are present.
    if (packet.has_track_event()) {
      ++counts_.track_event;
    } else if (packet.has_process_descriptor()) {
      ++counts_.process_desc;
    } else if (packet.has_thread_descriptor()) {
      ++counts_.thread_desc;
    } else if (packet.has_synchronization_marker() ||
               packet.has_trace_stats() || packet.has_trace_config() ||
               packet.has_system_info()) {
      // TODO(ssid): These should not be present in the traces or must be
      // whitelisted.
    } else {
      DCHECK_EQ(TracePacket::DataCase::DATA_NOT_SET, packet.data_case());
    }
    if (packet.has_interned_data()) {
      counts_.interned_name += packet.interned_data().legacy_event_names_size();
      packet.mutable_interned_data()->clear_legacy_event_names();
      counts_.interned_category +=
          packet.interned_data().event_categories_size();
      packet.mutable_interned_data()->clear_event_categories();
      counts_.interned_source_location +=
          packet.interned_data().source_locations_size();
      packet.mutable_interned_data()->clear_source_locations();

      std::string serialized;
      packet.interned_data().SerializeToString(&serialized);
      DCHECK_EQ(serialized.size(), 0u);
    }
  }
}

}  // namespace tracing
