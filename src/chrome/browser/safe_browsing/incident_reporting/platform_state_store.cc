// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the platform-neutral Load() and Store() functions for a
// profile's safebrowsing.incidents_sent preference dictionary. The preference
// dict is converted to a protocol buffer message which is then serialized into
// a byte array. This serialized data is written to or read from some
// platform-specific storage via {Read,Write}StoreData implemented elsewhere.
//
// A pref dict like so:
//   { "0": {"key1": "1235"}, {"key2": "6789"}}}
// is converted to an identical protocol buffer message, where the top-level
// mapping's keys are of type int, and the nested mappings' values are of type
// uint32_t.

#include "chrome/browser/safe_browsing/incident_reporting/platform_state_store.h"

#include "base/values.h"

#if defined(USE_PLATFORM_STATE_STORE)

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/safe_browsing/incident_reporting/state_store_data.pb.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

#endif  // USE_PLATFORM_STATE_STORE

namespace safe_browsing {
namespace platform_state_store {

#if defined(USE_PLATFORM_STATE_STORE)

namespace {

using google::protobuf::RepeatedPtrField;

// Copies the (key, digest) pairs from |keys_and_digests| (a dict of string
// values) to the |key_digest_pairs| protobuf.
void KeysAndDigestsToProtobuf(
    const base::DictionaryValue& keys_and_digests,
    RepeatedPtrField<StateStoreData::Incidents::KeyDigestMapFieldEntry>*
        key_digest_pairs) {
  std::string digest_value;
  for (base::DictionaryValue::Iterator iter(keys_and_digests); !iter.IsAtEnd();
       iter.Advance()) {
    uint32_t digest = 0;
    if (!iter.value().is_string() || !iter.value().GetAsString(&digest_value) ||
        !base::StringToUint(digest_value, &digest)) {
      NOTREACHED();
      continue;
    }
    StateStoreData::Incidents::KeyDigestMapFieldEntry* key_digest =
        key_digest_pairs->Add();
    key_digest->set_key(iter.key());
    key_digest->set_digest(digest);
  }
}

// Copies the (type, dict) pairs from |incidents_sent| (a dict of dict values)
// to the |typed_incidents| protobuf.
void IncidentsSentToProtobuf(
    const base::DictionaryValue& incidents_sent,
    RepeatedPtrField<StateStoreData::TypeIncidentsMapFieldEntry>*
        type_incidents_pairs) {
  for (base::DictionaryValue::Iterator iter(incidents_sent); !iter.IsAtEnd();
       iter.Advance()) {
    const base::DictionaryValue* keys_and_digests = nullptr;
    if (!iter.value().GetAsDictionary(&keys_and_digests)) {
      NOTREACHED();
      continue;
    }
    if (keys_and_digests->empty())
      continue;
    int incident_type = 0;
    if (!base::StringToInt(iter.key(), &incident_type)) {
      NOTREACHED();
      continue;
    }
    StateStoreData::TypeIncidentsMapFieldEntry* entry =
        type_incidents_pairs->Add();
    entry->set_type(incident_type);
    KeysAndDigestsToProtobuf(
        *keys_and_digests, entry->mutable_incidents()->mutable_key_to_digest());
  }
}

// Copies the (key, digest) pairs for a specific incident type into |type_dict|
// (a dict of string values).
void RestoreOfTypeFromProtobuf(
    const RepeatedPtrField<StateStoreData::Incidents::KeyDigestMapFieldEntry>&
        key_digest_pairs,
    base::Value* type_dict) {
  for (const auto& key_digest : key_digest_pairs) {
    if (!key_digest.has_key() || !key_digest.has_digest())
      continue;
    type_dict->SetKey(key_digest.key(),
                      base::Value(base::NumberToString(key_digest.digest())));
  }
}

// Copies the (type, dict) pairs into |value_dict| (a dict of dict values).
void RestoreFromProtobuf(
    const RepeatedPtrField<StateStoreData::TypeIncidentsMapFieldEntry>&
        type_incidents_pairs,
    base::Value* value_dict) {
  for (const auto& type_incidents : type_incidents_pairs) {
    if (!type_incidents.has_type() || !type_incidents.has_incidents() ||
        type_incidents.incidents().key_to_digest_size() == 0) {
      continue;
    }
    std::string type_string(base::NumberToString(type_incidents.type()));
    base::Value* type_dict =
        value_dict->FindKeyOfType(type_string, base::Value::Type::DICTIONARY);
    if (!type_dict) {
      type_dict = value_dict->SetKey(
          type_string, base::Value(base::Value::Type::DICTIONARY));
    }
    RestoreOfTypeFromProtobuf(type_incidents.incidents().key_to_digest(),
                              type_dict);
  }
}

}  // namespace

#endif  // USE_PLATFORM_STATE_STORE

std::unique_ptr<base::DictionaryValue> Load(Profile* profile) {
#if defined(USE_PLATFORM_STATE_STORE)
  std::unique_ptr<base::DictionaryValue> value_dict(
      new base::DictionaryValue());
  std::string data;
  PlatformStateStoreLoadResult result = ReadStoreData(profile, &data);
  if (result == PlatformStateStoreLoadResult::SUCCESS)
    result = DeserializeIncidentsSent(data, value_dict.get());
  switch (result) {
    case PlatformStateStoreLoadResult::SUCCESS:
    case PlatformStateStoreLoadResult::CLEARED_DATA:
    case PlatformStateStoreLoadResult::CLEARED_NO_DATA:
      // Return a (possibly empty) dictionary for the success cases.
      break;
    case PlatformStateStoreLoadResult::DATA_CLEAR_FAILED:
    case PlatformStateStoreLoadResult::OPEN_FAILED:
    case PlatformStateStoreLoadResult::READ_FAILED:
    case PlatformStateStoreLoadResult::PARSE_ERROR:
      // Return null for all error cases.
      value_dict.reset();
      break;
    case PlatformStateStoreLoadResult::NUM_RESULTS:
      NOTREACHED();
      break;
  }
  return value_dict;
#else
  return nullptr;
#endif
}

void Store(Profile* profile, const base::DictionaryValue* incidents_sent) {
#if defined(USE_PLATFORM_STATE_STORE)
  std::string data;
  SerializeIncidentsSent(incidents_sent, &data);
  WriteStoreData(profile, data);
#endif
}

#if defined(USE_PLATFORM_STATE_STORE)

void SerializeIncidentsSent(const base::DictionaryValue* incidents_sent,
                            std::string* data) {
  StateStoreData store_data;

  IncidentsSentToProtobuf(*incidents_sent,
                          store_data.mutable_type_to_incidents());
  store_data.SerializeToString(data);
}

PlatformStateStoreLoadResult DeserializeIncidentsSent(
    const std::string& data,
    base::DictionaryValue* value_dict) {
  StateStoreData store_data;
  if (data.empty()) {
    value_dict->Clear();
    return PlatformStateStoreLoadResult::SUCCESS;
  }
  if (!store_data.ParseFromString(data))
    return PlatformStateStoreLoadResult::PARSE_ERROR;
  value_dict->Clear();
  RestoreFromProtobuf(store_data.type_to_incidents(), value_dict);
  return PlatformStateStoreLoadResult::SUCCESS;
}

#endif  // USE_PLATFORM_STATE_STORE

}  // namespace platform_state_store
}  // namespace safe_browsing
