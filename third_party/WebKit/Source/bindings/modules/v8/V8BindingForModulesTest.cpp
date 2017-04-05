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

#include "bindings/core/v8/SerializationTag.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBValue.h"
#include "platform/SharedBuffer.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/WebData.h"
#include "public/platform/WebString.h"
#include "public/platform/modules/indexeddb/WebIDBKey.h"
#include "public/platform/modules/indexeddb/WebIDBKeyPath.h"
#include "public/platform/modules/indexeddb/WebIDBValue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

IDBKey* checkKeyFromValueAndKeyPathInternal(v8::Isolate* isolate,
                                            const ScriptValue& value,
                                            const String& keyPath) {
  IDBKeyPath idbKeyPath(keyPath);
  EXPECT_TRUE(idbKeyPath.isValid());

  NonThrowableExceptionState exceptionState;
  return ScriptValue::to<IDBKey*>(isolate, value, exceptionState, idbKeyPath);
}

void checkKeyPathNullValue(v8::Isolate* isolate,
                           const ScriptValue& value,
                           const String& keyPath) {
  ASSERT_FALSE(checkKeyFromValueAndKeyPathInternal(isolate, value, keyPath));
}

bool injectKey(ScriptState* scriptState,
               IDBKey* key,
               ScriptValue& value,
               const String& keyPath) {
  IDBKeyPath idbKeyPath(keyPath);
  EXPECT_TRUE(idbKeyPath.isValid());
  ScriptValue keyValue = ScriptValue::from(scriptState, key);
  return injectV8KeyIntoV8Value(scriptState->isolate(), keyValue.v8Value(),
                                value.v8Value(), idbKeyPath);
}

void checkInjection(ScriptState* scriptState,
                    IDBKey* key,
                    ScriptValue& value,
                    const String& keyPath) {
  bool result = injectKey(scriptState, key, value, keyPath);
  ASSERT_TRUE(result);
  IDBKey* extractedKey = checkKeyFromValueAndKeyPathInternal(
      scriptState->isolate(), value, keyPath);
  EXPECT_TRUE(key->isEqual(extractedKey));
}

void checkInjectionIgnored(ScriptState* scriptState,
                           IDBKey* key,
                           ScriptValue& value,
                           const String& keyPath) {
  bool result = injectKey(scriptState, key, value, keyPath);
  ASSERT_TRUE(result);
  IDBKey* extractedKey = checkKeyFromValueAndKeyPathInternal(
      scriptState->isolate(), value, keyPath);
  EXPECT_FALSE(key->isEqual(extractedKey));
}

void checkInjectionDisallowed(ScriptState* scriptState,
                              ScriptValue& value,
                              const String& keyPath) {
  const IDBKeyPath idbKeyPath(keyPath);
  ASSERT_TRUE(idbKeyPath.isValid());
  EXPECT_FALSE(canInjectIDBKeyIntoScriptValue(scriptState->isolate(), value,
                                              idbKeyPath));
}

void checkKeyPathStringValue(v8::Isolate* isolate,
                             const ScriptValue& value,
                             const String& keyPath,
                             const String& expected) {
  IDBKey* idbKey = checkKeyFromValueAndKeyPathInternal(isolate, value, keyPath);
  ASSERT_TRUE(idbKey);
  ASSERT_EQ(IDBKey::StringType, idbKey->getType());
  ASSERT_TRUE(expected == idbKey->string());
}

void checkKeyPathNumberValue(v8::Isolate* isolate,
                             const ScriptValue& value,
                             const String& keyPath,
                             int expected) {
  IDBKey* idbKey = checkKeyFromValueAndKeyPathInternal(isolate, value, keyPath);
  ASSERT_TRUE(idbKey);
  ASSERT_EQ(IDBKey::NumberType, idbKey->getType());
  ASSERT_TRUE(expected == idbKey->number());
}

// SerializedScriptValue header format offsets are inferred from the Blink and
// V8 serialization code. The code below DCHECKs that
constexpr static size_t kSSVHeaderBlinkVersionOffset = 0;
constexpr static size_t kSSVHeaderBlinkVersionTagOffset = 1;
constexpr static size_t kSSVHeaderV8VersionOffset = 2;
constexpr static size_t kSSVHeaderV8VersionTagOffset = 3;

// 13 is v8::internal::kLatestVersion in v8/src/value-serializer.cc at the
// time when this test was written. Unlike Blink, V8 does not currently export
// its serialization version, so this number might get stale.
constexpr static unsigned char kV8LatestKnownVersion = 13;

// Follows the same steps as the IndexedDB value serialization code.
void serializeV8Value(v8::Local<v8::Value> value,
                      v8::Isolate* isolate,
                      Vector<char>* wireBytes) {
  NonThrowableExceptionState nonThrowableExceptionState;

  SerializedScriptValue::SerializeOptions options;
  RefPtr<SerializedScriptValue> serializedValue =
      SerializedScriptValue::serialize(isolate, value, options,
                                       nonThrowableExceptionState);
  serializedValue->toWireBytes(*wireBytes);

  // Sanity check that the serialization header has not changed, as the tests
  // that use this method rely on the header format.
  //
  // The cast from char* to unsigned char* is necessary to avoid VS2015 warning
  // C4309 (truncation of constant value). This happens because VersionTag is
  // 0xFF.
  const unsigned char* wireData =
      reinterpret_cast<unsigned char*>(wireBytes->data());
  ASSERT_EQ(
      static_cast<unsigned char>(SerializedScriptValue::wireFormatVersion),
      wireData[kSSVHeaderBlinkVersionOffset]);
  ASSERT_EQ(static_cast<unsigned char>(VersionTag),
            wireData[kSSVHeaderBlinkVersionTagOffset]);

  ASSERT_GE(static_cast<unsigned char>(kV8LatestKnownVersion),
            wireData[kSSVHeaderV8VersionOffset]);
  ASSERT_EQ(static_cast<unsigned char>(VersionTag),
            wireData[kSSVHeaderV8VersionTagOffset]);
}

PassRefPtr<IDBValue> createIDBValue(v8::Isolate* isolate,
                                    Vector<char>& wireBytes,
                                    double primaryKey,
                                    const WebString& keyPath) {
  WebData webData(SharedBuffer::adoptVector(wireBytes));
  Vector<WebBlobInfo> webBlobInfo;
  WebIDBKey webIdbKey = WebIDBKey::createNumber(primaryKey);
  WebIDBKeyPath webIdbKeyPath(keyPath);
  WebIDBValue webIdbValue(webData, webBlobInfo, webIdbKey, webIdbKeyPath);
  return IDBValue::create(webIdbValue, isolate);
}

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyStringValue) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.isolate();

  // object = { foo: "zoo" }
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  ASSERT_TRUE(
      v8CallBoolean(object->Set(scope.context(), v8AtomicString(isolate, "foo"),
                                v8AtomicString(isolate, "zoo"))));

  ScriptValue scriptValue(scope.getScriptState(), object);

  checkKeyPathStringValue(isolate, scriptValue, "foo", "zoo");
  checkKeyPathNullValue(isolate, scriptValue, "bar");
}

}  // namespace

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyNumberValue) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.isolate();

  // object = { foo: 456 }
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  ASSERT_TRUE(
      v8CallBoolean(object->Set(scope.context(), v8AtomicString(isolate, "foo"),
                                v8::Number::New(isolate, 456))));

  ScriptValue scriptValue(scope.getScriptState(), object);

  checkKeyPathNumberValue(isolate, scriptValue, "foo", 456);
  checkKeyPathNullValue(isolate, scriptValue, "bar");
}

TEST(IDBKeyFromValueAndKeyPathTest, SubProperty) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.isolate();

  // object = { foo: { bar: "zee" } }
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  v8::Local<v8::Object> subProperty = v8::Object::New(isolate);
  ASSERT_TRUE(v8CallBoolean(subProperty->Set(scope.context(),
                                             v8AtomicString(isolate, "bar"),
                                             v8AtomicString(isolate, "zee"))));
  ASSERT_TRUE(v8CallBoolean(object->Set(
      scope.context(), v8AtomicString(isolate, "foo"), subProperty)));

  ScriptValue scriptValue(scope.getScriptState(), object);

  checkKeyPathStringValue(isolate, scriptValue, "foo.bar", "zee");
  checkKeyPathNullValue(isolate, scriptValue, "bar");
}

TEST(InjectIDBKeyTest, ImplicitValues) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.isolate();
  {
    v8::Local<v8::String> string = v8String(isolate, "string");
    ScriptValue value = ScriptValue(scope.getScriptState(), string);
    checkInjectionIgnored(scope.getScriptState(), IDBKey::createNumber(123),
                          value, "length");
  }
  {
    v8::Local<v8::Array> array = v8::Array::New(isolate);
    ScriptValue value = ScriptValue(scope.getScriptState(), array);
    checkInjectionIgnored(scope.getScriptState(), IDBKey::createNumber(456),
                          value, "length");
  }
}

TEST(InjectIDBKeyTest, TopLevelPropertyStringValue) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.isolate();

  // object = { foo: "zoo" }
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  ASSERT_TRUE(
      v8CallBoolean(object->Set(scope.context(), v8AtomicString(isolate, "foo"),
                                v8AtomicString(isolate, "zoo"))));

  ScriptValue scriptObject(scope.getScriptState(), object);
  checkInjection(scope.getScriptState(), IDBKey::createString("myNewKey"),
                 scriptObject, "bar");
  checkInjection(scope.getScriptState(), IDBKey::createNumber(1234),
                 scriptObject, "bar");

  checkInjectionDisallowed(scope.getScriptState(), scriptObject, "foo.bar");
}

TEST(InjectIDBKeyTest, SubProperty) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.isolate();

  // object = { foo: { bar: "zee" } }
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  v8::Local<v8::Object> subProperty = v8::Object::New(isolate);
  ASSERT_TRUE(v8CallBoolean(subProperty->Set(scope.context(),
                                             v8AtomicString(isolate, "bar"),
                                             v8AtomicString(isolate, "zee"))));
  ASSERT_TRUE(v8CallBoolean(object->Set(
      scope.context(), v8AtomicString(isolate, "foo"), subProperty)));

  ScriptValue scriptObject(scope.getScriptState(), object);
  checkInjection(scope.getScriptState(), IDBKey::createString("myNewKey"),
                 scriptObject, "foo.baz");
  checkInjection(scope.getScriptState(), IDBKey::createNumber(789),
                 scriptObject, "foo.baz");
  checkInjection(scope.getScriptState(), IDBKey::createDate(4567), scriptObject,
                 "foo.baz");
  checkInjection(scope.getScriptState(), IDBKey::createDate(4567), scriptObject,
                 "bar");
  checkInjection(scope.getScriptState(),
                 IDBKey::createArray(IDBKey::KeyArray()), scriptObject,
                 "foo.baz");
  checkInjection(scope.getScriptState(),
                 IDBKey::createArray(IDBKey::KeyArray()), scriptObject, "bar");

  checkInjectionDisallowed(scope.getScriptState(), scriptObject, "foo.bar.baz");
  checkInjection(scope.getScriptState(), IDBKey::createString("zoo"),
                 scriptObject, "foo.xyz.foo");
}

TEST(DeserializeIDBValueTest, CurrentVersions) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.isolate();

  Vector<char> objectBytes;
  v8::Local<v8::Object> emptyObject = v8::Object::New(isolate);
  serializeV8Value(emptyObject, isolate, &objectBytes);
  RefPtr<IDBValue> idbValue = createIDBValue(isolate, objectBytes, 42.0, "foo");

  v8::Local<v8::Value> v8Value =
      deserializeIDBValue(isolate, scope.context()->Global(), idbValue.get());
  EXPECT_TRUE(!scope.getExceptionState().hadException());

  ASSERT_TRUE(v8Value->IsObject());
  v8::Local<v8::Object> v8ValueObject = v8Value.As<v8::Object>();
  v8::Local<v8::Value> v8NumberValue =
      v8ValueObject->Get(scope.context(), v8AtomicString(isolate, "foo"))
          .ToLocalChecked();
  ASSERT_TRUE(v8NumberValue->IsNumber());
  v8::Local<v8::Number> v8Number = v8NumberValue.As<v8::Number>();
  EXPECT_EQ(v8Number->Value(), 42.0);
}

TEST(DeserializeIDBValueTest, FutureV8Version) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.isolate();

  // Pretend that the object was serialized by a future version of V8.
  Vector<char> objectBytes;
  v8::Local<v8::Object> emptyObject = v8::Object::New(isolate);
  serializeV8Value(emptyObject, isolate, &objectBytes);
  objectBytes[kSSVHeaderV8VersionTagOffset] += 1;

  // The call sequence below mimics IndexedDB's usage pattern when attempting to
  // read a value in an object store with a key generator and a key path, but
  // the serialized value uses a newer format version.
  //
  // http://crbug.com/703704 has a reproduction for this test's circumstances.
  RefPtr<IDBValue> idbValue = createIDBValue(isolate, objectBytes, 42.0, "foo");

  v8::Local<v8::Value> v8Value =
      deserializeIDBValue(isolate, scope.context()->Global(), idbValue.get());
  EXPECT_TRUE(!scope.getExceptionState().hadException());
  EXPECT_TRUE(v8Value->IsNull());
}

TEST(DeserializeIDBValueTest, InjectionIntoNonObject) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.isolate();

  // Simulate a storage corruption where an object is read back as a number.
  // This test uses a one-segment key path.
  Vector<char> objectBytes;
  v8::Local<v8::Number> number = v8::Number::New(isolate, 42.0);
  serializeV8Value(number, isolate, &objectBytes);
  RefPtr<IDBValue> idbValue = createIDBValue(isolate, objectBytes, 42.0, "foo");

  v8::Local<v8::Value> v8Value =
      deserializeIDBValue(isolate, scope.context()->Global(), idbValue.get());
  EXPECT_TRUE(!scope.getExceptionState().hadException());
  ASSERT_TRUE(v8Value->IsNumber());
  v8::Local<v8::Number> v8Number = v8Value.As<v8::Number>();
  EXPECT_EQ(v8Number->Value(), 42.0);
}

TEST(DeserializeIDBValueTest, NestedInjectionIntoNonObject) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.isolate();

  // Simulate a storage corruption where an object is read back as a number.
  // This test uses a multiple-segment key path.
  Vector<char> objectBytes;
  v8::Local<v8::Number> number = v8::Number::New(isolate, 42.0);
  serializeV8Value(number, isolate, &objectBytes);
  RefPtr<IDBValue> idbValue =
      createIDBValue(isolate, objectBytes, 42.0, "foo.bar");

  v8::Local<v8::Value> v8Value =
      deserializeIDBValue(isolate, scope.context()->Global(), idbValue.get());
  EXPECT_TRUE(!scope.getExceptionState().hadException());
  ASSERT_TRUE(v8Value->IsNumber());
  v8::Local<v8::Number> v8Number = v8Value.As<v8::Number>();
  EXPECT_EQ(v8Number->Value(), 42.0);
}

}  // namespace blink
