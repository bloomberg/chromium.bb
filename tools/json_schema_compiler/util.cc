// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/any.h"
#include "tools/json_schema_compiler/util.h"

#include "base/values.h"

namespace json_schema_compiler {
namespace util {

bool GetItemFromList(const ListValue& from, int index, int* out) {
  return from.GetInteger(index, out);
}

bool GetItemFromList(const ListValue& from, int index, bool* out) {
  return from.GetBoolean(index, out);
}

bool GetItemFromList(const ListValue& from, int index, double* out) {
  return from.GetDouble(index, out);
}

bool GetItemFromList(const ListValue& from, int index, std::string* out) {
  return from.GetString(index, out);
}

bool GetItemFromList(const ListValue& from, int index,
    linked_ptr<any::Any>* out) {
  Value* value = NULL;
  if (!from.Get(index, &value))
    return false;
  scoped_ptr<any::Any> any_object(new any::Any());
  any_object->Init(*value);
  *out = linked_ptr<any::Any>(any_object.release());
  return true;
}

bool GetItemFromList(const ListValue& from, int index,
    linked_ptr<base::DictionaryValue>* out) {
  DictionaryValue* dict = NULL;
  if (!from.GetDictionary(index, &dict))
    return false;
  *out = linked_ptr<DictionaryValue>(dict->DeepCopy());
  return true;
}

void AddItemToList(const int from, base::ListValue* out) {
  out->Append(base::Value::CreateIntegerValue(from));
}
void AddItemToList(const bool from, base::ListValue* out) {
  out->Append(base::Value::CreateBooleanValue(from));
}
void AddItemToList(const double from, base::ListValue* out) {
  out->Append(base::Value::CreateDoubleValue(from));
}
void AddItemToList(const std::string& from, base::ListValue* out) {
  out->Append(base::Value::CreateStringValue(from));
}
void AddItemToList(const linked_ptr<base::DictionaryValue>& from,
    base::ListValue* out) {
  out->Append(static_cast<Value*>(from->DeepCopy()));
}
void AddItemToList(const linked_ptr<any::Any>& from,
    base::ListValue* out) {
  out->Append(from->value().DeepCopy());
}

}  // namespace api_util
}  // namespace extensions
