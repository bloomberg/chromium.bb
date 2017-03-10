// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/NativeValueTraitsImpl.h"

#include <utility>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

template <typename T>
v8::Local<v8::Value> ToV8(V8TestingScope* scope, T value) {
  return blink::ToV8(value, scope->context()->Global(), scope->isolate());
}

void ThrowException(v8::Local<v8::Name>,
                    const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetIsolate()->ThrowException(v8String(info.GetIsolate(), "bogus!"));
}

void ReturnBogusObjectDescriptor(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  auto context = isolate->GetCurrentContext();

  v8::Local<v8::Object> descriptor = v8::Object::New(isolate);
  EXPECT_TRUE(
      v8CallBoolean(descriptor->Set(context, v8String(isolate, "configurable"),
                                    v8::Boolean::New(isolate, true))));
  EXPECT_TRUE(v8CallBoolean(descriptor->SetAccessor(
      context, v8String(isolate, "enumerable"), ThrowException)));
  info.GetReturnValue().Set(descriptor);
}

TEST(NativeValueTraitsImplTest, IDLRecord) {
  V8TestingScope scope;
  {
    v8::Local<v8::Object> v8Object = v8::Object::New(scope.isolate());
    NonThrowableExceptionState exceptionState;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLString, IDLOctet>>::nativeValue(
            scope.isolate(), v8Object, exceptionState);
    EXPECT_TRUE(record.isEmpty());
  }
  {
    v8::Local<v8::Object> v8Object = v8::Object::New(scope.isolate());
    EXPECT_TRUE(v8CallBoolean(
        v8Object->Set(scope.context(), ToV8(&scope, "foo"), ToV8(&scope, 34))));
    EXPECT_TRUE(v8CallBoolean(v8Object->Set(
        scope.context(), ToV8(&scope, "bar"), ToV8(&scope, -1024))));
    EXPECT_TRUE(v8CallBoolean(
        v8Object->Set(scope.context(), ToV8(&scope, "foo"), ToV8(&scope, 42))));

    NonThrowableExceptionState exceptionState;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLByteString, IDLLong>>::nativeValue(
            scope.isolate(), v8Object, exceptionState);
    EXPECT_EQ(2U, record.size());
    EXPECT_EQ(std::make_pair(String("foo"), int32_t(42)), record[0]);
    EXPECT_EQ(std::make_pair(String("bar"), int32_t(-1024)), record[1]);
  }
  {
    v8::Local<v8::Object> v8Object = v8::Object::New(scope.isolate());
    EXPECT_TRUE(v8CallBoolean(
        v8Object->Set(scope.context(), ToV8(&scope, "foo"), ToV8(&scope, 34))));
    EXPECT_TRUE(v8CallBoolean(v8Object->DefineOwnProperty(
        scope.context(), v8String(scope.isolate(), "bar"), ToV8(&scope, -1024),
        v8::PropertyAttribute::DontEnum)));
    EXPECT_TRUE(v8CallBoolean(
        v8Object->Set(scope.context(), ToV8(&scope, "baz"), ToV8(&scope, 42))));

    NonThrowableExceptionState exceptionState;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLByteString, IDLLong>>::nativeValue(
            scope.isolate(), v8Object, exceptionState);
    EXPECT_EQ(2U, record.size());
    EXPECT_EQ(std::make_pair(String("foo"), int32_t(34)), record[0]);
    EXPECT_EQ(std::make_pair(String("baz"), int32_t(42)), record[1]);
  }
  {
    // Exceptions are being thrown in this test, so we need another scope.
    V8TestingScope scope;
    v8::Local<v8::Object> originalObject = v8::Object::New(scope.isolate());
    EXPECT_TRUE(v8CallBoolean(originalObject->Set(
        scope.context(), ToV8(&scope, "foo"), ToV8(&scope, 34))));
    EXPECT_TRUE(v8CallBoolean(originalObject->Set(
        scope.context(), ToV8(&scope, "bar"), ToV8(&scope, 42))));

    NonThrowableExceptionState exceptionState;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLString, IDLLong>>::nativeValue(
            scope.isolate(), originalObject, exceptionState);
    EXPECT_EQ(2U, record.size());

    v8::Local<v8::Object> handler = v8::Object::New(scope.isolate());
    EXPECT_TRUE(v8CallBoolean(handler->Set(
        scope.context(), ToV8(&scope, "getOwnPropertyDescriptor"),
        v8::Function::New(scope.context(), ReturnBogusObjectDescriptor)
            .ToLocalChecked())));
    v8::Local<v8::Proxy> proxy =
        v8::Proxy::New(scope.context(), originalObject, handler)
            .ToLocalChecked();
    ExceptionState exceptionStateFromProxy(
        scope.isolate(), ExceptionState::ExecutionContext,
        "NativeValueTraitsImplTest", "IDLRecordTest");
    const auto& recordFromProxy =
        NativeValueTraits<IDLRecord<IDLString, IDLLong>>::nativeValue(
            scope.isolate(), proxy, exceptionStateFromProxy);
    EXPECT_EQ(0U, recordFromProxy.size());
    EXPECT_TRUE(exceptionStateFromProxy.hadException());
    EXPECT_TRUE(exceptionStateFromProxy.message().isEmpty());
    v8::Local<v8::Value> v8Exception = exceptionStateFromProxy.getException();
    EXPECT_TRUE(v8Exception->IsString());
    EXPECT_TRUE(
        v8String(scope.isolate(), "bogus!")
            ->Equals(v8Exception->ToString(scope.context()).ToLocalChecked()));
  }
  {
    v8::Local<v8::Object> v8Object = v8::Object::New(scope.isolate());
    EXPECT_TRUE(v8CallBoolean(
        v8Object->Set(scope.context(), ToV8(&scope, "foo"), ToV8(&scope, 42))));
    EXPECT_TRUE(v8CallBoolean(
        v8Object->Set(scope.context(), ToV8(&scope, "bar"), ToV8(&scope, 0))));
    EXPECT_TRUE(v8CallBoolean(v8Object->Set(scope.context(), ToV8(&scope, "xx"),
                                            ToV8(&scope, true))));
    EXPECT_TRUE(v8CallBoolean(v8Object->Set(
        scope.context(), ToV8(&scope, "abcd"), ToV8(&scope, false))));

    NonThrowableExceptionState exceptionState;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLByteString, IDLBoolean>>::nativeValue(
            scope.isolate(), v8Object, exceptionState);
    EXPECT_EQ(4U, record.size());
    EXPECT_EQ(std::make_pair(String("foo"), true), record[0]);
    EXPECT_EQ(std::make_pair(String("bar"), false), record[1]);
    EXPECT_EQ(std::make_pair(String("xx"), true), record[2]);
    EXPECT_EQ(std::make_pair(String("abcd"), false), record[3]);
  }
  {
    v8::Local<v8::Array> v8StringArray = v8::Array::New(scope.isolate(), 2);
    EXPECT_TRUE(v8CallBoolean(v8StringArray->Set(
        scope.context(), ToV8(&scope, 0), ToV8(&scope, "Hello, World!"))));
    EXPECT_TRUE(v8CallBoolean(v8StringArray->Set(
        scope.context(), ToV8(&scope, 1), ToV8(&scope, "Hi, Mom!"))));
    EXPECT_TRUE(v8CallBoolean(v8StringArray->Set(
        scope.context(), ToV8(&scope, "foo"), ToV8(&scope, "Ohai"))));

    NonThrowableExceptionState exceptionState;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLUSVString, IDLString>>::nativeValue(
            scope.isolate(), v8StringArray, exceptionState);
    EXPECT_EQ(3U, record.size());
    EXPECT_EQ(std::make_pair(String("0"), String("Hello, World!")), record[0]);
    EXPECT_EQ(std::make_pair(String("1"), String("Hi, Mom!")), record[1]);
    EXPECT_EQ(std::make_pair(String("foo"), String("Ohai")), record[2]);
  }
  {
    v8::Local<v8::Object> v8Object = v8::Object::New(scope.isolate());
    EXPECT_TRUE(v8CallBoolean(v8Object->Set(
        scope.context(), v8::Symbol::GetToStringTag(scope.isolate()),
        ToV8(&scope, 34))));
    EXPECT_TRUE(v8CallBoolean(
        v8Object->Set(scope.context(), ToV8(&scope, "foo"), ToV8(&scope, 42))));

    NonThrowableExceptionState exceptionState;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLString, IDLShort>>::nativeValue(
            scope.isolate(), v8Object, exceptionState);
    EXPECT_EQ(1U, record.size());
    EXPECT_EQ(std::make_pair(String("foo"), int16_t(42)), record[0]);
  }
  {
    v8::Local<v8::Object> v8Object = v8::Object::New(scope.isolate());
    EXPECT_TRUE(v8CallBoolean(v8Object->Set(
        scope.context(), v8::Symbol::GetToStringTag(scope.isolate()),
        ToV8(&scope, 34))));
    EXPECT_TRUE(v8CallBoolean(
        v8Object->Set(scope.context(), ToV8(&scope, "foo"), ToV8(&scope, 42))));

    NonThrowableExceptionState exceptionState;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLString, IDLShort>>::nativeValue(
            scope.isolate(), v8Object, exceptionState);
    EXPECT_EQ(1U, record.size());
    EXPECT_EQ(std::make_pair(String("foo"), int16_t(42)), record[0]);
  }
  {
    v8::Local<v8::Object> v8ParentObject = v8::Object::New(scope.isolate());
    EXPECT_TRUE(v8CallBoolean(v8ParentObject->Set(
        scope.context(), ToV8(&scope, "foo"), ToV8(&scope, 34))));
    EXPECT_TRUE(v8CallBoolean(v8ParentObject->Set(
        scope.context(), ToV8(&scope, "bar"), ToV8(&scope, 512))));
    v8::Local<v8::Object> v8Object = v8::Object::New(scope.isolate());
    EXPECT_TRUE(
        v8CallBoolean(v8Object->SetPrototype(scope.context(), v8ParentObject)));

    NonThrowableExceptionState exceptionState;
    auto record =
        NativeValueTraits<IDLRecord<IDLString, IDLUnsignedLong>>::nativeValue(
            scope.isolate(), v8Object, exceptionState);
    EXPECT_TRUE(record.isEmpty());

    EXPECT_TRUE(v8CallBoolean(v8Object->Set(
        scope.context(), ToV8(&scope, "quux"), ToV8(&scope, 42))));
    EXPECT_TRUE(v8CallBoolean(v8Object->Set(
        scope.context(), ToV8(&scope, "foo"), ToV8(&scope, 1024))));
    record =
        NativeValueTraits<IDLRecord<IDLString, IDLUnsignedLong>>::nativeValue(
            scope.isolate(), v8Object, exceptionState);
    EXPECT_EQ(2U, record.size());
    EXPECT_EQ(std::make_pair(String("quux"), uint32_t(42)), record[0]);
    EXPECT_EQ(std::make_pair(String("foo"), uint32_t(1024)), record[1]);
  }
  {
    v8::Local<v8::Array> v8StringArray = v8::Array::New(scope.isolate(), 2);
    EXPECT_TRUE(v8CallBoolean(v8StringArray->Set(
        scope.context(), ToV8(&scope, 0), ToV8(&scope, "Hello, World!"))));
    EXPECT_TRUE(v8CallBoolean(v8StringArray->Set(
        scope.context(), ToV8(&scope, 1), ToV8(&scope, "Hi, Mom!"))));
    v8::Local<v8::Object> v8Object = v8::Object::New(scope.isolate());
    EXPECT_TRUE(v8CallBoolean(v8Object->Set(
        scope.context(), ToV8(&scope, "foo"), ToV8(&scope, v8StringArray))));

    NonThrowableExceptionState exceptionState;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLString, IDLSequence<IDLString>>>::
            nativeValue(scope.isolate(), v8Object, exceptionState);
    EXPECT_EQ(1U, record.size());
    EXPECT_EQ("foo", record[0].first);
    EXPECT_EQ("Hello, World!", record[0].second[0]);
    EXPECT_EQ("Hi, Mom!", record[0].second[1]);
  }
}

}  // namespace

}  // namespace blink
