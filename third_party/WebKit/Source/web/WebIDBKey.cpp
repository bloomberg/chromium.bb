/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/platform/modules/indexeddb/WebIDBKey.h"

#include "modules/indexeddb/IDBKey.h"

namespace blink {

WebIDBKey WebIDBKey::CreateArray(const WebVector<WebIDBKey>& array) {
  WebIDBKey key;
  key.AssignArray(array);
  return key;
}

WebIDBKey WebIDBKey::CreateBinary(const WebData& binary) {
  WebIDBKey key;
  key.AssignBinary(binary);
  return key;
}

WebIDBKey WebIDBKey::CreateString(const WebString& string) {
  WebIDBKey key;
  key.AssignString(string);
  return key;
}

WebIDBKey WebIDBKey::CreateDate(double date) {
  WebIDBKey key;
  key.AssignDate(date);
  return key;
}

WebIDBKey WebIDBKey::CreateNumber(double number) {
  WebIDBKey key;
  key.AssignNumber(number);
  return key;
}

WebIDBKey WebIDBKey::CreateInvalid() {
  WebIDBKey key;
  key.AssignInvalid();
  return key;
}

WebIDBKey WebIDBKey::CreateNull() {
  WebIDBKey key;
  key.AssignNull();
  return key;
}

void WebIDBKey::Reset() {
  private_.Reset();
}

void WebIDBKey::Assign(const WebIDBKey& value) {
  private_ = value.private_;
}

static IDBKey* ConvertFromWebIDBKeyArray(const WebVector<WebIDBKey>& array) {
  IDBKey::KeyArray keys;
  keys.ReserveCapacity(array.size());
  for (size_t i = 0; i < array.size(); ++i) {
    switch (array[i].KeyType()) {
      case kWebIDBKeyTypeArray:
        keys.push_back(ConvertFromWebIDBKeyArray(array[i].Array()));
        break;
      case kWebIDBKeyTypeBinary:
        keys.push_back(IDBKey::CreateBinary(array[i].Binary()));
        break;
      case kWebIDBKeyTypeString:
        keys.push_back(IDBKey::CreateString(array[i].GetString()));
        break;
      case kWebIDBKeyTypeDate:
        keys.push_back(IDBKey::CreateDate(array[i].Date()));
        break;
      case kWebIDBKeyTypeNumber:
        keys.push_back(IDBKey::CreateNumber(array[i].Number()));
        break;
      case kWebIDBKeyTypeInvalid:
        keys.push_back(IDBKey::CreateInvalid());
        break;
      case kWebIDBKeyTypeNull:
      case kWebIDBKeyTypeMin:
        NOTREACHED();
        break;
    }
  }
  return IDBKey::CreateArray(keys);
}

static void ConvertToWebIDBKeyArray(const IDBKey::KeyArray& array,
                                    WebVector<WebIDBKey>& result) {
  WebVector<WebIDBKey> keys(array.size());
  WebVector<WebIDBKey> subkeys;
  for (size_t i = 0; i < array.size(); ++i) {
    IDBKey* key = array[i];
    switch (key->GetType()) {
      case IDBKey::kArrayType:
        ConvertToWebIDBKeyArray(key->Array(), subkeys);
        keys[i] = WebIDBKey::CreateArray(subkeys);
        break;
      case IDBKey::kBinaryType:
        keys[i] = WebIDBKey::CreateBinary(key->Binary());
        break;
      case IDBKey::kStringType:
        keys[i] = WebIDBKey::CreateString(key->GetString());
        break;
      case IDBKey::kDateType:
        keys[i] = WebIDBKey::CreateDate(key->Date());
        break;
      case IDBKey::kNumberType:
        keys[i] = WebIDBKey::CreateNumber(key->Number());
        break;
      case IDBKey::kInvalidType:
        keys[i] = WebIDBKey::CreateInvalid();
        break;
      case IDBKey::kTypeEnumMax:
        NOTREACHED();
        break;
    }
  }
  result.Swap(keys);
}

void WebIDBKey::AssignArray(const WebVector<WebIDBKey>& array) {
  private_ = ConvertFromWebIDBKeyArray(array);
}

void WebIDBKey::AssignBinary(const WebData& binary) {
  private_ = IDBKey::CreateBinary(binary);
}

void WebIDBKey::AssignString(const WebString& string) {
  private_ = IDBKey::CreateString(string);
}

void WebIDBKey::AssignDate(double date) {
  private_ = IDBKey::CreateDate(date);
}

void WebIDBKey::AssignNumber(double number) {
  private_ = IDBKey::CreateNumber(number);
}

void WebIDBKey::AssignInvalid() {
  private_ = IDBKey::CreateInvalid();
}

void WebIDBKey::AssignNull() {
  private_.Reset();
}

WebIDBKeyType WebIDBKey::KeyType() const {
  if (!private_.Get())
    return kWebIDBKeyTypeNull;
  return static_cast<WebIDBKeyType>(private_->GetType());
}

bool WebIDBKey::IsValid() const {
  if (!private_.Get())
    return false;
  return private_->IsValid();
}

WebVector<WebIDBKey> WebIDBKey::Array() const {
  WebVector<WebIDBKey> keys;
  ConvertToWebIDBKeyArray(private_->Array(), keys);
  return keys;
}

WebData WebIDBKey::Binary() const {
  return private_->Binary();
}

WebString WebIDBKey::GetString() const {
  return private_->GetString();
}

double WebIDBKey::Date() const {
  return private_->Date();
}

double WebIDBKey::Number() const {
  return private_->Number();
}

}  // namespace blink
