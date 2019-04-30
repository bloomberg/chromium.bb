// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/privacy_filtering_check.h"

#include "base/logging.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace tracing {
namespace {

using perfetto::protos::pbzero::InternedData;
using perfetto::protos::pbzero::TracePacket;
using perfetto::protos::pbzero::TrackEvent;
using protozero::ProtoDecoder;

// A Message node created from a tree of trace packet proto messages.
struct Message {
  // List of accepted field ids in the output for this message. The end of list
  // is marked by a -1.
  const int* accepted_field_ids;

  // List of sub messages that correspond to the accepted field ids list. There
  // is no end of list marker and the length is this list is equal to length of
  // |accepted_field_ids| - 1.
  const Message* const* const sub_messages;
};

// The structure of acceptable field IDs in the output proto.
// TODO(ssid): This should be generated as a resource file using the proto
// definitions using a script.

// Interned data types:
constexpr int kSourceLocationIndices[] = {1, 2, 3, -1};
constexpr Message kSourceLocation = {kSourceLocationIndices, nullptr};

constexpr int kLegacyEventNameIndices[] = {1, 2, -1};
constexpr Message kLegacyEventName = {kLegacyEventNameIndices, nullptr};

constexpr int kEventCategoryIndices[] = {1, 2, -1};
constexpr Message kEventCategory = {kEventCategoryIndices, nullptr};

constexpr int kInternedDataIndices[] = {1, 2, 4, -1};
constexpr Message const* kInternedDataComplexMessages[] = {
    &kEventCategory, &kLegacyEventName, &kSourceLocation};
constexpr Message kInternedData = {kInternedDataIndices,
                                   kInternedDataComplexMessages};

// Typed track events:
constexpr int kTaskExecutionIndices[] = {1, -1};
constexpr Message kTaskExecution = {kTaskExecutionIndices, nullptr};

// Track event tree:
constexpr int kThreadDescriptorIndices[] = {1, 2, 4, 6, 7, -1};
constexpr Message kThreadDescriptor = {kThreadDescriptorIndices, nullptr};

constexpr int kProcessDescriptorIndices[] = {1, 4, -1};
constexpr Message kProcessDescriptor = {kProcessDescriptorIndices, nullptr};

constexpr int kLegacyEventIndices[] = {1,  2,  3,  4,  6,  8,  9, 10,
                                       11, 12, 13, 14, 18, 19, -1};
constexpr Message kLegacyEvent = {kLegacyEventIndices, nullptr};

constexpr int kTrackEventIndices[] = {1, 2, 3, 5, 6, 16, 17, -1};
constexpr Message const* kTrackEventComplexMessages[] = {
    nullptr,       nullptr, nullptr, &kTaskExecution,
    &kLegacyEvent, nullptr, nullptr};
constexpr Message kTrackEvent = {kTrackEventIndices,
                                 kTrackEventComplexMessages};

// Trace packet:
constexpr int kTracePacketIndices[] = {10, 11, 12, 41, 42, 43, 44,
                                       // These should be removed or whitelisted
                                       3, 33, 35, 36, 45, -1};
constexpr Message const* kTracePacketComplexMessages[] = {nullptr,
                                                          &kTrackEvent,
                                                          &kInternedData,
                                                          nullptr,
                                                          nullptr,
                                                          &kProcessDescriptor,
                                                          &kThreadDescriptor,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr,
                                                          nullptr};
constexpr Message kTracePacket = {kTracePacketIndices,
                                  kTracePacketComplexMessages};

int FindIndexOfValue(const int* const arr, uint32_t value) {
  for (unsigned i = 0; arr[i] != -1; ++i) {
    if (static_cast<int>(value) == arr[i])
      return i;
  }
  return -1;
}

// Recursively verifies that the |proto| contains only accepted field IDs
// including all sub messages.
void VerifyProto(const Message* root, ProtoDecoder* proto) {
  proto->Reset();
  for (auto f = proto->ReadField(); f.valid(); f = proto->ReadField()) {
    int index = FindIndexOfValue(root->accepted_field_ids, f.id());
    if (index == -1) {
      NOTREACHED() << " unexpected field id " << f.id();
      continue;
    }
    if (root->sub_messages && root->sub_messages[index] != nullptr) {
      ProtoDecoder decoder(f.data(), f.size());
      VerifyProto(root->sub_messages[index], &decoder);
    }
  }
}
}

PrivacyFilteringCheck::PrivacyFilteringCheck() = default;
PrivacyFilteringCheck::~PrivacyFilteringCheck() = default;

// static
void PrivacyFilteringCheck::CheckProtoForUnexpectedFields(
    const std::string& serialized_trace_proto) {
  perfetto::protos::pbzero::Trace::Decoder trace(
      reinterpret_cast<const uint8_t*>(serialized_trace_proto.data()),
      serialized_trace_proto.size());

  for (auto it = trace.packet(); !!it; ++it) {
    TracePacket::Decoder packet(it->data(), it->size());
    const Message* root = &kTracePacket;
    VerifyProto(root, &packet);

    if (packet.has_track_event()) {
      ++stats_.track_event;
    } else if (packet.has_process_descriptor()) {
      ++stats_.process_desc;
    } else if (packet.has_thread_descriptor()) {
      ++stats_.thread_desc;
    }
    if (packet.has_interned_data()) {
      InternedData::Decoder interned_data(packet.interned_data().data,
                                          packet.interned_data().size);
      stats_.has_interned_names |= interned_data.has_legacy_event_names();
      stats_.has_interned_categories |= interned_data.has_event_categories();
      stats_.has_interned_source_locations |=
          interned_data.has_source_locations();
    }
  }
}

}  // namespace tracing
