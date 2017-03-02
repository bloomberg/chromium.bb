// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ToV8.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/testing/GarbageCollectedScriptWrappable.h"
#include "platform/heap/Heap.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"
#include <limits>

#define TEST_TOV8(expected, value) \
  testToV8(&scope, expected, value, __FILE__, __LINE__)

namespace blink {

namespace {

template <typename T>
void testToV8(V8TestingScope* scope,
              const char* expected,
              T value,
              const char* path,
              int lineNumber) {
  v8::Local<v8::Value> actual =
      ToV8(value, scope->context()->Global(), scope->isolate());
  if (actual.IsEmpty()) {
    ADD_FAILURE_AT(path, lineNumber) << "toV8 returns an empty value.";
    return;
  }
  String actualString =
      toCoreString(actual->ToString(scope->context()).ToLocalChecked());
  if (String(expected) != actualString) {
    ADD_FAILURE_AT(path, lineNumber)
        << "toV8 returns an incorrect value.\n  Actual: "
        << actualString.utf8().data() << "\nExpected: " << expected;
    return;
  }
}

class GarbageCollectedHolder : public GarbageCollected<GarbageCollectedHolder> {
 public:
  GarbageCollectedHolder(GarbageCollectedScriptWrappable* scriptWrappable)
      : m_scriptWrappable(scriptWrappable) {}

  DEFINE_INLINE_TRACE() { visitor->trace(m_scriptWrappable); }

  // This should be public in order to access a Member<X> object.
  Member<GarbageCollectedScriptWrappable> m_scriptWrappable;
};

class OffHeapGarbageCollectedHolder {
 public:
  OffHeapGarbageCollectedHolder(
      GarbageCollectedScriptWrappable* scriptWrappable)
      : m_scriptWrappable(scriptWrappable) {}

  // This should be public in order to access a Persistent<X> object.
  Persistent<GarbageCollectedScriptWrappable> m_scriptWrappable;
};

TEST(ToV8Test, garbageCollectedScriptWrappable) {
  V8TestingScope scope;
  GarbageCollectedScriptWrappable* object =
      new GarbageCollectedScriptWrappable("world");
  GarbageCollectedHolder holder(object);
  OffHeapGarbageCollectedHolder offHeapHolder(object);

  TEST_TOV8("world", object);
  TEST_TOV8("world", holder.m_scriptWrappable);
  TEST_TOV8("world", offHeapHolder.m_scriptWrappable);

  object = nullptr;
  holder.m_scriptWrappable = nullptr;
  offHeapHolder.m_scriptWrappable = nullptr;

  TEST_TOV8("null", object);
  TEST_TOV8("null", holder.m_scriptWrappable);
  TEST_TOV8("null", offHeapHolder.m_scriptWrappable);
}

TEST(ToV8Test, string) {
  V8TestingScope scope;
  char arrayString[] = "arrayString";
  const char constArrayString[] = "constArrayString";
  TEST_TOV8("arrayString", arrayString);
  TEST_TOV8("constArrayString", constArrayString);
  TEST_TOV8("pointer", const_cast<char*>("pointer"));
  TEST_TOV8("constPointer", static_cast<const char*>("constPointer"));
  TEST_TOV8("coreString", String("coreString"));
  TEST_TOV8("atomicString", AtomicString("atomicString"));

  // Null strings are converted to empty strings.
  TEST_TOV8("", static_cast<char*>(nullptr));
  TEST_TOV8("", static_cast<const char*>(nullptr));
  TEST_TOV8("", String());
  TEST_TOV8("", AtomicString());
}

TEST(ToV8Test, numeric) {
  V8TestingScope scope;
  TEST_TOV8("0", static_cast<int>(0));
  TEST_TOV8("1", static_cast<int>(1));
  TEST_TOV8("-1", static_cast<int>(-1));

  TEST_TOV8("-2", static_cast<long>(-2));
  TEST_TOV8("2", static_cast<unsigned>(2));
  TEST_TOV8("2", static_cast<unsigned long>(2));

  TEST_TOV8("-2147483648", std::numeric_limits<int32_t>::min());
  TEST_TOV8("2147483647", std::numeric_limits<int32_t>::max());
  TEST_TOV8("4294967295", std::numeric_limits<uint32_t>::max());
  // v8::Number can represent exact numbers in [-(2^53-1), 2^53-1].
  TEST_TOV8("-9007199254740991", -9007199254740991);  // -(2^53-1)
  TEST_TOV8("9007199254740991", 9007199254740991);    // 2^53-1

  TEST_TOV8("0.5", static_cast<double>(0.5));
  TEST_TOV8("-0.5", static_cast<float>(-0.5));
  TEST_TOV8("NaN", std::numeric_limits<double>::quiet_NaN());
  TEST_TOV8("Infinity", std::numeric_limits<double>::infinity());
  TEST_TOV8("-Infinity", -std::numeric_limits<double>::infinity());
}

TEST(ToV8Test, boolean) {
  V8TestingScope scope;
  TEST_TOV8("true", true);
  TEST_TOV8("false", false);
}

TEST(ToV8Test, v8Value) {
  V8TestingScope scope;
  v8::Local<v8::Value> localValue(v8::Number::New(scope.isolate(), 1234));
  v8::Local<v8::Value> handleValue(v8::Number::New(scope.isolate(), 5678));

  TEST_TOV8("1234", localValue);
  TEST_TOV8("5678", handleValue);
}

TEST(ToV8Test, undefinedType) {
  V8TestingScope scope;
  TEST_TOV8("undefined", ToV8UndefinedGenerator());
}

TEST(ToV8Test, scriptValue) {
  V8TestingScope scope;
  ScriptValue value(scope.getScriptState(),
                    v8::Number::New(scope.isolate(), 1234));

  TEST_TOV8("1234", value);
}

TEST(ToV8Test, stringVectors) {
  V8TestingScope scope;
  Vector<String> stringVector;
  stringVector.push_back("foo");
  stringVector.push_back("bar");
  TEST_TOV8("foo,bar", stringVector);

  Vector<AtomicString> atomicStringVector;
  atomicStringVector.push_back("quux");
  atomicStringVector.push_back("bar");
  TEST_TOV8("quux,bar", atomicStringVector);
}

TEST(ToV8Test, basicTypeVectors) {
  V8TestingScope scope;
  Vector<int> intVector;
  intVector.push_back(42);
  intVector.push_back(23);
  TEST_TOV8("42,23", intVector);

  Vector<long> longVector;
  longVector.push_back(31773);
  longVector.push_back(404);
  TEST_TOV8("31773,404", longVector);

  Vector<unsigned> unsignedVector;
  unsignedVector.push_back(1);
  unsignedVector.push_back(2);
  TEST_TOV8("1,2", unsignedVector);

  Vector<unsigned long> unsignedLongVector;
  unsignedLongVector.push_back(1001);
  unsignedLongVector.push_back(2002);
  TEST_TOV8("1001,2002", unsignedLongVector);

  Vector<float> floatVector;
  floatVector.push_back(0.125);
  floatVector.push_back(1.);
  TEST_TOV8("0.125,1", floatVector);

  Vector<double> doubleVector;
  doubleVector.push_back(2.3);
  doubleVector.push_back(4.2);
  TEST_TOV8("2.3,4.2", doubleVector);

  Vector<bool> boolVector;
  boolVector.push_back(true);
  boolVector.push_back(true);
  boolVector.push_back(false);
  TEST_TOV8("true,true,false", boolVector);
}

TEST(ToV8Test, pairVector) {
  V8TestingScope scope;
  Vector<std::pair<String, int>> pairVector;
  pairVector.push_back(std::make_pair("one", 1));
  pairVector.push_back(std::make_pair("two", 2));
  TEST_TOV8("[object Object]", pairVector);
  v8::Local<v8::Context> context = scope.getScriptState()->context();
  v8::Local<v8::Object> result =
      ToV8(pairVector, context->Global(), scope.isolate())
          ->ToObject(context)
          .ToLocalChecked();
  v8::Local<v8::Value> one =
      result->Get(context, v8String(scope.isolate(), "one")).ToLocalChecked();
  EXPECT_EQ(1, one->NumberValue(context).FromJust());
  v8::Local<v8::Value> two =
      result->Get(context, v8String(scope.isolate(), "two")).ToLocalChecked();
  EXPECT_EQ(2, two->NumberValue(context).FromJust());
}

TEST(ToV8Test, pairHeapVector) {
  V8TestingScope scope;
  HeapVector<std::pair<String, Member<GarbageCollectedScriptWrappable>>>
      pairHeapVector;
  pairHeapVector.push_back(
      std::make_pair("one", new GarbageCollectedScriptWrappable("foo")));
  pairHeapVector.push_back(
      std::make_pair("two", new GarbageCollectedScriptWrappable("bar")));
  TEST_TOV8("[object Object]", pairHeapVector);
  v8::Local<v8::Context> context = scope.getScriptState()->context();
  v8::Local<v8::Object> result =
      ToV8(pairHeapVector, context->Global(), scope.isolate())
          ->ToObject(context)
          .ToLocalChecked();
  v8::Local<v8::Value> one =
      result->Get(context, v8String(scope.isolate(), "one")).ToLocalChecked();
  EXPECT_TRUE(one->IsObject());
  EXPECT_EQ(String("foo"), toCoreString(one->ToString()));
  v8::Local<v8::Value> two =
      result->Get(context, v8String(scope.isolate(), "two")).ToLocalChecked();
  EXPECT_TRUE(two->IsObject());
  EXPECT_EQ(String("bar"), toCoreString(two->ToString()));
}

TEST(ToV8Test, stringVectorVector) {
  V8TestingScope scope;

  Vector<String> stringVector1;
  stringVector1.push_back("foo");
  stringVector1.push_back("bar");
  Vector<String> stringVector2;
  stringVector2.push_back("quux");

  Vector<Vector<String>> compoundVector;
  compoundVector.push_back(stringVector1);
  compoundVector.push_back(stringVector2);

  EXPECT_EQ(2U, compoundVector.size());
  TEST_TOV8("foo,bar,quux", compoundVector);

  v8::Local<v8::Context> context = scope.getScriptState()->context();
  v8::Local<v8::Object> result =
      ToV8(compoundVector, context->Global(), scope.isolate())
          ->ToObject(context)
          .ToLocalChecked();
  v8::Local<v8::Value> vector1 = result->Get(context, 0).ToLocalChecked();
  EXPECT_TRUE(vector1->IsArray());
  EXPECT_EQ(2U, vector1.As<v8::Array>()->Length());
  v8::Local<v8::Value> vector2 = result->Get(context, 1).ToLocalChecked();
  EXPECT_TRUE(vector2->IsArray());
  EXPECT_EQ(1U, vector2.As<v8::Array>()->Length());
}

TEST(ToV8Test, heapVector) {
  V8TestingScope scope;
  HeapVector<Member<GarbageCollectedScriptWrappable>> v;
  v.push_back(new GarbageCollectedScriptWrappable("hoge"));
  v.push_back(new GarbageCollectedScriptWrappable("fuga"));
  v.push_back(nullptr);

  TEST_TOV8("hoge,fuga,", v);
}

TEST(ToV8Test, withScriptState) {
  V8TestingScope scope;
  ScriptValue value(scope.getScriptState(),
                    v8::Number::New(scope.isolate(), 1234.0));

  v8::Local<v8::Value> actual = ToV8(value, scope.getScriptState());
  EXPECT_FALSE(actual.IsEmpty());

  double actualAsNumber = actual.As<v8::Number>()->Value();
  EXPECT_EQ(1234.0, actualAsNumber);
}

}  // namespace

}  // namespace blink
