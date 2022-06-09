// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/types.h"

#include <ostream>
#include <utility>

#include "base/base64.h"
#include "base/json/values_util.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/v2/public/types.h"

// Note: This file contains implementation for both types.h and public/types.h.
//       because our build system will not allow multiple types.cc files in the
//       same target.

namespace feed {
namespace {

void PickleNetworkResponseInfo(const NetworkResponseInfo& value,
                               base::Pickle& pickle) {
  pickle.WriteInt(value.status_code);
  pickle.WriteUInt64(value.fetch_duration.InMilliseconds());
  pickle.WriteUInt64(
      (value.fetch_time - base::Time::UnixEpoch()).InMilliseconds());
  pickle.WriteString(value.bless_nonce);
  pickle.WriteString(value.base_request_url.spec());
}

bool UnpickleNetworkResponseInfo(base::PickleIterator& iterator,
                                 NetworkResponseInfo& value) {
  uint64_t fetch_duration_ms;
  uint64_t fetch_time_ms;
  std::string base_request_url;
  if (!(iterator.ReadInt(&value.status_code) &&
        iterator.ReadUInt64(&fetch_duration_ms) &&
        iterator.ReadUInt64(&fetch_time_ms) &&
        iterator.ReadString(&value.bless_nonce) &&
        iterator.ReadString(&base_request_url)))
    return false;
  value.fetch_duration = base::Milliseconds(fetch_duration_ms);
  value.fetch_time =
      base::Milliseconds(fetch_time_ms) + base::Time::UnixEpoch();
  value.base_request_url = GURL(base_request_url);
  return true;
}

void PickleOptionalNetworkResponseInfo(
    const absl::optional<NetworkResponseInfo>& value,
    base::Pickle& pickle) {
  if (value.has_value()) {
    pickle.WriteBool(true);
    PickleNetworkResponseInfo(*value, pickle);
  } else {
    pickle.WriteBool(false);
  }
}

bool UnpickleOptionalNetworkResponseInfo(
    base::PickleIterator& iterator,
    absl::optional<NetworkResponseInfo>& value) {
  bool has_network_response_info = false;
  if (!iterator.ReadBool(&has_network_response_info))
    return false;

  if (has_network_response_info) {
    NetworkResponseInfo reponse_info;
    if (!UnpickleNetworkResponseInfo(iterator, reponse_info))
      return false;
    value = std::move(reponse_info);
  } else {
    value.reset();
  }
  return true;
}

void PickleDebugStreamData(const DebugStreamData& value, base::Pickle& pickle) {
  (void)PickleOptionalNetworkResponseInfo;
  pickle.WriteInt(DebugStreamData::kVersion);
  PickleOptionalNetworkResponseInfo(value.fetch_info, pickle);
  PickleOptionalNetworkResponseInfo(value.upload_info, pickle);
  pickle.WriteString(value.load_stream_status);
}

bool UnpickleDebugStreamData(base::PickleIterator iterator,
                             DebugStreamData& value) {
  int version;
  return iterator.ReadInt(&version) && version == DebugStreamData::kVersion &&
         UnpickleOptionalNetworkResponseInfo(iterator, value.fetch_info) &&
         UnpickleOptionalNetworkResponseInfo(iterator, value.upload_info) &&
         iterator.ReadString(&value.load_stream_status);
}

}  // namespace

RequestMetadata::RequestMetadata() = default;
RequestMetadata::~RequestMetadata() = default;
RequestMetadata::RequestMetadata(RequestMetadata&&) = default;
RequestMetadata& RequestMetadata::operator=(RequestMetadata&&) = default;

NetworkResponseInfo::NetworkResponseInfo() = default;
NetworkResponseInfo::~NetworkResponseInfo() = default;
NetworkResponseInfo::NetworkResponseInfo(const NetworkResponseInfo&) = default;
NetworkResponseInfo& NetworkResponseInfo::operator=(
    const NetworkResponseInfo&) = default;

std::string ToString(ContentRevision c) {
  // The 'c/' prefix is used to identify slices as content. Don't change this
  // without updating the Java side.
  return base::StrCat({"c/", base::NumberToString(c.value())});
}

ContentRevision ToContentRevision(const std::string& str) {
  if (str.size() < 3)
    return {};

  uint32_t value;
  if (str[0] == 'c' && str[1] == '/' &&
      base::StringToUint(base::StringPiece(str).substr(2), &value)) {
    return ContentRevision(value);
  }
  return {};
}

std::string SerializeDebugStreamData(const DebugStreamData& data) {
  base::Pickle pickle;
  PickleDebugStreamData(data, pickle);
  const uint8_t* pickle_data_ptr = static_cast<const uint8_t*>(pickle.data());
  return base::Base64Encode(
      base::span<const uint8_t>(pickle_data_ptr, pickle.size()));
}

absl::optional<DebugStreamData> DeserializeDebugStreamData(
    base::StringPiece base64_encoded) {
  std::string binary_data;
  if (!base::Base64Decode(base64_encoded, &binary_data))
    return absl::nullopt;
  base::Pickle pickle(binary_data.data(), binary_data.size());
  DebugStreamData result;
  if (!UnpickleDebugStreamData(base::PickleIterator(pickle), result))
    return absl::nullopt;
  return result;
}

DebugStreamData::DebugStreamData() = default;
DebugStreamData::~DebugStreamData() = default;
DebugStreamData::DebugStreamData(const DebugStreamData&) = default;
DebugStreamData& DebugStreamData::operator=(const DebugStreamData&) = default;

base::Value PersistentMetricsDataToValue(const PersistentMetricsData& data) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("day_start", base::TimeToValue(data.current_day_start));
  dict.SetKey("time_spent_in_feed",
              base::TimeDeltaToValue(data.accumulated_time_spent_in_feed));
  return dict;
}

PersistentMetricsData PersistentMetricsDataFromValue(const base::Value& value) {
  PersistentMetricsData result;
  if (!value.is_dict())
    return result;
  absl::optional<base::Time> day_start =
      base::ValueToTime(value.FindKey("day_start"));
  if (!day_start)
    return result;
  result.current_day_start = *day_start;
  absl::optional<base::TimeDelta> time_spent_in_feed =
      base::ValueToTimeDelta(value.FindKey("time_spent_in_feed"));
  if (time_spent_in_feed) {
    result.accumulated_time_spent_in_feed = *time_spent_in_feed;
  }

  return result;
}

LoadLatencyTimes::LoadLatencyTimes() : last_time_(base::TimeTicks::Now()) {}
LoadLatencyTimes::~LoadLatencyTimes() = default;
void LoadLatencyTimes::StepComplete(StepKind kind) {
  auto now = base::TimeTicks::Now();
  steps_.push_back(Step{kind, now - last_time_});
  last_time_ = now;
}

ContentIdSet::ContentIdSet() = default;
ContentIdSet::~ContentIdSet() = default;
ContentIdSet::ContentIdSet(base::flat_set<int64_t> content_ids)
    : content_ids_(std::move(content_ids)) {}
ContentIdSet::ContentIdSet(const ContentIdSet&) = default;
ContentIdSet::ContentIdSet(ContentIdSet&&) = default;
ContentIdSet& ContentIdSet::operator=(const ContentIdSet&) = default;
ContentIdSet& ContentIdSet::operator=(ContentIdSet&&) = default;
bool ContentIdSet::ContainsAllOf(const ContentIdSet& items) const {
  for (int64_t id : items.content_ids_) {
    if (!content_ids_.contains(id))
      return false;
  }
  return true;
}
bool ContentIdSet::IsEmpty() const {
  return content_ids_.empty();
}
bool ContentIdSet::operator==(const ContentIdSet& rhs) const {
  return content_ids_ == rhs.content_ids_;
}
std::ostream& operator<<(std::ostream& s, const ContentIdSet& id_set) {
  s << "{";
  for (int64_t id : id_set.values()) {
    s << id << ", ";
  }
  s << "}";
  return s;
}

LaunchResult::LaunchResult(LoadStreamStatus load_stream_status,
                           feedwire::DiscoverLaunchResult launch_result)
    : load_stream_status(load_stream_status), launch_result(launch_result) {}
LaunchResult::LaunchResult(const LaunchResult& other) = default;
LaunchResult::~LaunchResult() = default;
LaunchResult& LaunchResult::operator=(const LaunchResult& other) = default;

LoggingParameters::LoggingParameters() = default;
LoggingParameters::~LoggingParameters() = default;
LoggingParameters::LoggingParameters(const LoggingParameters&) = default;
LoggingParameters::LoggingParameters(LoggingParameters&&) = default;
LoggingParameters& LoggingParameters::operator=(const LoggingParameters&) =
    default;

bool LoggingParameters::operator==(const LoggingParameters& rhs) const {
  return std::tie(email, client_instance_id, logging_enabled,
                  view_actions_enabled) ==
         std::tie(rhs.email, rhs.client_instance_id, logging_enabled,
                  view_actions_enabled);
}

LoggingParameters FromProto(const feedui::LoggingParameters& proto) {
  LoggingParameters result;
  result.email = proto.email();
  result.client_instance_id = proto.client_instance_id();
  result.logging_enabled = proto.logging_enabled();
  result.view_actions_enabled = proto.view_actions_enabled();
  result.root_event_id = proto.root_event_id();
  return result;
}

void ToProto(const LoggingParameters& logging_parameters,
             feedui::LoggingParameters& proto) {
  proto.set_email(logging_parameters.email);
  proto.set_client_instance_id(logging_parameters.client_instance_id);
  proto.set_logging_enabled(logging_parameters.logging_enabled);
  proto.set_view_actions_enabled(logging_parameters.view_actions_enabled);
  proto.set_root_event_id(logging_parameters.root_event_id);
}

}  // namespace feed
