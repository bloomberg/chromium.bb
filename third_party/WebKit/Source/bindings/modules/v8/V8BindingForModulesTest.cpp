/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/modules/v8/V8BindingForModules.h"

#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/serialization/SerializationTag.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBValue.h"
#include "platform/SharedBuffer.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/WebData.h"
#include "public/platform/WebString.h"
#include "public/platform/modules/indexeddb/WebIDBKey.h"
#include "public/platform/modules/indexeddb/WebIDBKeyPath.h"
#include "public/platform/modules/indexeddb/WebIDBValue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

IDBKey* CheckKeyFromValueAndKeyPathInternal(v8::Isolate* isolate,
                                            const ScriptValue& value,
                                            const String& key_path) {
  IDBKeyPath idb_key_path(key_path);
  EXPECT_TRUE(idb_key_path.IsValid());

  NonThrowableExceptionState exception_state;
  return ScriptValue::To<IDBKey*>(isolate, value, exception_state,
                                  idb_key_path);
}

void CheckKeyPathNullValue(v8::Isolate* isolate,
                           const ScriptValue& value,
                           const String& key_path) {
  ASSERT_FALSE(CheckKeyFromValueAndKeyPathInternal(isolate, value, key_path));
}

bool InjectKey(ScriptState* script_state,
               IDBKey* key,
               ScriptValue& value,
               const String& key_path) {
  IDBKeyPath idb_key_path(key_path);
  EXPECT_TRUE(idb_key_path.IsValid());
  ScriptValue key_value = ScriptValue::From(script_state, key);
  return InjectV8KeyIntoV8Value(script_state->GetIsolate(), key_value.V8Value(),
                                value.V8Value(), idb_key_path);
}

void CheckInjection(ScriptState* script_state,
                    IDBKey* key,
                    ScriptValue& value,
                    const String& key_path) {
  bool result = InjectKey(script_state, key, value, key_path);
  ASSERT_TRUE(result);
  IDBKey* extracted_key = CheckKeyFromValueAndKeyPathInternal(
      script_state->GetIsolate(), value, key_path);
  EXPECT_TRUE(key->IsEqual(extracted_key));
}

void CheckInjectionIgnored(ScriptState* script_state,
                           IDBKey* key,
                           ScriptValue& value,
                           const String& key_path) {
  bool result = InjectKey(script_state, key, value, key_path);
  ASSERT_TRUE(result);
  IDBKey* extracted_key = CheckKeyFromValueAndKeyPathInternal(
      script_state->GetIsolate(), value, key_path);
  EXPECT_FALSE(key->IsEqual(extracted_key));
}

void CheckInjectionDisallowed(ScriptState* script_state,
                              ScriptValue& value,
                              const String& key_path) {
  const IDBKeyPath idb_key_path(key_path);
  ASSERT_TRUE(idb_key_path.IsValid());
  EXPECT_FALSE(CanInjectIDBKeyIntoScriptValue(script_state->GetIsolate(), value,
                                              idb_key_path));
}

void CheckKeyPathStringValue(v8::Isolate* isolate,
                             const ScriptValue& value,
                             const String& key_path,
                             const String& expected) {
  IDBKey* idb_key =
      CheckKeyFromValueAndKeyPathInternal(isolate, value, key_path);
  ASSERT_TRUE(idb_key);
  ASSERT_EQ(IDBKey::kStringType, idb_key->GetType());
  ASSERT_TRUE(expected == idb_key->GetString());
}

void CheckKeyPathNumberValue(v8::Isolate* isolate,
                             const ScriptValue& value,
                             const String& key_path,
                             int expected) {
  IDBKey* idb_key =
      CheckKeyFromValueAndKeyPathInternal(isolate, value, key_path);
  ASSERT_TRUE(idb_key);
  ASSERT_EQ(IDBKey::kNumberType, idb_key->GetType());
  ASSERT_TRUE(expected == idb_key->Number());
}

// SerializedScriptValue header format offsets are inferred from the Blink and
// V8 serialization code. The code below DCHECKs that
constexpr static size_t kSSVHeaderBlinkVersionTagOffset = 0;
constexpr static size_t kSSVHeaderBlinkVersionOffset = 1;
constexpr static size_t kSSVHeaderV8VersionTagOffset = 2;
// constexpr static size_t kSSVHeaderV8VersionOffset = 3;

// Follows the same steps as the IndexedDB value serialization code.
void SerializeV8Value(v8::Local<v8::Value> value,
                      v8::Isolate* isolate,
                      Vector<char>* wire_bytes) {
  NonThrowableExceptionState non_throwable_exception_state;

  SerializedScriptValue::SerializeOptions options;
  scoped_refptr<SerializedScriptValue> serialized_value =
      SerializedScriptValue::Serialize(isolate, value, options,
                                       non_throwable_exception_state);
  serialized_value->ToWireBytes(*wire_bytes);

  // Sanity check that the serialization header has not changed, as the tests
  // that use this method rely on the header format.
  //
  // The cast from char* to unsigned char* is necessary to avoid VS2015 warning
  // C4309 (truncation of constant value). This happens because VersionTag is
  // 0xFF.
  const unsigned char* wire_data =
      reinterpret_cast<unsigned char*>(wire_bytes->data());
  ASSERT_EQ(static_cast<unsigned char>(kVersionTag),
            wire_data[kSSVHeaderBlinkVersionTagOffset]);
  ASSERT_EQ(
      static_cast<unsigned char>(SerializedScriptValue::kWireFormatVersion),
      wire_data[kSSVHeaderBlinkVersionOffset]);

  ASSERT_EQ(static_cast<unsigned char>(kVersionTag),
            wire_data[kSSVHeaderV8VersionTagOffset]);
  // TODO(jbroman): Use the compile-time constant for V8 data format version.
  // ASSERT_EQ(v8::ValueSerializer::GetCurrentDataFormatVersion(),
  //           wire_data[kSSVHeaderV8VersionOffset]);
}

scoped_refptr<IDBValue> CreateIDBValue(v8::Isolate* isolate,
                                       Vector<char>& wire_bytes,
                                       double primary_key,
                                       const WebString& key_path) {
  WebData web_data(SharedBuffer::AdoptVector(wire_bytes));
  Vector<WebBlobInfo> web_blob_info;
  WebIDBKey web_idb_key = WebIDBKey::CreateNumber(primary_key);
  WebIDBKeyPath web_idb_key_path(key_path);
  WebIDBValue web_idb_value(web_data, web_blob_info, web_idb_key,
                            web_idb_key_path);
  return IDBValue::Create(web_idb_value, isolate);
}

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyStringValue) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // object = { foo: "zoo" }
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  ASSERT_TRUE(V8CallBoolean(object->Set(scope.GetContext(),
                                        V8AtomicString(isolate, "foo"),
                                        V8AtomicString(isolate, "zoo"))));

  ScriptValue script_value(scope.GetScriptState(), object);

  CheckKeyPathStringValue(isolate, script_value, "foo", "zoo");
  CheckKeyPathNullValue(isolate, script_value, "bar");
}

}  // namespace

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyNumberValue) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // object = { foo: 456 }
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  ASSERT_TRUE(V8CallBoolean(object->Set(scope.GetContext(),
                                        V8AtomicString(isolate, "foo"),
                                        v8::Number::New(isolate, 456))));

  ScriptValue script_value(scope.GetScriptState(), object);

  CheckKeyPathNumberValue(isolate, script_value, "foo", 456);
  CheckKeyPathNullValue(isolate, script_value, "bar");
}

TEST(IDBKeyFromValueAndKeyPathTest, SubProperty) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // object = { foo: { bar: "zee" } }
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  v8::Local<v8::Object> sub_property = v8::Object::New(isolate);
  ASSERT_TRUE(V8CallBoolean(sub_property->Set(scope.GetContext(),
                                              V8AtomicString(isolate, "bar"),
                                              V8AtomicString(isolate, "zee"))));
  ASSERT_TRUE(V8CallBoolean(object->Set(
      scope.GetContext(), V8AtomicString(isolate, "foo"), sub_property)));

  ScriptValue script_value(scope.GetScriptState(), object);

  CheckKeyPathStringValue(isolate, script_value, "foo.bar", "zee");
  CheckKeyPathNullValue(isolate, script_value, "bar");
}

TEST(InjectIDBKeyTest, ImplicitValues) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  {
    v8::Local<v8::String> string = V8String(isolate, "string");
    ScriptValue value = ScriptValue(scope.GetScriptState(), string);
    CheckInjectionIgnored(scope.GetScriptState(), IDBKey::CreateNumber(123),
                          value, "length");
  }
  {
    v8::Local<v8::Array> array = v8::Array::New(isolate);
    ScriptValue value = ScriptValue(scope.GetScriptState(), array);
    CheckInjectionIgnored(scope.GetScriptState(), IDBKey::CreateNumber(456),
                          value, "length");
  }
}

TEST(InjectIDBKeyTest, TopLevelPropertyStringValue) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // object = { foo: "zoo" }
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  ASSERT_TRUE(V8CallBoolean(object->Set(scope.GetContext(),
                                        V8AtomicString(isolate, "foo"),
                                        V8AtomicString(isolate, "zoo"))));

  ScriptValue script_object(scope.GetScriptState(), object);
  CheckInjection(scope.GetScriptState(), IDBKey::CreateString("myNewKey"),
                 script_object, "bar");
  CheckInjection(scope.GetScriptState(), IDBKey::CreateNumber(1234),
                 script_object, "bar");

  CheckInjectionDisallowed(scope.GetScriptState(), script_object, "foo.bar");
}

TEST(InjectIDBKeyTest, SubProperty) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // object = { foo: { bar: "zee" } }
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  v8::Local<v8::Object> sub_property = v8::Object::New(isolate);
  ASSERT_TRUE(V8CallBoolean(sub_property->Set(scope.GetContext(),
                                              V8AtomicString(isolate, "bar"),
                                              V8AtomicString(isolate, "zee"))));
  ASSERT_TRUE(V8CallBoolean(object->Set(
      scope.GetContext(), V8AtomicString(isolate, "foo"), sub_property)));

  ScriptValue script_object(scope.GetScriptState(), object);
  CheckInjection(scope.GetScriptState(), IDBKey::CreateString("myNewKey"),
                 script_object, "foo.baz");
  CheckInjection(scope.GetScriptState(), IDBKey::CreateNumber(789),
                 script_object, "foo.baz");
  CheckInjection(scope.GetScriptState(), IDBKey::CreateDate(4567),
                 script_object, "foo.baz");
  CheckInjection(scope.GetScriptState(), IDBKey::CreateDate(4567),
                 script_object, "bar");
  CheckInjection(scope.GetScriptState(),
                 IDBKey::CreateArray(IDBKey::KeyArray()), script_object,
                 "foo.baz");
  CheckInjection(scope.GetScriptState(),
                 IDBKey::CreateArray(IDBKey::KeyArray()), script_object, "bar");

  CheckInjectionDisallowed(scope.GetScriptState(), script_object,
                           "foo.bar.baz");
  CheckInjection(scope.GetScriptState(), IDBKey::CreateString("zoo"),
                 script_object, "foo.xyz.foo");
}

TEST(DeserializeIDBValueTest, CurrentVersions) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  Vector<char> object_bytes;
  v8::Local<v8::Object> empty_object = v8::Object::New(isolate);
  SerializeV8Value(empty_object, isolate, &object_bytes);
  scoped_refptr<IDBValue> idb_value =
      CreateIDBValue(isolate, object_bytes, 42.0, "foo");

  v8::Local<v8::Value> v8_value = DeserializeIDBValue(
      isolate, scope.GetContext()->Global(), idb_value.get());
  EXPECT_TRUE(!scope.GetExceptionState().HadException());

  ASSERT_TRUE(v8_value->IsObject());
  v8::Local<v8::Object> v8_value_object = v8_value.As<v8::Object>();
  v8::Local<v8::Value> v8_number_value =
      v8_value_object->Get(scope.GetContext(), V8AtomicString(isolate, "foo"))
          .ToLocalChecked();
  ASSERT_TRUE(v8_number_value->IsNumber());
  v8::Local<v8::Number> v8_number = v8_number_value.As<v8::Number>();
  EXPECT_EQ(v8_number->Value(), 42.0);
}

TEST(DeserializeIDBValueTest, FutureV8Version) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Pretend that the object was serialized by a future version of V8.
  Vector<char> object_bytes;
  v8::Local<v8::Object> empty_object = v8::Object::New(isolate);
  SerializeV8Value(empty_object, isolate, &object_bytes);
  object_bytes[kSSVHeaderV8VersionTagOffset] += 1;

  // The call sequence below mimics IndexedDB's usage pattern when attempting to
  // read a value in an object store with a key generator and a key path, but
  // the serialized value uses a newer format version.
  //
  // http://crbug.com/703704 has a reproduction for this test's circumstances.
  scoped_refptr<IDBValue> idb_value =
      CreateIDBValue(isolate, object_bytes, 42.0, "foo");

  v8::Local<v8::Value> v8_value = DeserializeIDBValue(
      isolate, scope.GetContext()->Global(), idb_value.get());
  EXPECT_TRUE(!scope.GetExceptionState().HadException());
  EXPECT_TRUE(v8_value->IsNull());
}

TEST(DeserializeIDBValueTest, InjectionIntoNonObject) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Simulate a storage corruption where an object is read back as a number.
  // This test uses a one-segment key path.
  Vector<char> object_bytes;
  v8::Local<v8::Number> number = v8::Number::New(isolate, 42.0);
  SerializeV8Value(number, isolate, &object_bytes);
  scoped_refptr<IDBValue> idb_value =
      CreateIDBValue(isolate, object_bytes, 42.0, "foo");

  v8::Local<v8::Value> v8_value = DeserializeIDBValue(
      isolate, scope.GetContext()->Global(), idb_value.get());
  EXPECT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(v8_value->IsNumber());
  v8::Local<v8::Number> v8_number = v8_value.As<v8::Number>();
  EXPECT_EQ(v8_number->Value(), 42.0);
}

TEST(DeserializeIDBValueTest, NestedInjectionIntoNonObject) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Simulate a storage corruption where an object is read back as a number.
  // This test uses a multiple-segment key path.
  Vector<char> object_bytes;
  v8::Local<v8::Number> number = v8::Number::New(isolate, 42.0);
  SerializeV8Value(number, isolate, &object_bytes);
  scoped_refptr<IDBValue> idb_value =
      CreateIDBValue(isolate, object_bytes, 42.0, "foo.bar");

  v8::Local<v8::Value> v8_value = DeserializeIDBValue(
      isolate, scope.GetContext()->Global(), idb_value.get());
  EXPECT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(v8_value->IsNumber());
  v8::Local<v8::Number> v8_number = v8_value.As<v8::Number>();
  EXPECT_EQ(v8_number->Value(), 42.0);
}

}  // namespace blink
