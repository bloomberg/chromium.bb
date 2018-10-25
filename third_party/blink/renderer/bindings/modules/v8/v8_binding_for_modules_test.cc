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

#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key_path.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_value.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialization_tag.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/to_v8_for_modules.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_any.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

namespace {

std::unique_ptr<IDBKey> CheckKeyFromValueAndKeyPathInternal(
    v8::Isolate* isolate,
    const ScriptValue& value,
    const String& key_path) {
  IDBKeyPath idb_key_path(key_path);
  EXPECT_TRUE(idb_key_path.IsValid());

  NonThrowableExceptionState exception_state;
  return ScriptValue::To<std::unique_ptr<IDBKey>>(
      isolate, value, exception_state, idb_key_path);
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
  std::unique_ptr<IDBKey> extracted_key = CheckKeyFromValueAndKeyPathInternal(
      script_state->GetIsolate(), value, key_path);
  EXPECT_TRUE(key->IsEqual(extracted_key.get()));
}

void CheckInjectionIgnored(ScriptState* script_state,
                           IDBKey* key,
                           ScriptValue& value,
                           const String& key_path) {
  bool result = InjectKey(script_state, key, value, key_path);
  ASSERT_TRUE(result);
  std::unique_ptr<IDBKey> extracted_key = CheckKeyFromValueAndKeyPathInternal(
      script_state->GetIsolate(), value, key_path);
  EXPECT_FALSE(key->IsEqual(extracted_key.get()));
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
  std::unique_ptr<IDBKey> idb_key =
      CheckKeyFromValueAndKeyPathInternal(isolate, value, key_path);
  ASSERT_TRUE(idb_key);
  ASSERT_EQ(IDBKey::kStringType, idb_key->GetType());
  ASSERT_TRUE(expected == idb_key->GetString());
}

void CheckKeyPathNumberValue(v8::Isolate* isolate,
                             const ScriptValue& value,
                             const String& key_path,
                             int expected) {
  std::unique_ptr<IDBKey> idb_key =
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
  base::span<const uint8_t> ssv_wire_data = serialized_value->GetWireData();
  DCHECK(wire_bytes->IsEmpty());
  wire_bytes->Append(ssv_wire_data.data(),
                     static_cast<wtf_size_t>(ssv_wire_data.size()));

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

std::unique_ptr<IDBValue> CreateIDBValue(v8::Isolate* isolate,
                                         Vector<char>& wire_bytes,
                                         double primary_key,
                                         const WebString& key_path) {
  WebData web_data(SharedBuffer::AdoptVector(wire_bytes));
  WebIDBValue web_idb_value(web_data, Vector<WebBlobInfo>());
  web_idb_value.SetInjectedPrimaryKey(WebIDBKey::CreateNumber(primary_key),
                                      WebIDBKeyPath(key_path));

  std::unique_ptr<IDBValue> idb_value = web_idb_value.ReleaseIdbValue();
  idb_value->SetIsolate(isolate);
  return idb_value;
}

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyStringValue) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // object = { foo: "zoo" }
  ScriptValue script_value = V8ObjectBuilder(scope.GetScriptState())
                                 .Add("foo", "zoo")
                                 .GetScriptValue();
  CheckKeyPathStringValue(isolate, script_value, "foo", "zoo");
  CheckKeyPathNullValue(isolate, script_value, "bar");
}

}  // namespace

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyNumberValue) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // object = { foo: 456 }
  ScriptValue script_value = V8ObjectBuilder(scope.GetScriptState())
                                 .AddNumber("foo", 456)
                                 .GetScriptValue();
  CheckKeyPathNumberValue(isolate, script_value, "foo", 456);
  CheckKeyPathNullValue(isolate, script_value, "bar");
}

TEST(IDBKeyFromValueAndKeyPathTest, SubProperty) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  // object = { foo: { bar: "zee" } }
  ScriptValue script_value =
      V8ObjectBuilder(script_state)
          .Add("foo", V8ObjectBuilder(script_state).Add("bar", "zee"))
          .GetScriptValue();
  CheckKeyPathStringValue(isolate, script_value, "foo.bar", "zee");
  CheckKeyPathNullValue(isolate, script_value, "bar");
}

TEST(InjectIDBKeyTest, ImplicitValues) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  {
    v8::Local<v8::String> string = V8String(isolate, "string");
    ScriptValue value = ScriptValue(scope.GetScriptState(), string);
    std::unique_ptr<IDBKey> idb_key = IDBKey::CreateNumber(123);
    CheckInjectionIgnored(scope.GetScriptState(), idb_key.get(), value,
                          "length");
  }
  {
    v8::Local<v8::Array> array = v8::Array::New(isolate);
    ScriptValue value = ScriptValue(scope.GetScriptState(), array);
    std::unique_ptr<IDBKey> idb_key = IDBKey::CreateNumber(456);
    CheckInjectionIgnored(scope.GetScriptState(), idb_key.get(), value,
                          "length");
  }
}

TEST(InjectIDBKeyTest, TopLevelPropertyStringValue) {
  V8TestingScope scope;

  // object = { foo: "zoo" }
  ScriptValue script_object = V8ObjectBuilder(scope.GetScriptState())
                                  .Add("foo", "zoo")
                                  .GetScriptValue();
  std::unique_ptr<IDBKey> idb_string_key = IDBKey::CreateString("myNewKey");
  CheckInjection(scope.GetScriptState(), idb_string_key.get(), script_object,
                 "bar");
  std::unique_ptr<IDBKey> idb_number_key = IDBKey::CreateNumber(1234);
  CheckInjection(scope.GetScriptState(), idb_number_key.get(), script_object,
                 "bar");

  CheckInjectionDisallowed(scope.GetScriptState(), script_object, "foo.bar");
}

TEST(InjectIDBKeyTest, SubProperty) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // object = { foo: { bar: "zee" } }
  ScriptValue script_object =
      V8ObjectBuilder(script_state)
          .Add("foo", V8ObjectBuilder(script_state).Add("bar", "zee"))
          .GetScriptValue();

  std::unique_ptr<IDBKey> idb_string_key = IDBKey::CreateString("myNewKey");
  CheckInjection(scope.GetScriptState(), idb_string_key.get(), script_object,
                 "foo.baz");
  std::unique_ptr<IDBKey> idb_number_key = IDBKey::CreateNumber(789);
  CheckInjection(scope.GetScriptState(), idb_number_key.get(), script_object,
                 "foo.baz");
  std::unique_ptr<IDBKey> idb_date_key = IDBKey::CreateDate(4567);
  CheckInjection(scope.GetScriptState(), idb_date_key.get(), script_object,
                 "foo.baz");
  CheckInjection(scope.GetScriptState(), idb_date_key.get(), script_object,
                 "bar");
  std::unique_ptr<IDBKey> idb_array_key =
      IDBKey::CreateArray(IDBKey::KeyArray());
  CheckInjection(scope.GetScriptState(), idb_array_key.get(), script_object,
                 "foo.baz");
  CheckInjection(scope.GetScriptState(), idb_array_key.get(), script_object,
                 "bar");

  CheckInjectionDisallowed(scope.GetScriptState(), script_object,
                           "foo.bar.baz");
  std::unique_ptr<IDBKey> idb_zoo_key = IDBKey::CreateString("zoo");
  CheckInjection(scope.GetScriptState(), idb_zoo_key.get(), script_object,
                 "foo.xyz.foo");
}

TEST(DeserializeIDBValueTest, CurrentVersions) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  Vector<char> object_bytes;
  v8::Local<v8::Object> empty_object = v8::Object::New(isolate);
  SerializeV8Value(empty_object, isolate, &object_bytes);
  std::unique_ptr<IDBValue> idb_value =
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
  std::unique_ptr<IDBValue> idb_value =
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
  std::unique_ptr<IDBValue> idb_value =
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
  std::unique_ptr<IDBValue> idb_value =
      CreateIDBValue(isolate, object_bytes, 42.0, "foo.bar");

  v8::Local<v8::Value> v8_value = DeserializeIDBValue(
      isolate, scope.GetContext()->Global(), idb_value.get());
  EXPECT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(v8_value->IsNumber());
  v8::Local<v8::Number> v8_number = v8_value.As<v8::Number>();
  EXPECT_EQ(v8_number->Value(), 42.0);
}

}  // namespace blink
