// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/entry_kernel.h"

#include "base/string_number_conversions.h"
#include "sync/protocol/proto_value_conversions.h"
#include "sync/syncable/syncable_enum_conversions.h"

namespace syncable {

EntryKernel::EntryKernel() : dirty_(false) {
  // Everything else should already be default-initialized.
  for (int i = INT64_FIELDS_BEGIN; i < INT64_FIELDS_END; ++i) {
    int64_fields[i] = 0;
  }
}

EntryKernel::~EntryKernel() {}

syncable::ModelType EntryKernel::GetServerModelType() const {
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

namespace {

// Utility function to loop through a set of enum values and add the
// field keys/values in the kernel to the given dictionary.
//
// V should be convertible to Value.
template <class T, class U, class V>
void SetFieldValues(const EntryKernel& kernel,
                    DictionaryValue* dictionary_value,
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

// Helper functions for SetFieldValues().

StringValue* Int64ToValue(int64 i) {
  return Value::CreateStringValue(base::Int64ToString(i));
}

StringValue* TimeToValue(const base::Time& t) {
  return Value::CreateStringValue(browser_sync::GetTimeDebugString(t));
}

StringValue* IdToValue(const Id& id) {
  return id.ToValue();
}

}  // namespace

DictionaryValue* EntryKernel::ToValue() const {
  DictionaryValue* kernel_info = new DictionaryValue();
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
                 &GetIndexedBitFieldString, &Value::CreateBooleanValue,
                 BIT_FIELDS_BEGIN, INDEXED_BIT_FIELDS_END - 1);
  SetFieldValues(*this, kernel_info,
                 &GetIsDelFieldString, &Value::CreateBooleanValue,
                 INDEXED_BIT_FIELDS_END, IS_DEL);
  SetFieldValues(*this, kernel_info,
                 &GetBitFieldString, &Value::CreateBooleanValue,
                 IS_DEL + 1, BIT_FIELDS_END - 1);

  // String fields.
  {
    // Pick out the function overload we want.
    StringValue* (*string_to_value)(const std::string&) =
        &Value::CreateStringValue;
    SetFieldValues(*this, kernel_info,
                   &GetStringFieldString, string_to_value,
                   STRING_FIELDS_BEGIN, STRING_FIELDS_END - 1);
  }

  // Proto fields.
  SetFieldValues(*this, kernel_info,
                 &GetProtoFieldString, &browser_sync::EntitySpecificsToValue,
                 PROTO_FIELDS_BEGIN, PROTO_FIELDS_END - 1);

  // Bit temps.
  SetFieldValues(*this, kernel_info,
                 &GetBitTempString, &Value::CreateBooleanValue,
                 BIT_TEMPS_BEGIN, BIT_TEMPS_END - 1);

  return kernel_info;
}

ListValue* EntryKernelMutationMapToValue(
    const EntryKernelMutationMap& mutations) {
  ListValue* list = new ListValue();
  for (EntryKernelMutationMap::const_iterator it = mutations.begin();
       it != mutations.end(); ++it) {
    list->Append(EntryKernelMutationToValue(it->second));
  }
  return list;
}

DictionaryValue* EntryKernelMutationToValue(
    const EntryKernelMutation& mutation) {
  DictionaryValue* dict = new DictionaryValue();
  dict->Set("original", mutation.original.ToValue());
  dict->Set("mutated", mutation.mutated.ToValue());
  return dict;
}

}
