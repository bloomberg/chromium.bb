// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/invalidation.h"

#include <cstddef>
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace syncer {

namespace {
// Hopefully enough bytes for uniqueness.
const size_t kBytesInHandle = 16;
}  // namespace

AckHandle AckHandle::CreateUnique() {
  // This isn't a valid UUID, so we don't attempt to format it like one.
  uint8 random_bytes[kBytesInHandle];
  base::RandBytes(random_bytes, sizeof(random_bytes));
  return AckHandle(base::HexEncode(random_bytes, sizeof(random_bytes)),
                   base::Time::Now());
}

AckHandle AckHandle::InvalidAckHandle() {
  return AckHandle(std::string(), base::Time());
}

bool AckHandle::Equals(const AckHandle& other) const {
  return state_ == other.state_ && timestamp_ == other.timestamp_;
}

scoped_ptr<base::DictionaryValue> AckHandle::ToValue() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString("state", state_);
  value->SetString("timestamp",
                   base::Int64ToString(timestamp_.ToInternalValue()));
  return value.Pass();
}

bool AckHandle::ResetFromValue(const base::DictionaryValue& value) {
  if (!value.GetString("state", &state_))
    return false;
  std::string timestamp_as_string;
  if (!value.GetString("timestamp", &timestamp_as_string))
    return false;
  int64 timestamp_value;
  if (!base::StringToInt64(timestamp_as_string, &timestamp_value))
    return false;
  timestamp_ = base::Time::FromInternalValue(timestamp_value);
  return true;
}

bool AckHandle::IsValid() const {
  return !state_.empty();
}

AckHandle::AckHandle(const std::string& state, base::Time timestamp)
    : state_(state), timestamp_(timestamp) {
}

AckHandle::~AckHandle() {
}

const int64 Invalidation::kUnknownVersion = -1;

Invalidation::Invalidation()
    : version(kUnknownVersion), ack_handle(AckHandle::InvalidAckHandle()) {
}

Invalidation::~Invalidation() {
}

bool Invalidation::Equals(const Invalidation& other) const {
  return (version == other.version) && (payload == other.payload) &&
      ack_handle.Equals(other.ack_handle);
}

scoped_ptr<base::DictionaryValue> Invalidation::ToValue() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString("version", base::Int64ToString(version));
  value->SetString("payload", payload);
  value->Set("ackHandle", ack_handle.ToValue().release());
  return value.Pass();
}

bool Invalidation::ResetFromValue(const base::DictionaryValue& value) {
  const base::DictionaryValue* ack_handle_value = NULL;
  std::string version_as_string;
  if (value.GetString("version", &version_as_string)) {
    if (!base::StringToInt64(version_as_string, &version))
      return false;
  } else {
    version = kUnknownVersion;
  }
  return
      value.GetString("payload", &payload) &&
      value.GetDictionary("ackHandle", &ack_handle_value) &&
      ack_handle.ResetFromValue(*ack_handle_value);
}

}  // namespace syncer
