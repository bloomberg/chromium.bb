// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/entry_kernel.h"

#include "base/string_number_conversions.h"
#include "sync/protocol/proto_value_conversions.h"
#include "sync/syncable/syncable_enum_conversions.h"
#include "sync/util/cryptographer.h"

namespace syncer {
namespace syncable {

EntryKernel::EntryKernel() : dirty_(false) {
  // Everything else should already be default-initialized.
  for (int i = INT64_FIELDS_BEGIN; i < INT64_FIELDS_END; ++i) {
    int64_fields[i] = 0;
  }
}

EntryKernel::~EntryKernel() {}

ModelType EntryKernel::GetModelType() const {
  ModelType specifics_type = GetModelTypeFromSpecifics(ref(SPECIFICS));
  if (specifics_type != UNSPECIFIED)
    return specifics_type;
  if (ref(ID).IsRoot())
    return TOP_LEVEL_FOLDER;
  // Loose check for server-created top-level folders that aren't
  // bound to a particular model type.
  if (!ref(UNIQUE_SERVER_TAG).empty() && ref(SERVER_IS_DIR))
    return TOP_LEVEL_FOLDER;

  return UNSPECIFIED;
}

ModelType EntryKernel::GetServerModelType() const {
  ModelType specifics_type = GetModelTypeFromSpecifics(ref(SERVER_SPECIFICS));
  if (specifics_type != UNSPECIFIED)
    return specifics_type;
  if (ref(ID).IsRoot())
    return TOP_LEVEL_FOLDER;
  // Loose check for server-created top-level folders that aren't
  // bound to a particular model type.
  if (!ref(UNIQUE_SERVER_TAG).empty() && ref(SERVER_IS_DIR))
    return TOP_LEVEL_FOLDER;

  return UNSPECIFIED;
}

bool EntryKernel::ShouldMaintainPosition() const {
  // We maintain positions for all bookmarks, except those that are
  // server-created top-level folders.
  return (GetModelTypeFromSpecifics(ref(SPECIFICS)) == syncer::BOOKMARKS)
      && !(!ref(UNIQUE_SERVER_TAG).empty() && ref(IS_DIR));
}

namespace {

// Utility function to loop through a set of enum values and add the
// field keys/values in the kernel to the given dictionary.
//
// V should be convertible to Value.
template <class T, class U, class V>
void SetFieldValues(const EntryKernel& kernel,
                    base::DictionaryValue* dictionary_value,
                    const char* (*enum_key_fn)(T),
                    V* (*enum_value_fn)(U),
                    int field_key_min, int field_key_max) {
  DCHECK_LE(field_key_min, field_key_max);
  for (int i = field_key_min; i <= field_key_max; ++i) {
    T field = static_cast<T>(i);
    const std::string& key = enum_key_fn(field);
    V* value = enum_value_fn(kernel.ref(field));
    dictionary_value->Set(key, value);
  }
}

void SetEncryptableProtoValues(
    const EntryKernel& kernel,
    Cryptographer* cryptographer,
    base::DictionaryValue* dictionary_value,
    int field_key_min, int field_key_max) {
  DCHECK_LE(field_key_min, field_key_max);
  for (int i = field_key_min; i <= field_key_max; ++i) {
    ProtoField field = static_cast<ProtoField>(i);
    const std::string& key = GetProtoFieldString(field);

    base::DictionaryValue* value = NULL;
    sync_pb::EntitySpecifics decrypted;
    const sync_pb::EncryptedData& encrypted = kernel.ref(field).encrypted();
    if (cryptographer &&
        kernel.ref(field).has_encrypted() &&
        cryptographer->CanDecrypt(encrypted) &&
        cryptographer->Decrypt(encrypted, &decrypted)) {
      value = EntitySpecificsToValue(decrypted);
      value->SetBoolean("encrypted", true);
    } else {
      value = EntitySpecificsToValue(kernel.ref(field));
    }
    dictionary_value->Set(key, value);
  }
}

// Helper functions for SetFieldValues().

base::StringValue* Int64ToValue(int64 i) {
  return new base::StringValue(base::Int64ToString(i));
}

base::StringValue* TimeToValue(const base::Time& t) {
  return new base::StringValue(GetTimeDebugString(t));
}

base::StringValue* IdToValue(const Id& id) {
  return id.ToValue();
}

base::FundamentalValue* BooleanToValue(bool bool_val) {
  return new base::FundamentalValue(bool_val);
}

base::StringValue* StringToValue(const std::string& str) {
  return new base::StringValue(str);
}

StringValue* UniquePositionToValue(const UniquePosition& pos) {
  return Value::CreateStringValue(pos.ToDebugString());
}

}  // namespace

base::DictionaryValue* EntryKernel::ToValue(
    Cryptographer* cryptographer) const {
  base::DictionaryValue* kernel_info = new base::DictionaryValue();
  kernel_info->SetBoolean("isDirty", is_dirty());
  kernel_info->Set("serverModelType", ModelTypeToValue(GetServerModelType()));

  // Int64 fields.
  SetFieldValues(*this, kernel_info,
                 &GetMetahandleFieldString, &Int64ToValue,
                 INT64_FIELDS_BEGIN, META_HANDLE);
  SetFieldValues(*this, kernel_info,
                 &GetBaseVersionString, &Int64ToValue,
                 META_HANDLE + 1, BASE_VERSION);
  SetFieldValues(*this, kernel_info,
                 &GetInt64FieldString, &Int64ToValue,
                 BASE_VERSION + 1, INT64_FIELDS_END - 1);

  // Time fields.
  SetFieldValues(*this, kernel_info,
                 &GetTimeFieldString, &TimeToValue,
                 TIME_FIELDS_BEGIN, TIME_FIELDS_END - 1);

  // ID fields.
  SetFieldValues(*this, kernel_info,
                 &GetIdFieldString, &IdToValue,
                 ID_FIELDS_BEGIN, ID_FIELDS_END - 1);

  // Bit fields.
  SetFieldValues(*this, kernel_info,
                 &GetIndexedBitFieldString, &BooleanToValue,
                 BIT_FIELDS_BEGIN, INDEXED_BIT_FIELDS_END - 1);
  SetFieldValues(*this, kernel_info,
                 &GetIsDelFieldString, &BooleanToValue,
                 INDEXED_BIT_FIELDS_END, IS_DEL);
  SetFieldValues(*this, kernel_info,
                 &GetBitFieldString, &BooleanToValue,
                 IS_DEL + 1, BIT_FIELDS_END - 1);

  // String fields.
  {
    // Pick out the function overload we want.
    SetFieldValues(*this, kernel_info,
                   &GetStringFieldString, &StringToValue,
                   STRING_FIELDS_BEGIN, STRING_FIELDS_END - 1);
  }

  // Proto fields.
  SetEncryptableProtoValues(*this, cryptographer, kernel_info,
                            PROTO_FIELDS_BEGIN, PROTO_FIELDS_END - 1);

  // UniquePosition fields
  SetFieldValues(*this, kernel_info,
                 &GetUniquePositionFieldString, &UniquePositionToValue,
                 UNIQUE_POSITION_FIELDS_BEGIN, UNIQUE_POSITION_FIELDS_END - 1);

  // Bit temps.
  SetFieldValues(*this, kernel_info,
                 &GetBitTempString, &BooleanToValue,
                 BIT_TEMPS_BEGIN, BIT_TEMPS_END - 1);

  return kernel_info;
}

base::ListValue* EntryKernelMutationMapToValue(
    const EntryKernelMutationMap& mutations) {
  base::ListValue* list = new base::ListValue();
  for (EntryKernelMutationMap::const_iterator it = mutations.begin();
       it != mutations.end(); ++it) {
    list->Append(EntryKernelMutationToValue(it->second));
  }
  return list;
}

base::DictionaryValue* EntryKernelMutationToValue(
    const EntryKernelMutation& mutation) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->Set("original", mutation.original.ToValue(NULL));
  dict->Set("mutated", mutation.mutated.ToValue(NULL));
  return dict;
}

}  // namespace syncer
}  // namespace syncable
