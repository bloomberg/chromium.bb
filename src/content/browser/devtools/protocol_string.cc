// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol_string.h"

#include <utility>
#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/devtools/protocol/protocol.h"

namespace content {
namespace protocol {

std::unique_ptr<protocol::Value> toProtocolValue(
    const base::Value* value, int depth) {
  if (!value || !depth)
    return nullptr;
  if (value->is_none())
    return protocol::Value::null();
  if (value->is_bool()) {
    bool inner;
    value->GetAsBoolean(&inner);
    return protocol::FundamentalValue::create(inner);
  }
  if (value->is_int()) {
    int inner;
    value->GetAsInteger(&inner);
    return protocol::FundamentalValue::create(inner);
  }
  if (value->is_double()) {
    double inner;
    value->GetAsDouble(&inner);
    return protocol::FundamentalValue::create(inner);
  }
  if (value->is_string()) {
    std::string inner;
    value->GetAsString(&inner);
    return protocol::StringValue::create(inner);
  }
  if (value->is_list()) {
    const base::ListValue* list = nullptr;
    value->GetAsList(&list);
    std::unique_ptr<protocol::ListValue> result = protocol::ListValue::create();
    for (size_t i = 0; i < list->GetSize(); i++) {
      const base::Value* item = nullptr;
      list->Get(i, &item);
      std::unique_ptr<protocol::Value> converted =
          toProtocolValue(item, depth - 1);
      if (converted)
        result->pushValue(std::move(converted));
    }
    return std::move(result);
  }
  if (value->is_dict()) {
    const base::DictionaryValue* dictionary = nullptr;
    value->GetAsDictionary(&dictionary);
    std::unique_ptr<protocol::DictionaryValue> result =
        protocol::DictionaryValue::create();
    for (base::DictionaryValue::Iterator it(*dictionary);
        !it.IsAtEnd(); it.Advance()) {
      std::unique_ptr<protocol::Value> converted =
          toProtocolValue(&it.value(), depth - 1);
      if (converted)
        result->setValue(it.key(), std::move(converted));
    }
    return std::move(result);
  }
  return nullptr;
}

std::unique_ptr<base::Value> toBaseValue(
    protocol::Value* value, int depth) {
  if (!value || !depth)
    return nullptr;
  if (value->type() == protocol::Value::TypeNull)
    return std::make_unique<base::Value>();
  if (value->type() == protocol::Value::TypeBoolean) {
    bool inner;
    value->asBoolean(&inner);
    return base::WrapUnique(new base::Value(inner));
  }
  if (value->type() == protocol::Value::TypeInteger) {
    int inner;
    value->asInteger(&inner);
    return base::WrapUnique(new base::Value(inner));
  }
  if (value->type() == protocol::Value::TypeDouble) {
    double inner;
    value->asDouble(&inner);
    return base::WrapUnique(new base::Value(inner));
  }
  if (value->type() == protocol::Value::TypeString) {
    std::string inner;
    value->asString(&inner);
    return base::WrapUnique(new base::Value(inner));
  }
  if (value->type() == protocol::Value::TypeArray) {
    protocol::ListValue* list = protocol::ListValue::cast(value);
    std::unique_ptr<base::ListValue> result(new base::ListValue());
    for (size_t i = 0; i < list->size(); i++) {
      std::unique_ptr<base::Value> converted =
          toBaseValue(list->at(i), depth - 1);
      if (converted)
        result->Append(std::move(converted));
    }
    return std::move(result);
  }
  if (value->type() == protocol::Value::TypeObject) {
    protocol::DictionaryValue* dict = protocol::DictionaryValue::cast(value);
    std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
    for (size_t i = 0; i < dict->size(); i++) {
      protocol::DictionaryValue::Entry entry = dict->at(i);
      std::unique_ptr<base::Value> converted =
          toBaseValue(entry.second, depth - 1);
      if (converted)
        result->SetWithoutPathExpansion(entry.first, std::move(converted));
    }
    return std::move(result);
  }
  return nullptr;
}

// static
std::unique_ptr<protocol::Value> StringUtil::parseJSON(
    const std::string& json) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(json);
  return toProtocolValue(value.get(), 1000);
}

StringBuilder::StringBuilder() {}

StringBuilder::~StringBuilder() {}

void StringBuilder::append(const std::string& s) {
  string_ += s;
}

void StringBuilder::append(char c) {
  string_ += c;
}

void StringBuilder::append(const char* characters, size_t length) {
  string_.append(characters, length);
}

// static
void StringUtil::builderAppendQuotedString(StringBuilder& builder,
                                           const String& str) {
  builder.append('"');
  base::string16 str16 = base::UTF8ToUTF16(str);
  escapeWideStringForJSON(reinterpret_cast<const uint16_t*>(&str16[0]),
                          str16.length(), &builder);
  builder.append('"');
}

std::string StringBuilder::toString() {
  return string_;
}

void StringBuilder::reserveCapacity(size_t capacity) {
  string_.reserve(capacity);
}

Binary::Binary() : bytes_(new base::RefCountedBytes) {}
Binary::Binary(const Binary& binary) : bytes_(binary.bytes_) {}
Binary::Binary(scoped_refptr<base::RefCountedMemory> bytes) : bytes_(bytes) {}
Binary::~Binary() {}

String Binary::toBase64() const {
  std::string encoded;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(bytes_->front()),
                        bytes_->size()),
      &encoded);
  return encoded;
}

// static
Binary Binary::fromBase64(const String& base64, bool* success) {
  std::string decoded;
  *success = base::Base64Decode(base::StringPiece(base64), &decoded);
  if (*success) {
    return Binary::fromString(std::move(decoded));
  }
  return Binary();
}

// static
Binary Binary::fromRefCounted(scoped_refptr<base::RefCountedMemory> memory) {
  return Binary(memory);
}

// static
Binary Binary::fromVector(std::vector<uint8_t> data) {
  return Binary(base::RefCountedBytes::TakeVector(&data));
}

// static
Binary Binary::fromString(std::string data) {
  return Binary(base::RefCountedString::TakeString(&data));
}
}  // namespace protocol
}  // namespace content
