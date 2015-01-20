// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/util.h"

#include "base/stl_util.h"
#include "base/values.h"

namespace json_schema_compiler {
namespace util {

bool PopulateItem(const base::Value& from, int* out) {
  return from.GetAsInteger(out);
}

bool PopulateItem(const base::Value& from, bool* out) {
  return from.GetAsBoolean(out);
}

bool PopulateItem(const base::Value& from, double* out) {
  return from.GetAsDouble(out);
}

bool PopulateItem(const base::Value& from, std::string* out) {
  return from.GetAsString(out);
}

bool PopulateItem(const base::Value& from, std::vector<char>* out) {
  const base::BinaryValue* binary = nullptr;
  if (!from.GetAsBinary(&binary))
    return false;
  out->assign(binary->GetBuffer(), binary->GetBuffer() + binary->GetSize());
  return true;
}

bool PopulateItem(const base::Value& from, linked_ptr<base::Value>* out) {
  *out = make_linked_ptr(from.DeepCopy());
  return true;
}

bool PopulateItem(const base::Value& from,
                  linked_ptr<base::DictionaryValue>* out) {
  const base::DictionaryValue* dict = nullptr;
  if (!from.GetAsDictionary(&dict))
    return false;
  *out = make_linked_ptr(dict->DeepCopy());
  return true;
}

void AddItemToList(const int from, base::ListValue* out) {
  out->Append(new base::FundamentalValue(from));
}

void AddItemToList(const bool from, base::ListValue* out) {
  out->Append(new base::FundamentalValue(from));
}

void AddItemToList(const double from, base::ListValue* out) {
  out->Append(new base::FundamentalValue(from));
}

void AddItemToList(const std::string& from, base::ListValue* out) {
  out->Append(new base::StringValue(from));
}

void AddItemToList(const std::vector<char>& from, base::ListValue* out) {
  out->Append(base::BinaryValue::CreateWithCopiedBuffer(vector_as_array(&from),
                                                        from.size()));
}

void AddItemToList(const linked_ptr<base::Value>& from, base::ListValue* out) {
  out->Append(from->DeepCopy());
}

void AddItemToList(const linked_ptr<base::DictionaryValue>& from,
                   base::ListValue* out) {
  out->Append(static_cast<base::Value*>(from->DeepCopy()));
}

std::string ValueTypeToString(base::Value::Type type) {
  switch (type) {
    case base::Value::TYPE_NULL:
      return "null";
    case base::Value::TYPE_BOOLEAN:
      return "boolean";
    case base::Value::TYPE_INTEGER:
      return "integer";
    case base::Value::TYPE_DOUBLE:
      return "number";
    case base::Value::TYPE_STRING:
      return "string";
    case base::Value::TYPE_BINARY:
      return "binary";
    case base::Value::TYPE_DICTIONARY:
      return "dictionary";
    case base::Value::TYPE_LIST:
      return "list";
  }
  NOTREACHED();
  return "";
}

}  // namespace util
}  // namespace json_schema_compiler
