// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/invalidation.h"

#include <cstddef>

#include "base/json/json_string_value_serializer.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "sync/internal_api/public/base/ack_handler.h"
#include "sync/internal_api/public/base/invalidation_util.h"

namespace syncer {

namespace {
const char kObjectIdKey[] = "objectId";
const char kIsUnknownVersionKey[] = "isUnknownVersion";
const char kVersionKey[] = "version";
const char kPayloadKey[] = "payload";
const int64 kInvalidVersion = -1;
}

Invalidation Invalidation::Init(
    const invalidation::ObjectId& id,
    int64 version,
    const std::string& payload) {
  return Invalidation(id, false, version, payload, AckHandle::CreateUnique());
}

Invalidation Invalidation::InitUnknownVersion(
    const invalidation::ObjectId& id) {
  return Invalidation(id, true, kInvalidVersion,
                      std::string(), AckHandle::CreateUnique());
}

Invalidation Invalidation::InitFromDroppedInvalidation(
    const Invalidation& dropped) {
  return Invalidation(dropped.id_, true, kInvalidVersion,
                      std::string(), dropped.ack_handle_);
}

Invalidation::Invalidation(const Invalidation& other)
  : id_(other.id_),
    is_unknown_version_(other.is_unknown_version_),
    version_(other.version_),
    payload_(other.payload_),
    ack_handle_(other.ack_handle_),
    ack_handler_(other.ack_handler_) {
}

scoped_ptr<Invalidation> Invalidation::InitFromValue(
    const base::DictionaryValue& value) {
  invalidation::ObjectId id;

  const base::DictionaryValue* object_id_dict;
  if (!value.GetDictionary(kObjectIdKey, &object_id_dict)
      || !ObjectIdFromValue(*object_id_dict, &id)) {
    DLOG(WARNING) << "Failed to parse id";
    return scoped_ptr<Invalidation>();
  }
  bool is_unknown_version;
  if (!value.GetBoolean(kIsUnknownVersionKey, &is_unknown_version)) {
    DLOG(WARNING) << "Failed to parse is_unknown_version flag";
    return scoped_ptr<Invalidation>();
  }
  if (is_unknown_version) {
    return scoped_ptr<Invalidation>(new Invalidation(
        id,
        true,
        kInvalidVersion,
        std::string(),
        AckHandle::CreateUnique()));
  }
  int64 version = 0;
  std::string version_as_string;
  if (!value.GetString(kVersionKey, &version_as_string)
      || !base::StringToInt64(version_as_string, &version)) {
    DLOG(WARNING) << "Failed to parse version";
    return scoped_ptr<Invalidation>();
  }
  std::string payload;
  if (!value.GetString(kPayloadKey, &payload)) {
    DLOG(WARNING) << "Failed to parse payload";
    return scoped_ptr<Invalidation>();
  }
  return scoped_ptr<Invalidation>(new Invalidation(
      id,
      false,
      version,
      payload,
      AckHandle::CreateUnique()));
}

Invalidation::~Invalidation() {}

Invalidation& Invalidation::operator=(const Invalidation& other) {
  id_ = other.id_;
  is_unknown_version_ = other.is_unknown_version_;
  version_ = other.version_;
  payload_ = other.payload_;
  ack_handle_ = other.ack_handle_;
  ack_handler_ = other.ack_handler_;
  return *this;
}

invalidation::ObjectId Invalidation::object_id() const {
  return id_;
}

bool Invalidation::is_unknown_version() const {
  return is_unknown_version_;
}

int64 Invalidation::version() const {
  DCHECK(!is_unknown_version_);
  return version_;
}

const std::string& Invalidation::payload() const {
  DCHECK(!is_unknown_version_);
  return payload_;
}

const AckHandle& Invalidation::ack_handle() const {
  return ack_handle_;
}

void Invalidation::set_ack_handler(syncer::WeakHandle<AckHandler> handler) {
  ack_handler_ = handler;
}

bool Invalidation::SupportsAcknowledgement() const {
  return ack_handler_.IsInitialized();
}

void Invalidation::Acknowledge() const {
  if (SupportsAcknowledgement()) {
    ack_handler_.Call(FROM_HERE,
                      &AckHandler::Acknowledge,
                      id_,
                      ack_handle_);
  }
}

void Invalidation::Drop() {
  if (SupportsAcknowledgement()) {
    ack_handler_.Call(FROM_HERE,
                      &AckHandler::Drop,
                      id_,
                      ack_handle_);
  }
}

bool Invalidation::Equals(const Invalidation& other) const {
  return id_ == other.id_
      && is_unknown_version_ == other.is_unknown_version_
      && version_ == other.version_
      && payload_ == other.payload_;
}

scoped_ptr<base::DictionaryValue> Invalidation::ToValue() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->Set(kObjectIdKey, ObjectIdToValue(id_).release());
  if (is_unknown_version_) {
    value->SetBoolean(kIsUnknownVersionKey, true);
  } else {
    value->SetBoolean(kIsUnknownVersionKey, false);
    value->SetString(kVersionKey, base::Int64ToString(version_));
    value->SetString(kPayloadKey, payload_);
  }
  return value.Pass();
}

std::string Invalidation::ToString() const {
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  serializer.Serialize(*ToValue().get());
  return output;
}

Invalidation::Invalidation(
    const invalidation::ObjectId& id,
    bool is_unknown_version,
    int64 version,
    const std::string& payload,
    AckHandle ack_handle)
  : id_(id),
    is_unknown_version_(is_unknown_version),
    version_(version),
    payload_(payload),
    ack_handle_(ack_handle) {}

}  // namespace syncer
