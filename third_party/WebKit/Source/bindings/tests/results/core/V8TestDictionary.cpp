// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/dictionary_v8.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "V8TestDictionary.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8Element.h"
#include "bindings/core/v8/V8EventTarget.h"
#include "bindings/core/v8/V8InternalDictionary.h"
#include "bindings/core/v8/V8TestInterface.h"
#include "bindings/core/v8/V8TestInterface2.h"
#include "bindings/core/v8/V8TestInterfaceGarbageCollected.h"
#include "bindings/core/v8/V8TestObject.h"
#include "bindings/core/v8/V8Uint8Array.h"
#include "core/frame/Deprecation.h"
#include "core/origin_trials/origin_trials.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/FlexibleArrayBufferView.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

static const v8::Eternal<v8::Name>* eternalV8TestDictionaryKeys(v8::Isolate* isolate) {
  static const char* const kKeys[] = {
    "anyInRecordMember",
    "anyMember",
    "applicableToTypeLongMember",
    "applicableToTypeStringMember",
    "booleanMember",
    "create",
    "deprecatedCreateMember",
    "dictionaryMember",
    "doubleOrNullMember",
    "doubleOrStringMember",
    "doubleOrStringSequenceMember",
    "elementOrNullMember",
    "enumMember",
    "enumSequenceMember",
    "eventTargetMember",
    "garbageCollectedRecordMember",
    "internalDictionarySequenceMember",
    "longMember",
    "objectMember",
    "objectOrNullMember",
    "originTrialMember",
    "originTrialSecondMember",
    "otherDoubleOrStringMember",
    "public",
    "recordMember",
    "restrictedDoubleMember",
    "runtimeMember",
    "runtimeSecondMember",
    "stringMember",
    "stringOrNullMember",
    "stringSequenceMember",
    "testInterface2OrUint8ArrayMember",
    "testInterfaceGarbageCollectedMember",
    "testInterfaceGarbageCollectedOrNullMember",
    "testInterfaceGarbageCollectedSequenceMember",
    "testInterfaceMember",
    "testInterfaceOrNullMember",
    "testInterfaceSequenceMember",
    "testObjectSequenceMember",
    "treatNullAsStringSequenceMember",
    "uint8ArrayMember",
    "unionInRecordMember",
    "unionWithTypedefs",
    "unrestrictedDoubleMember",
  };
  return V8PerIsolateData::From(isolate)->FindOrCreateEternalNameCache(
      kKeys, kKeys, WTF_ARRAY_LENGTH(kKeys));
}

void V8TestDictionary::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, TestDictionary& impl, ExceptionState& exceptionState) {
  if (IsUndefinedOrNull(v8Value)) {
    return;
  }
  if (!v8Value->IsObject()) {
    exceptionState.ThrowTypeError("cannot convert to dictionary.");
    return;
  }
  v8::Local<v8::Object> v8Object = v8Value.As<v8::Object>();
  ALLOW_UNUSED_LOCAL(v8Object);

  const v8::Eternal<v8::Name>* keys = eternalV8TestDictionaryKeys(isolate);
  v8::TryCatch block(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  ExecutionContext* executionContext = ToExecutionContext(context);
  DCHECK(executionContext);
  v8::Local<v8::Value> anyInRecordMemberValue;
  if (!v8Object->Get(context, keys[0].Get(isolate)).ToLocal(&anyInRecordMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (anyInRecordMemberValue.IsEmpty() || anyInRecordMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<std::pair<String, ScriptValue>> anyInRecordMemberCppValue = NativeValueTraits<IDLRecord<IDLString, ScriptValue>>::NativeValue(isolate, anyInRecordMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setAnyInRecordMember(anyInRecordMemberCppValue);
  }

  v8::Local<v8::Value> anyMemberValue;
  if (!v8Object->Get(context, keys[1].Get(isolate)).ToLocal(&anyMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (anyMemberValue.IsEmpty() || anyMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    ScriptValue anyMemberCppValue = ScriptValue(ScriptState::Current(isolate), anyMemberValue);
    impl.setAnyMember(anyMemberCppValue);
  }

  v8::Local<v8::Value> applicableToTypeLongMemberValue;
  if (!v8Object->Get(context, keys[2].Get(isolate)).ToLocal(&applicableToTypeLongMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (applicableToTypeLongMemberValue.IsEmpty() || applicableToTypeLongMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    int32_t applicableToTypeLongMemberCppValue = NativeValueTraits<IDLLong>::NativeValue(isolate, applicableToTypeLongMemberValue, exceptionState, kClamp);
    if (exceptionState.HadException())
      return;
    impl.setApplicableToTypeLongMember(applicableToTypeLongMemberCppValue);
  }

  v8::Local<v8::Value> applicableToTypeStringMemberValue;
  if (!v8Object->Get(context, keys[3].Get(isolate)).ToLocal(&applicableToTypeStringMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (applicableToTypeStringMemberValue.IsEmpty() || applicableToTypeStringMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<kTreatNullAsEmptyString> applicableToTypeStringMemberCppValue = applicableToTypeStringMemberValue;
    if (!applicableToTypeStringMemberCppValue.Prepare(exceptionState))
      return;
    impl.setApplicableToTypeStringMember(applicableToTypeStringMemberCppValue);
  }

  v8::Local<v8::Value> booleanMemberValue;
  if (!v8Object->Get(context, keys[4].Get(isolate)).ToLocal(&booleanMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (booleanMemberValue.IsEmpty() || booleanMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    bool booleanMemberCppValue = NativeValueTraits<IDLBoolean>::NativeValue(isolate, booleanMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setBooleanMember(booleanMemberCppValue);
  }

  v8::Local<v8::Value> createValue;
  if (!v8Object->Get(context, keys[5].Get(isolate)).ToLocal(&createValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (createValue.IsEmpty() || createValue->IsUndefined()) {
    // Do nothing.
  } else {
    bool createCppValue = NativeValueTraits<IDLBoolean>::NativeValue(isolate, createValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setCreateMember(createCppValue);
  }

  v8::Local<v8::Value> deprecatedCreateMemberValue;
  if (!v8Object->Get(context, keys[6].Get(isolate)).ToLocal(&deprecatedCreateMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (deprecatedCreateMemberValue.IsEmpty() || deprecatedCreateMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    Deprecation::CountDeprecation(CurrentExecutionContext(isolate), WebFeature::kCreateMember);
    bool deprecatedCreateMemberCppValue = NativeValueTraits<IDLBoolean>::NativeValue(isolate, deprecatedCreateMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setCreateMember(deprecatedCreateMemberCppValue);
  }

  v8::Local<v8::Value> dictionaryMemberValue;
  if (!v8Object->Get(context, keys[7].Get(isolate)).ToLocal(&dictionaryMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (dictionaryMemberValue.IsEmpty() || dictionaryMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    Dictionary dictionaryMemberCppValue = NativeValueTraits<Dictionary>::NativeValue(isolate, dictionaryMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    if (!dictionaryMemberCppValue.IsObject()) {
      exceptionState.ThrowTypeError("member dictionaryMember is not an object.");
      return;
    }
    impl.setDictionaryMember(dictionaryMemberCppValue);
  }

  v8::Local<v8::Value> doubleOrNullMemberValue;
  if (!v8Object->Get(context, keys[8].Get(isolate)).ToLocal(&doubleOrNullMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (doubleOrNullMemberValue.IsEmpty() || doubleOrNullMemberValue->IsUndefined()) {
    // Do nothing.
  } else if (doubleOrNullMemberValue->IsNull()) {
    impl.setDoubleOrNullMemberToNull();
  } else {
    double doubleOrNullMemberCppValue = NativeValueTraits<IDLDouble>::NativeValue(isolate, doubleOrNullMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setDoubleOrNullMember(doubleOrNullMemberCppValue);
  }

  v8::Local<v8::Value> doubleOrStringMemberValue;
  if (!v8Object->Get(context, keys[9].Get(isolate)).ToLocal(&doubleOrStringMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (doubleOrStringMemberValue.IsEmpty() || doubleOrStringMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    DoubleOrString doubleOrStringMemberCppValue;
    V8DoubleOrString::ToImpl(isolate, doubleOrStringMemberValue, doubleOrStringMemberCppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setDoubleOrStringMember(doubleOrStringMemberCppValue);
  }

  v8::Local<v8::Value> doubleOrStringSequenceMemberValue;
  if (!v8Object->Get(context, keys[10].Get(isolate)).ToLocal(&doubleOrStringSequenceMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (doubleOrStringSequenceMemberValue.IsEmpty() || doubleOrStringSequenceMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<DoubleOrString> doubleOrStringSequenceMemberCppValue = NativeValueTraits<IDLSequence<DoubleOrString>>::NativeValue(isolate, doubleOrStringSequenceMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setDoubleOrStringSequenceMember(doubleOrStringSequenceMemberCppValue);
  }

  v8::Local<v8::Value> elementOrNullMemberValue;
  if (!v8Object->Get(context, keys[11].Get(isolate)).ToLocal(&elementOrNullMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (elementOrNullMemberValue.IsEmpty() || elementOrNullMemberValue->IsUndefined()) {
    // Do nothing.
  } else if (elementOrNullMemberValue->IsNull()) {
    impl.setElementOrNullMemberToNull();
  } else {
    Element* elementOrNullMemberCppValue = V8Element::ToImplWithTypeCheck(isolate, elementOrNullMemberValue);
    if (!elementOrNullMemberCppValue) {
      exceptionState.ThrowTypeError("member elementOrNullMember is not of type Element.");
      return;
    }
    impl.setElementOrNullMember(elementOrNullMemberCppValue);
  }

  v8::Local<v8::Value> enumMemberValue;
  if (!v8Object->Get(context, keys[12].Get(isolate)).ToLocal(&enumMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (enumMemberValue.IsEmpty() || enumMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<> enumMemberCppValue = enumMemberValue;
    if (!enumMemberCppValue.Prepare(exceptionState))
      return;
    const char* validValues[] = {
        "",
        "EnumValue1",
        "EnumValue2",
        "EnumValue3",
    };
    if (!IsValidEnum(enumMemberCppValue, validValues, WTF_ARRAY_LENGTH(validValues), "TestEnum", exceptionState))
      return;
    impl.setEnumMember(enumMemberCppValue);
  }

  v8::Local<v8::Value> enumSequenceMemberValue;
  if (!v8Object->Get(context, keys[13].Get(isolate)).ToLocal(&enumSequenceMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (enumSequenceMemberValue.IsEmpty() || enumSequenceMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<String> enumSequenceMemberCppValue = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(isolate, enumSequenceMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    const char* validValues[] = {
        "",
        "EnumValue1",
        "EnumValue2",
        "EnumValue3",
    };
    if (!IsValidEnum(enumSequenceMemberCppValue, validValues, WTF_ARRAY_LENGTH(validValues), "TestEnum", exceptionState))
      return;
    impl.setEnumSequenceMember(enumSequenceMemberCppValue);
  }

  v8::Local<v8::Value> eventTargetMemberValue;
  if (!v8Object->Get(context, keys[14].Get(isolate)).ToLocal(&eventTargetMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (eventTargetMemberValue.IsEmpty() || eventTargetMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    EventTarget* eventTargetMemberCppValue = V8EventTarget::ToImplWithTypeCheck(isolate, eventTargetMemberValue);
    if (!eventTargetMemberCppValue) {
      exceptionState.ThrowTypeError("member eventTargetMember is not of type EventTarget.");
      return;
    }
    impl.setEventTargetMember(eventTargetMemberCppValue);
  }

  v8::Local<v8::Value> garbageCollectedRecordMemberValue;
  if (!v8Object->Get(context, keys[15].Get(isolate)).ToLocal(&garbageCollectedRecordMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (garbageCollectedRecordMemberValue.IsEmpty() || garbageCollectedRecordMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<std::pair<String, Member<TestObject>>> garbageCollectedRecordMemberCppValue = NativeValueTraits<IDLRecord<IDLUSVString, TestObject>>::NativeValue(isolate, garbageCollectedRecordMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setGarbageCollectedRecordMember(garbageCollectedRecordMemberCppValue);
  }

  v8::Local<v8::Value> internalDictionarySequenceMemberValue;
  if (!v8Object->Get(context, keys[16].Get(isolate)).ToLocal(&internalDictionarySequenceMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (internalDictionarySequenceMemberValue.IsEmpty() || internalDictionarySequenceMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<InternalDictionary> internalDictionarySequenceMemberCppValue = NativeValueTraits<IDLSequence<InternalDictionary>>::NativeValue(isolate, internalDictionarySequenceMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setInternalDictionarySequenceMember(internalDictionarySequenceMemberCppValue);
  }

  v8::Local<v8::Value> longMemberValue;
  if (!v8Object->Get(context, keys[17].Get(isolate)).ToLocal(&longMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (longMemberValue.IsEmpty() || longMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    int32_t longMemberCppValue = NativeValueTraits<IDLLong>::NativeValue(isolate, longMemberValue, exceptionState, kNormalConversion);
    if (exceptionState.HadException())
      return;
    impl.setLongMember(longMemberCppValue);
  }

  v8::Local<v8::Value> objectMemberValue;
  if (!v8Object->Get(context, keys[18].Get(isolate)).ToLocal(&objectMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (objectMemberValue.IsEmpty() || objectMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    ScriptValue objectMemberCppValue = ScriptValue(ScriptState::Current(isolate), objectMemberValue);
    if (!objectMemberCppValue.IsObject()) {
      exceptionState.ThrowTypeError("member objectMember is not an object.");
      return;
    }
    impl.setObjectMember(objectMemberCppValue);
  }

  v8::Local<v8::Value> objectOrNullMemberValue;
  if (!v8Object->Get(context, keys[19].Get(isolate)).ToLocal(&objectOrNullMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (objectOrNullMemberValue.IsEmpty() || objectOrNullMemberValue->IsUndefined()) {
    // Do nothing.
  } else if (objectOrNullMemberValue->IsNull()) {
    impl.setObjectOrNullMemberToNull();
  } else {
    ScriptValue objectOrNullMemberCppValue = ScriptValue(ScriptState::Current(isolate), objectOrNullMemberValue);
    if (!objectOrNullMemberCppValue.IsObject()) {
      exceptionState.ThrowTypeError("member objectOrNullMember is not an object.");
      return;
    }
    impl.setObjectOrNullMember(objectOrNullMemberCppValue);
  }

  v8::Local<v8::Value> otherDoubleOrStringMemberValue;
  if (!v8Object->Get(context, keys[22].Get(isolate)).ToLocal(&otherDoubleOrStringMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (otherDoubleOrStringMemberValue.IsEmpty() || otherDoubleOrStringMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    DoubleOrString otherDoubleOrStringMemberCppValue;
    V8DoubleOrString::ToImpl(isolate, otherDoubleOrStringMemberValue, otherDoubleOrStringMemberCppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setOtherDoubleOrStringMember(otherDoubleOrStringMemberCppValue);
  }

  v8::Local<v8::Value> publicValue;
  if (!v8Object->Get(context, keys[23].Get(isolate)).ToLocal(&publicValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (publicValue.IsEmpty() || publicValue->IsUndefined()) {
    // Do nothing.
  } else {
    bool publicCppValue = NativeValueTraits<IDLBoolean>::NativeValue(isolate, publicValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setIsPublic(publicCppValue);
  }

  v8::Local<v8::Value> recordMemberValue;
  if (!v8Object->Get(context, keys[24].Get(isolate)).ToLocal(&recordMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (recordMemberValue.IsEmpty() || recordMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<std::pair<String, int8_t>> recordMemberCppValue = NativeValueTraits<IDLRecord<IDLByteString, IDLByte>>::NativeValue(isolate, recordMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setRecordMember(recordMemberCppValue);
  }

  v8::Local<v8::Value> restrictedDoubleMemberValue;
  if (!v8Object->Get(context, keys[25].Get(isolate)).ToLocal(&restrictedDoubleMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (restrictedDoubleMemberValue.IsEmpty() || restrictedDoubleMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    double restrictedDoubleMemberCppValue = NativeValueTraits<IDLDouble>::NativeValue(isolate, restrictedDoubleMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setRestrictedDoubleMember(restrictedDoubleMemberCppValue);
  }

  v8::Local<v8::Value> stringMemberValue;
  if (!v8Object->Get(context, keys[28].Get(isolate)).ToLocal(&stringMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (stringMemberValue.IsEmpty() || stringMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<> stringMemberCppValue = stringMemberValue;
    if (!stringMemberCppValue.Prepare(exceptionState))
      return;
    impl.setStringMember(stringMemberCppValue);
  }

  v8::Local<v8::Value> stringOrNullMemberValue;
  if (!v8Object->Get(context, keys[29].Get(isolate)).ToLocal(&stringOrNullMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (stringOrNullMemberValue.IsEmpty() || stringOrNullMemberValue->IsUndefined()) {
    // Do nothing.
  } else if (stringOrNullMemberValue->IsNull()) {
    impl.setStringOrNullMemberToNull();
  } else {
    V8StringResource<> stringOrNullMemberCppValue = stringOrNullMemberValue;
    if (!stringOrNullMemberCppValue.Prepare(exceptionState))
      return;
    impl.setStringOrNullMember(stringOrNullMemberCppValue);
  }

  v8::Local<v8::Value> stringSequenceMemberValue;
  if (!v8Object->Get(context, keys[30].Get(isolate)).ToLocal(&stringSequenceMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (stringSequenceMemberValue.IsEmpty() || stringSequenceMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<String> stringSequenceMemberCppValue = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(isolate, stringSequenceMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setStringSequenceMember(stringSequenceMemberCppValue);
  }

  v8::Local<v8::Value> testInterface2OrUint8ArrayMemberValue;
  if (!v8Object->Get(context, keys[31].Get(isolate)).ToLocal(&testInterface2OrUint8ArrayMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (testInterface2OrUint8ArrayMemberValue.IsEmpty() || testInterface2OrUint8ArrayMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    TestInterface2OrUint8Array testInterface2OrUint8ArrayMemberCppValue;
    V8TestInterface2OrUint8Array::ToImpl(isolate, testInterface2OrUint8ArrayMemberValue, testInterface2OrUint8ArrayMemberCppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setTestInterface2OrUint8ArrayMember(testInterface2OrUint8ArrayMemberCppValue);
  }

  v8::Local<v8::Value> testInterfaceGarbageCollectedMemberValue;
  if (!v8Object->Get(context, keys[32].Get(isolate)).ToLocal(&testInterfaceGarbageCollectedMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (testInterfaceGarbageCollectedMemberValue.IsEmpty() || testInterfaceGarbageCollectedMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    TestInterfaceGarbageCollected* testInterfaceGarbageCollectedMemberCppValue = V8TestInterfaceGarbageCollected::ToImplWithTypeCheck(isolate, testInterfaceGarbageCollectedMemberValue);
    if (!testInterfaceGarbageCollectedMemberCppValue) {
      exceptionState.ThrowTypeError("member testInterfaceGarbageCollectedMember is not of type TestInterfaceGarbageCollected.");
      return;
    }
    impl.setTestInterfaceGarbageCollectedMember(testInterfaceGarbageCollectedMemberCppValue);
  }

  v8::Local<v8::Value> testInterfaceGarbageCollectedOrNullMemberValue;
  if (!v8Object->Get(context, keys[33].Get(isolate)).ToLocal(&testInterfaceGarbageCollectedOrNullMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (testInterfaceGarbageCollectedOrNullMemberValue.IsEmpty() || testInterfaceGarbageCollectedOrNullMemberValue->IsUndefined()) {
    // Do nothing.
  } else if (testInterfaceGarbageCollectedOrNullMemberValue->IsNull()) {
    impl.setTestInterfaceGarbageCollectedOrNullMemberToNull();
  } else {
    TestInterfaceGarbageCollected* testInterfaceGarbageCollectedOrNullMemberCppValue = V8TestInterfaceGarbageCollected::ToImplWithTypeCheck(isolate, testInterfaceGarbageCollectedOrNullMemberValue);
    if (!testInterfaceGarbageCollectedOrNullMemberCppValue) {
      exceptionState.ThrowTypeError("member testInterfaceGarbageCollectedOrNullMember is not of type TestInterfaceGarbageCollected.");
      return;
    }
    impl.setTestInterfaceGarbageCollectedOrNullMember(testInterfaceGarbageCollectedOrNullMemberCppValue);
  }

  v8::Local<v8::Value> testInterfaceGarbageCollectedSequenceMemberValue;
  if (!v8Object->Get(context, keys[34].Get(isolate)).ToLocal(&testInterfaceGarbageCollectedSequenceMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (testInterfaceGarbageCollectedSequenceMemberValue.IsEmpty() || testInterfaceGarbageCollectedSequenceMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<Member<TestInterfaceGarbageCollected>> testInterfaceGarbageCollectedSequenceMemberCppValue = NativeValueTraits<IDLSequence<TestInterfaceGarbageCollected>>::NativeValue(isolate, testInterfaceGarbageCollectedSequenceMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setTestInterfaceGarbageCollectedSequenceMember(testInterfaceGarbageCollectedSequenceMemberCppValue);
  }

  v8::Local<v8::Value> testInterfaceMemberValue;
  if (!v8Object->Get(context, keys[35].Get(isolate)).ToLocal(&testInterfaceMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (testInterfaceMemberValue.IsEmpty() || testInterfaceMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    TestInterfaceImplementation* testInterfaceMemberCppValue = V8TestInterface::ToImplWithTypeCheck(isolate, testInterfaceMemberValue);
    if (!testInterfaceMemberCppValue) {
      exceptionState.ThrowTypeError("member testInterfaceMember is not of type TestInterface.");
      return;
    }
    impl.setTestInterfaceMember(testInterfaceMemberCppValue);
  }

  v8::Local<v8::Value> testInterfaceOrNullMemberValue;
  if (!v8Object->Get(context, keys[36].Get(isolate)).ToLocal(&testInterfaceOrNullMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (testInterfaceOrNullMemberValue.IsEmpty() || testInterfaceOrNullMemberValue->IsUndefined()) {
    // Do nothing.
  } else if (testInterfaceOrNullMemberValue->IsNull()) {
    impl.setTestInterfaceOrNullMemberToNull();
  } else {
    TestInterfaceImplementation* testInterfaceOrNullMemberCppValue = V8TestInterface::ToImplWithTypeCheck(isolate, testInterfaceOrNullMemberValue);
    if (!testInterfaceOrNullMemberCppValue) {
      exceptionState.ThrowTypeError("member testInterfaceOrNullMember is not of type TestInterface.");
      return;
    }
    impl.setTestInterfaceOrNullMember(testInterfaceOrNullMemberCppValue);
  }

  v8::Local<v8::Value> testInterfaceSequenceMemberValue;
  if (!v8Object->Get(context, keys[37].Get(isolate)).ToLocal(&testInterfaceSequenceMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (testInterfaceSequenceMemberValue.IsEmpty() || testInterfaceSequenceMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<Member<TestInterfaceImplementation>> testInterfaceSequenceMemberCppValue = NativeValueTraits<IDLSequence<TestInterfaceImplementation>>::NativeValue(isolate, testInterfaceSequenceMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setTestInterfaceSequenceMember(testInterfaceSequenceMemberCppValue);
  }

  v8::Local<v8::Value> testObjectSequenceMemberValue;
  if (!v8Object->Get(context, keys[38].Get(isolate)).ToLocal(&testObjectSequenceMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (testObjectSequenceMemberValue.IsEmpty() || testObjectSequenceMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<Member<TestObject>> testObjectSequenceMemberCppValue = NativeValueTraits<IDLSequence<TestObject>>::NativeValue(isolate, testObjectSequenceMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setTestObjectSequenceMember(testObjectSequenceMemberCppValue);
  }

  v8::Local<v8::Value> treatNullAsStringSequenceMemberValue;
  if (!v8Object->Get(context, keys[39].Get(isolate)).ToLocal(&treatNullAsStringSequenceMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (treatNullAsStringSequenceMemberValue.IsEmpty() || treatNullAsStringSequenceMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<String> treatNullAsStringSequenceMemberCppValue = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(isolate, treatNullAsStringSequenceMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setTreatNullAsStringSequenceMember(treatNullAsStringSequenceMemberCppValue);
  }

  v8::Local<v8::Value> uint8ArrayMemberValue;
  if (!v8Object->Get(context, keys[40].Get(isolate)).ToLocal(&uint8ArrayMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (uint8ArrayMemberValue.IsEmpty() || uint8ArrayMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    NotShared<DOMUint8Array> uint8ArrayMemberCppValue = ToNotShared<NotShared<DOMUint8Array>>(isolate, uint8ArrayMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    if (!uint8ArrayMemberCppValue) {
      exceptionState.ThrowTypeError("member uint8ArrayMember is not of type Uint8Array.");
      return;
    }
    impl.setUint8ArrayMember(uint8ArrayMemberCppValue);
  }

  v8::Local<v8::Value> unionInRecordMemberValue;
  if (!v8Object->Get(context, keys[41].Get(isolate)).ToLocal(&unionInRecordMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (unionInRecordMemberValue.IsEmpty() || unionInRecordMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<std::pair<String, LongOrBoolean>> unionInRecordMemberCppValue = NativeValueTraits<IDLRecord<IDLByteString, LongOrBoolean>>::NativeValue(isolate, unionInRecordMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setUnionInRecordMember(unionInRecordMemberCppValue);
  }

  v8::Local<v8::Value> unionWithTypedefsValue;
  if (!v8Object->Get(context, keys[42].Get(isolate)).ToLocal(&unionWithTypedefsValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (unionWithTypedefsValue.IsEmpty() || unionWithTypedefsValue->IsUndefined()) {
    // Do nothing.
  } else {
    FloatOrBoolean unionWithTypedefsCppValue;
    V8FloatOrBoolean::ToImpl(isolate, unionWithTypedefsValue, unionWithTypedefsCppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setUnionWithTypedefs(unionWithTypedefsCppValue);
  }

  v8::Local<v8::Value> unrestrictedDoubleMemberValue;
  if (!v8Object->Get(context, keys[43].Get(isolate)).ToLocal(&unrestrictedDoubleMemberValue)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (unrestrictedDoubleMemberValue.IsEmpty() || unrestrictedDoubleMemberValue->IsUndefined()) {
    // Do nothing.
  } else {
    double unrestrictedDoubleMemberCppValue = NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(isolate, unrestrictedDoubleMemberValue, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setUnrestrictedDoubleMember(unrestrictedDoubleMemberCppValue);
  }

  if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
    v8::Local<v8::Value> runtimeMemberValue;
    if (!v8Object->Get(context, keys[26].Get(isolate)).ToLocal(&runtimeMemberValue)) {
      exceptionState.RethrowV8Exception(block.Exception());
      return;
    }
    if (runtimeMemberValue.IsEmpty() || runtimeMemberValue->IsUndefined()) {
      // Do nothing.
    } else {
      bool runtimeMemberCppValue = NativeValueTraits<IDLBoolean>::NativeValue(isolate, runtimeMemberValue, exceptionState);
      if (exceptionState.HadException())
        return;
      impl.setRuntimeMember(runtimeMemberCppValue);
    }

    v8::Local<v8::Value> runtimeSecondMemberValue;
    if (!v8Object->Get(context, keys[27].Get(isolate)).ToLocal(&runtimeSecondMemberValue)) {
      exceptionState.RethrowV8Exception(block.Exception());
      return;
    }
    if (runtimeSecondMemberValue.IsEmpty() || runtimeSecondMemberValue->IsUndefined()) {
      // Do nothing.
    } else {
      bool runtimeSecondMemberCppValue = NativeValueTraits<IDLBoolean>::NativeValue(isolate, runtimeSecondMemberValue, exceptionState);
      if (exceptionState.HadException())
        return;
      impl.setRuntimeSecondMember(runtimeSecondMemberCppValue);
    }
  }

  if (OriginTrials::featureName1Enabled(executionContext)) {
    v8::Local<v8::Value> originTrialSecondMemberValue;
    if (!v8Object->Get(context, keys[21].Get(isolate)).ToLocal(&originTrialSecondMemberValue)) {
      exceptionState.RethrowV8Exception(block.Exception());
      return;
    }
    if (originTrialSecondMemberValue.IsEmpty() || originTrialSecondMemberValue->IsUndefined()) {
      // Do nothing.
    } else {
      bool originTrialSecondMemberCppValue = NativeValueTraits<IDLBoolean>::NativeValue(isolate, originTrialSecondMemberValue, exceptionState);
      if (exceptionState.HadException())
        return;
      impl.setOriginTrialSecondMember(originTrialSecondMemberCppValue);
    }
  }

  if (OriginTrials::featureNameEnabled(executionContext)) {
    v8::Local<v8::Value> originTrialMemberValue;
    if (!v8Object->Get(context, keys[20].Get(isolate)).ToLocal(&originTrialMemberValue)) {
      exceptionState.RethrowV8Exception(block.Exception());
      return;
    }
    if (originTrialMemberValue.IsEmpty() || originTrialMemberValue->IsUndefined()) {
      // Do nothing.
    } else {
      bool originTrialMemberCppValue = NativeValueTraits<IDLBoolean>::NativeValue(isolate, originTrialMemberValue, exceptionState);
      if (exceptionState.HadException())
        return;
      impl.setOriginTrialMember(originTrialMemberCppValue);
    }
  }
}

v8::Local<v8::Value> TestDictionary::ToV8Impl(v8::Local<v8::Object> creationContext, v8::Isolate* isolate) const {
  v8::Local<v8::Object> v8Object = v8::Object::New(isolate);
  if (!toV8TestDictionary(*this, v8Object, creationContext, isolate))
    return v8::Undefined(isolate);
  return v8Object;
}

bool toV8TestDictionary(const TestDictionary& impl, v8::Local<v8::Object> dictionary, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  const v8::Eternal<v8::Name>* keys = eternalV8TestDictionaryKeys(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  ExecutionContext* executionContext = ToExecutionContext(context);
  DCHECK(executionContext);
  v8::Local<v8::Value> anyInRecordMemberValue;
  bool anyInRecordMemberHasValueOrDefault = false;
  if (impl.hasAnyInRecordMember()) {
    anyInRecordMemberValue = ToV8(impl.anyInRecordMember(), creationContext, isolate);
    anyInRecordMemberHasValueOrDefault = true;
  }
  if (anyInRecordMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[0].Get(isolate), anyInRecordMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> anyMemberValue;
  bool anyMemberHasValueOrDefault = false;
  if (impl.hasAnyMember()) {
    anyMemberValue = impl.anyMember().V8Value();
    anyMemberHasValueOrDefault = true;
  } else {
    anyMemberValue = v8::Null(isolate);
    anyMemberHasValueOrDefault = true;
  }
  if (anyMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[1].Get(isolate), anyMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> applicableToTypeLongMemberValue;
  bool applicableToTypeLongMemberHasValueOrDefault = false;
  if (impl.hasApplicableToTypeLongMember()) {
    applicableToTypeLongMemberValue = v8::Integer::New(isolate, impl.applicableToTypeLongMember());
    applicableToTypeLongMemberHasValueOrDefault = true;
  }
  if (applicableToTypeLongMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[2].Get(isolate), applicableToTypeLongMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> applicableToTypeStringMemberValue;
  bool applicableToTypeStringMemberHasValueOrDefault = false;
  if (impl.hasApplicableToTypeStringMember()) {
    applicableToTypeStringMemberValue = V8String(isolate, impl.applicableToTypeStringMember());
    applicableToTypeStringMemberHasValueOrDefault = true;
  }
  if (applicableToTypeStringMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[3].Get(isolate), applicableToTypeStringMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> booleanMemberValue;
  bool booleanMemberHasValueOrDefault = false;
  if (impl.hasBooleanMember()) {
    booleanMemberValue = v8::Boolean::New(isolate, impl.booleanMember());
    booleanMemberHasValueOrDefault = true;
  }
  if (booleanMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[4].Get(isolate), booleanMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> createValue;
  bool createHasValueOrDefault = false;
  if (impl.hasCreateMember()) {
    createValue = v8::Boolean::New(isolate, impl.createMember());
    createHasValueOrDefault = true;
  }
  if (createHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[5].Get(isolate), createValue))) {
    return false;
  }

  v8::Local<v8::Value> deprecatedCreateMemberValue;
  bool deprecatedCreateMemberHasValueOrDefault = false;
  if (impl.hasCreateMember()) {
    deprecatedCreateMemberValue = v8::Boolean::New(isolate, impl.createMember());
    deprecatedCreateMemberHasValueOrDefault = true;
  }
  if (deprecatedCreateMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[6].Get(isolate), deprecatedCreateMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> dictionaryMemberValue;
  bool dictionaryMemberHasValueOrDefault = false;
  if (impl.hasDictionaryMember()) {
    DCHECK(impl.dictionaryMember().IsObject());
    dictionaryMemberValue = impl.dictionaryMember().V8Value();
    dictionaryMemberHasValueOrDefault = true;
  }
  if (dictionaryMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[7].Get(isolate), dictionaryMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> doubleOrNullMemberValue;
  bool doubleOrNullMemberHasValueOrDefault = false;
  if (impl.hasDoubleOrNullMember()) {
    doubleOrNullMemberValue = v8::Number::New(isolate, impl.doubleOrNullMember());
    doubleOrNullMemberHasValueOrDefault = true;
  } else {
    doubleOrNullMemberValue = v8::Null(isolate);
    doubleOrNullMemberHasValueOrDefault = true;
  }
  if (doubleOrNullMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[8].Get(isolate), doubleOrNullMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> doubleOrStringMemberValue;
  bool doubleOrStringMemberHasValueOrDefault = false;
  if (impl.hasDoubleOrStringMember()) {
    doubleOrStringMemberValue = ToV8(impl.doubleOrStringMember(), creationContext, isolate);
    doubleOrStringMemberHasValueOrDefault = true;
  } else {
    doubleOrStringMemberValue = ToV8(DoubleOrString::FromDouble(3.14), creationContext, isolate);
    doubleOrStringMemberHasValueOrDefault = true;
  }
  if (doubleOrStringMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[9].Get(isolate), doubleOrStringMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> doubleOrStringSequenceMemberValue;
  bool doubleOrStringSequenceMemberHasValueOrDefault = false;
  if (impl.hasDoubleOrStringSequenceMember()) {
    doubleOrStringSequenceMemberValue = ToV8(impl.doubleOrStringSequenceMember(), creationContext, isolate);
    doubleOrStringSequenceMemberHasValueOrDefault = true;
  }
  if (doubleOrStringSequenceMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[10].Get(isolate), doubleOrStringSequenceMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> elementOrNullMemberValue;
  bool elementOrNullMemberHasValueOrDefault = false;
  if (impl.hasElementOrNullMember()) {
    elementOrNullMemberValue = ToV8(impl.elementOrNullMember(), creationContext, isolate);
    elementOrNullMemberHasValueOrDefault = true;
  } else {
    elementOrNullMemberValue = v8::Null(isolate);
    elementOrNullMemberHasValueOrDefault = true;
  }
  if (elementOrNullMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[11].Get(isolate), elementOrNullMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> enumMemberValue;
  bool enumMemberHasValueOrDefault = false;
  if (impl.hasEnumMember()) {
    enumMemberValue = V8String(isolate, impl.enumMember());
    enumMemberHasValueOrDefault = true;
  } else {
    enumMemberValue = V8String(isolate, "foo");
    enumMemberHasValueOrDefault = true;
  }
  if (enumMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[12].Get(isolate), enumMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> enumSequenceMemberValue;
  bool enumSequenceMemberHasValueOrDefault = false;
  if (impl.hasEnumSequenceMember()) {
    enumSequenceMemberValue = ToV8(impl.enumSequenceMember(), creationContext, isolate);
    enumSequenceMemberHasValueOrDefault = true;
  }
  if (enumSequenceMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[13].Get(isolate), enumSequenceMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> eventTargetMemberValue;
  bool eventTargetMemberHasValueOrDefault = false;
  if (impl.hasEventTargetMember()) {
    eventTargetMemberValue = ToV8(impl.eventTargetMember(), creationContext, isolate);
    eventTargetMemberHasValueOrDefault = true;
  }
  if (eventTargetMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[14].Get(isolate), eventTargetMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> garbageCollectedRecordMemberValue;
  bool garbageCollectedRecordMemberHasValueOrDefault = false;
  if (impl.hasGarbageCollectedRecordMember()) {
    garbageCollectedRecordMemberValue = ToV8(impl.garbageCollectedRecordMember(), creationContext, isolate);
    garbageCollectedRecordMemberHasValueOrDefault = true;
  }
  if (garbageCollectedRecordMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[15].Get(isolate), garbageCollectedRecordMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> internalDictionarySequenceMemberValue;
  bool internalDictionarySequenceMemberHasValueOrDefault = false;
  if (impl.hasInternalDictionarySequenceMember()) {
    internalDictionarySequenceMemberValue = ToV8(impl.internalDictionarySequenceMember(), creationContext, isolate);
    internalDictionarySequenceMemberHasValueOrDefault = true;
  }
  if (internalDictionarySequenceMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[16].Get(isolate), internalDictionarySequenceMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> longMemberValue;
  bool longMemberHasValueOrDefault = false;
  if (impl.hasLongMember()) {
    longMemberValue = v8::Integer::New(isolate, impl.longMember());
    longMemberHasValueOrDefault = true;
  } else {
    longMemberValue = v8::Integer::New(isolate, 1);
    longMemberHasValueOrDefault = true;
  }
  if (longMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[17].Get(isolate), longMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> objectMemberValue;
  bool objectMemberHasValueOrDefault = false;
  if (impl.hasObjectMember()) {
    DCHECK(impl.objectMember().IsObject());
    objectMemberValue = impl.objectMember().V8Value();
    objectMemberHasValueOrDefault = true;
  }
  if (objectMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[18].Get(isolate), objectMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> objectOrNullMemberValue;
  bool objectOrNullMemberHasValueOrDefault = false;
  if (impl.hasObjectOrNullMember()) {
    DCHECK(impl.objectOrNullMember().IsObject());
    objectOrNullMemberValue = impl.objectOrNullMember().V8Value();
    objectOrNullMemberHasValueOrDefault = true;
  } else {
    objectOrNullMemberValue = v8::Null(isolate);
    objectOrNullMemberHasValueOrDefault = true;
  }
  if (objectOrNullMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[19].Get(isolate), objectOrNullMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> otherDoubleOrStringMemberValue;
  bool otherDoubleOrStringMemberHasValueOrDefault = false;
  if (impl.hasOtherDoubleOrStringMember()) {
    otherDoubleOrStringMemberValue = ToV8(impl.otherDoubleOrStringMember(), creationContext, isolate);
    otherDoubleOrStringMemberHasValueOrDefault = true;
  } else {
    otherDoubleOrStringMemberValue = ToV8(DoubleOrString::FromString("default string value"), creationContext, isolate);
    otherDoubleOrStringMemberHasValueOrDefault = true;
  }
  if (otherDoubleOrStringMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[22].Get(isolate), otherDoubleOrStringMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> publicValue;
  bool publicHasValueOrDefault = false;
  if (impl.hasIsPublic()) {
    publicValue = v8::Boolean::New(isolate, impl.isPublic());
    publicHasValueOrDefault = true;
  }
  if (publicHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[23].Get(isolate), publicValue))) {
    return false;
  }

  v8::Local<v8::Value> recordMemberValue;
  bool recordMemberHasValueOrDefault = false;
  if (impl.hasRecordMember()) {
    recordMemberValue = ToV8(impl.recordMember(), creationContext, isolate);
    recordMemberHasValueOrDefault = true;
  }
  if (recordMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[24].Get(isolate), recordMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> restrictedDoubleMemberValue;
  bool restrictedDoubleMemberHasValueOrDefault = false;
  if (impl.hasRestrictedDoubleMember()) {
    restrictedDoubleMemberValue = v8::Number::New(isolate, impl.restrictedDoubleMember());
    restrictedDoubleMemberHasValueOrDefault = true;
  } else {
    restrictedDoubleMemberValue = v8::Number::New(isolate, 3.14);
    restrictedDoubleMemberHasValueOrDefault = true;
  }
  if (restrictedDoubleMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[25].Get(isolate), restrictedDoubleMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> stringMemberValue;
  bool stringMemberHasValueOrDefault = false;
  if (impl.hasStringMember()) {
    stringMemberValue = V8String(isolate, impl.stringMember());
    stringMemberHasValueOrDefault = true;
  }
  if (stringMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[28].Get(isolate), stringMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> stringOrNullMemberValue;
  bool stringOrNullMemberHasValueOrDefault = false;
  if (impl.hasStringOrNullMember()) {
    stringOrNullMemberValue = V8String(isolate, impl.stringOrNullMember());
    stringOrNullMemberHasValueOrDefault = true;
  } else {
    stringOrNullMemberValue = V8String(isolate, "default string value");
    stringOrNullMemberHasValueOrDefault = true;
  }
  if (stringOrNullMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[29].Get(isolate), stringOrNullMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> stringSequenceMemberValue;
  bool stringSequenceMemberHasValueOrDefault = false;
  if (impl.hasStringSequenceMember()) {
    stringSequenceMemberValue = ToV8(impl.stringSequenceMember(), creationContext, isolate);
    stringSequenceMemberHasValueOrDefault = true;
  } else {
    stringSequenceMemberValue = ToV8(Vector<String>(), creationContext, isolate);
    stringSequenceMemberHasValueOrDefault = true;
  }
  if (stringSequenceMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[30].Get(isolate), stringSequenceMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> testInterface2OrUint8ArrayMemberValue;
  bool testInterface2OrUint8ArrayMemberHasValueOrDefault = false;
  if (impl.hasTestInterface2OrUint8ArrayMember()) {
    testInterface2OrUint8ArrayMemberValue = ToV8(impl.testInterface2OrUint8ArrayMember(), creationContext, isolate);
    testInterface2OrUint8ArrayMemberHasValueOrDefault = true;
  }
  if (testInterface2OrUint8ArrayMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[31].Get(isolate), testInterface2OrUint8ArrayMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> testInterfaceGarbageCollectedMemberValue;
  bool testInterfaceGarbageCollectedMemberHasValueOrDefault = false;
  if (impl.hasTestInterfaceGarbageCollectedMember()) {
    testInterfaceGarbageCollectedMemberValue = ToV8(impl.testInterfaceGarbageCollectedMember(), creationContext, isolate);
    testInterfaceGarbageCollectedMemberHasValueOrDefault = true;
  }
  if (testInterfaceGarbageCollectedMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[32].Get(isolate), testInterfaceGarbageCollectedMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> testInterfaceGarbageCollectedOrNullMemberValue;
  bool testInterfaceGarbageCollectedOrNullMemberHasValueOrDefault = false;
  if (impl.hasTestInterfaceGarbageCollectedOrNullMember()) {
    testInterfaceGarbageCollectedOrNullMemberValue = ToV8(impl.testInterfaceGarbageCollectedOrNullMember(), creationContext, isolate);
    testInterfaceGarbageCollectedOrNullMemberHasValueOrDefault = true;
  } else {
    testInterfaceGarbageCollectedOrNullMemberValue = v8::Null(isolate);
    testInterfaceGarbageCollectedOrNullMemberHasValueOrDefault = true;
  }
  if (testInterfaceGarbageCollectedOrNullMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[33].Get(isolate), testInterfaceGarbageCollectedOrNullMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> testInterfaceGarbageCollectedSequenceMemberValue;
  bool testInterfaceGarbageCollectedSequenceMemberHasValueOrDefault = false;
  if (impl.hasTestInterfaceGarbageCollectedSequenceMember()) {
    testInterfaceGarbageCollectedSequenceMemberValue = ToV8(impl.testInterfaceGarbageCollectedSequenceMember(), creationContext, isolate);
    testInterfaceGarbageCollectedSequenceMemberHasValueOrDefault = true;
  } else {
    testInterfaceGarbageCollectedSequenceMemberValue = ToV8(HeapVector<Member<TestInterfaceGarbageCollected>>(), creationContext, isolate);
    testInterfaceGarbageCollectedSequenceMemberHasValueOrDefault = true;
  }
  if (testInterfaceGarbageCollectedSequenceMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[34].Get(isolate), testInterfaceGarbageCollectedSequenceMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> testInterfaceMemberValue;
  bool testInterfaceMemberHasValueOrDefault = false;
  if (impl.hasTestInterfaceMember()) {
    testInterfaceMemberValue = ToV8(impl.testInterfaceMember(), creationContext, isolate);
    testInterfaceMemberHasValueOrDefault = true;
  }
  if (testInterfaceMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[35].Get(isolate), testInterfaceMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> testInterfaceOrNullMemberValue;
  bool testInterfaceOrNullMemberHasValueOrDefault = false;
  if (impl.hasTestInterfaceOrNullMember()) {
    testInterfaceOrNullMemberValue = ToV8(impl.testInterfaceOrNullMember(), creationContext, isolate);
    testInterfaceOrNullMemberHasValueOrDefault = true;
  } else {
    testInterfaceOrNullMemberValue = v8::Null(isolate);
    testInterfaceOrNullMemberHasValueOrDefault = true;
  }
  if (testInterfaceOrNullMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[36].Get(isolate), testInterfaceOrNullMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> testInterfaceSequenceMemberValue;
  bool testInterfaceSequenceMemberHasValueOrDefault = false;
  if (impl.hasTestInterfaceSequenceMember()) {
    testInterfaceSequenceMemberValue = ToV8(impl.testInterfaceSequenceMember(), creationContext, isolate);
    testInterfaceSequenceMemberHasValueOrDefault = true;
  } else {
    testInterfaceSequenceMemberValue = ToV8(HeapVector<Member<TestInterfaceImplementation>>(), creationContext, isolate);
    testInterfaceSequenceMemberHasValueOrDefault = true;
  }
  if (testInterfaceSequenceMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[37].Get(isolate), testInterfaceSequenceMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> testObjectSequenceMemberValue;
  bool testObjectSequenceMemberHasValueOrDefault = false;
  if (impl.hasTestObjectSequenceMember()) {
    testObjectSequenceMemberValue = ToV8(impl.testObjectSequenceMember(), creationContext, isolate);
    testObjectSequenceMemberHasValueOrDefault = true;
  }
  if (testObjectSequenceMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[38].Get(isolate), testObjectSequenceMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> treatNullAsStringSequenceMemberValue;
  bool treatNullAsStringSequenceMemberHasValueOrDefault = false;
  if (impl.hasTreatNullAsStringSequenceMember()) {
    treatNullAsStringSequenceMemberValue = ToV8(impl.treatNullAsStringSequenceMember(), creationContext, isolate);
    treatNullAsStringSequenceMemberHasValueOrDefault = true;
  } else {
    treatNullAsStringSequenceMemberValue = ToV8(Vector<String>(), creationContext, isolate);
    treatNullAsStringSequenceMemberHasValueOrDefault = true;
  }
  if (treatNullAsStringSequenceMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[39].Get(isolate), treatNullAsStringSequenceMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> uint8ArrayMemberValue;
  bool uint8ArrayMemberHasValueOrDefault = false;
  if (impl.hasUint8ArrayMember()) {
    uint8ArrayMemberValue = ToV8(impl.uint8ArrayMember(), creationContext, isolate);
    uint8ArrayMemberHasValueOrDefault = true;
  }
  if (uint8ArrayMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[40].Get(isolate), uint8ArrayMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> unionInRecordMemberValue;
  bool unionInRecordMemberHasValueOrDefault = false;
  if (impl.hasUnionInRecordMember()) {
    unionInRecordMemberValue = ToV8(impl.unionInRecordMember(), creationContext, isolate);
    unionInRecordMemberHasValueOrDefault = true;
  }
  if (unionInRecordMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[41].Get(isolate), unionInRecordMemberValue))) {
    return false;
  }

  v8::Local<v8::Value> unionWithTypedefsValue;
  bool unionWithTypedefsHasValueOrDefault = false;
  if (impl.hasUnionWithTypedefs()) {
    unionWithTypedefsValue = ToV8(impl.unionWithTypedefs(), creationContext, isolate);
    unionWithTypedefsHasValueOrDefault = true;
  }
  if (unionWithTypedefsHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[42].Get(isolate), unionWithTypedefsValue))) {
    return false;
  }

  v8::Local<v8::Value> unrestrictedDoubleMemberValue;
  bool unrestrictedDoubleMemberHasValueOrDefault = false;
  if (impl.hasUnrestrictedDoubleMember()) {
    unrestrictedDoubleMemberValue = v8::Number::New(isolate, impl.unrestrictedDoubleMember());
    unrestrictedDoubleMemberHasValueOrDefault = true;
  } else {
    unrestrictedDoubleMemberValue = v8::Number::New(isolate, 3.14);
    unrestrictedDoubleMemberHasValueOrDefault = true;
  }
  if (unrestrictedDoubleMemberHasValueOrDefault &&
      !V8CallBoolean(dictionary->CreateDataProperty(context, keys[43].Get(isolate), unrestrictedDoubleMemberValue))) {
    return false;
  }

  if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
    v8::Local<v8::Value> runtimeMemberValue;
    bool runtimeMemberHasValueOrDefault = false;
    if (impl.hasRuntimeMember()) {
      runtimeMemberValue = v8::Boolean::New(isolate, impl.runtimeMember());
      runtimeMemberHasValueOrDefault = true;
    }
    if (runtimeMemberHasValueOrDefault &&
        !V8CallBoolean(dictionary->CreateDataProperty(context, keys[26].Get(isolate), runtimeMemberValue))) {
      return false;
    }

    v8::Local<v8::Value> runtimeSecondMemberValue;
    bool runtimeSecondMemberHasValueOrDefault = false;
    if (impl.hasRuntimeSecondMember()) {
      runtimeSecondMemberValue = v8::Boolean::New(isolate, impl.runtimeSecondMember());
      runtimeSecondMemberHasValueOrDefault = true;
    }
    if (runtimeSecondMemberHasValueOrDefault &&
        !V8CallBoolean(dictionary->CreateDataProperty(context, keys[27].Get(isolate), runtimeSecondMemberValue))) {
      return false;
    }
  }

  if (OriginTrials::featureName1Enabled(executionContext)) {
    v8::Local<v8::Value> originTrialSecondMemberValue;
    bool originTrialSecondMemberHasValueOrDefault = false;
    if (impl.hasOriginTrialSecondMember()) {
      originTrialSecondMemberValue = v8::Boolean::New(isolate, impl.originTrialSecondMember());
      originTrialSecondMemberHasValueOrDefault = true;
    }
    if (originTrialSecondMemberHasValueOrDefault &&
        !V8CallBoolean(dictionary->CreateDataProperty(context, keys[21].Get(isolate), originTrialSecondMemberValue))) {
      return false;
    }
  }

  if (OriginTrials::featureNameEnabled(executionContext)) {
    v8::Local<v8::Value> originTrialMemberValue;
    bool originTrialMemberHasValueOrDefault = false;
    if (impl.hasOriginTrialMember()) {
      originTrialMemberValue = v8::Boolean::New(isolate, impl.originTrialMember());
      originTrialMemberHasValueOrDefault = true;
    }
    if (originTrialMemberHasValueOrDefault &&
        !V8CallBoolean(dictionary->CreateDataProperty(context, keys[20].Get(isolate), originTrialMemberValue))) {
      return false;
    }
  }

  return true;
}

TestDictionary NativeValueTraits<TestDictionary>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestDictionary impl;
  V8TestDictionary::ToImpl(isolate, value, impl, exceptionState);
  return impl;
}

}  // namespace blink
