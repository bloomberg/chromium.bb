/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/core/profiler/utils/xplane_builder.h"

#include <algorithm>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/profiler/protobuf/xplane.pb.h"
#include "tensorflow/core/profiler/utils/time_utils.h"

namespace tensorflow {
namespace profiler {

XPlaneBuilder::XPlaneBuilder(XPlane* plane)
    : XStatsBuilder<XPlane>(plane, this), plane_(plane) {
  for (auto& iter : *plane->mutable_event_metadata()) {
    last_event_metadata_id_ =
        std::max<int64>(last_event_metadata_id_, iter.second.id());
    event_metadata_by_name_.try_emplace(iter.second.name(), &iter.second);
  }
  for (auto& iter : *plane->mutable_stat_metadata()) {
    last_stat_metadata_id_ =
        std::max<int64>(last_stat_metadata_id_, iter.second.id());
    stat_metadata_by_name_.try_emplace(iter.second.name(), &iter.second);
  }
  for (XLine& line : *plane->mutable_lines()) {
    lines_by_id_.try_emplace(line.id(), &line);
  }
}

XEventMetadata* XPlaneBuilder::GetOrCreateEventMetadata(int64 metadata_id) {
  XEventMetadata& metadata = (*plane_->mutable_event_metadata())[metadata_id];
  metadata.set_id(metadata_id);
  return &metadata;
}

XEventMetadata* XPlaneBuilder::GetOrCreateEventMetadata(
    absl::string_view name) {
  XEventMetadata*& metadata = event_metadata_by_name_[name];
  if (metadata == nullptr) {
    metadata = GetOrCreateEventMetadata(++last_event_metadata_id_);
    metadata->set_name(std::string(name));
  }
  return metadata;
}

XEventMetadata* XPlaneBuilder::GetOrCreateEventMetadata(std::string&& name) {
  XEventMetadata*& metadata = event_metadata_by_name_[name];
  if (metadata == nullptr) {
    metadata = GetOrCreateEventMetadata(++last_event_metadata_id_);
    metadata->set_name(std::move(name));
  }
  return metadata;
}

XStatMetadata* XPlaneBuilder::GetOrCreateStatMetadata(int64 metadata_id) {
  XStatMetadata& metadata = (*plane_->mutable_stat_metadata())[metadata_id];
  metadata.set_id(metadata_id);
  return &metadata;
}

XStatMetadata* XPlaneBuilder::GetOrCreateStatMetadata(absl::string_view name) {
  XStatMetadata*& metadata = stat_metadata_by_name_[name];
  if (metadata == nullptr) {
    metadata = GetOrCreateStatMetadata(++last_stat_metadata_id_);
    metadata->set_name(std::string(name));
  }
  return metadata;
}

XStatMetadata* XPlaneBuilder::GetOrCreateStatMetadata(std::string&& name) {
  XStatMetadata*& metadata = stat_metadata_by_name_[name];
  if (metadata == nullptr) {
    metadata = GetOrCreateStatMetadata(++last_stat_metadata_id_);
    metadata->set_name(std::move(name));
  }
  return metadata;
}

XLineBuilder XPlaneBuilder::GetOrCreateLine(int64 line_id) {
  XLine*& line = lines_by_id_[line_id];
  if (line == nullptr) {
    line = plane_->add_lines();
    line->set_id(line_id);
  }
  return XLineBuilder(line, this);
}

XEventBuilder XLineBuilder::AddEvent(const XEventMetadata& metadata) {
  XEvent* event = line_->add_events();
  event->set_metadata_id(metadata.id());
  return XEventBuilder(line_, plane_, event);
}

XEventBuilder XLineBuilder::AddEvent(const XEvent& event) {
  XEvent* new_event = line_->add_events();
  *new_event = event;
  return XEventBuilder(line_, plane_, new_event);
}

void XLineBuilder::SetTimestampNsAndAdjustEventOffsets(int64 timestamp_ns) {
  int64 offset_ps = NanosToPicos(line_->timestamp_ns() - timestamp_ns);
  line_->set_timestamp_ns(timestamp_ns);
  if (offset_ps) {
    for (auto& event : *line_->mutable_events()) {
      event.set_offset_ps(event.offset_ps() + offset_ps);
    }
  }
}

}  // namespace profiler
}  // namespace tensorflow
