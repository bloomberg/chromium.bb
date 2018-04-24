// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/dictionary_test.h"

#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/testing/internal_dictionary.h"
#include "third_party/blink/renderer/core/testing/internal_dictionary_derived.h"
#include "third_party/blink/renderer/core/testing/internal_dictionary_derived_derived.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {
namespace {
ScriptIterator GetIterator(const Dictionary& iterable,
                           ExecutionContext* execution_context) {
  v8::Local<v8::Value> iterator_getter;
  v8::Isolate* isolate = iterable.GetIsolate();
  if (!iterable.Get(v8::Symbol::GetIterator(isolate), iterator_getter) ||
      !iterator_getter->IsFunction()) {
    return nullptr;
  }
  v8::Local<v8::Value> iterator;
  if (!V8ScriptRunner::CallFunction(
           v8::Local<v8::Function>::Cast(iterator_getter), execution_context,
           iterable.V8Value(), 0, nullptr, isolate)
           .ToLocal(&iterator))
    return nullptr;
  if (!iterator->IsObject())
    return nullptr;
  return ScriptIterator(v8::Local<v8::Object>::Cast(iterator), isolate);
}
}  // namespace

DictionaryTest::DictionaryTest() : required_boolean_member_(false) {}

DictionaryTest::~DictionaryTest() = default;

void DictionaryTest::set(const InternalDictionary& testing_dictionary) {
  Reset();
  if (testing_dictionary.hasLongMember())
    long_member_ = testing_dictionary.longMember();
  if (testing_dictionary.hasLongMemberWithClamp())
    long_member_with_clamp_ = testing_dictionary.longMemberWithClamp();
  if (testing_dictionary.hasLongMemberWithEnforceRange())
    long_member_with_enforce_range_ =
        testing_dictionary.longMemberWithEnforceRange();
  long_member_with_default_ = testing_dictionary.longMemberWithDefault();
  if (testing_dictionary.hasLongOrNullMember())
    long_or_null_member_ = testing_dictionary.longOrNullMember();
  // |longOrNullMemberWithDefault| has a default value but can be null, so
  // we need to check availability.
  if (testing_dictionary.hasLongOrNullMemberWithDefault())
    long_or_null_member_with_default_ =
        testing_dictionary.longOrNullMemberWithDefault();
  if (testing_dictionary.hasBooleanMember())
    boolean_member_ = testing_dictionary.booleanMember();
  if (testing_dictionary.hasDoubleMember())
    double_member_ = testing_dictionary.doubleMember();
  if (testing_dictionary.hasUnrestrictedDoubleMember())
    unrestricted_double_member_ = testing_dictionary.unrestrictedDoubleMember();
  string_member_ = testing_dictionary.stringMember();
  string_member_with_default_ = testing_dictionary.stringMemberWithDefault();
  byte_string_member_ = testing_dictionary.byteStringMember();
  usv_string_member_ = testing_dictionary.usvStringMember();
  if (testing_dictionary.hasStringSequenceMember())
    string_sequence_member_ = testing_dictionary.stringSequenceMember();
  string_sequence_member_with_default_ =
      testing_dictionary.stringSequenceMemberWithDefault();
  if (testing_dictionary.hasStringSequenceOrNullMember())
    string_sequence_or_null_member_ =
        testing_dictionary.stringSequenceOrNullMember();
  enum_member_ = testing_dictionary.enumMember();
  enum_member_with_default_ = testing_dictionary.enumMemberWithDefault();
  enum_or_null_member_ = testing_dictionary.enumOrNullMember();
  if (testing_dictionary.hasElementMember())
    element_member_ = testing_dictionary.elementMember();
  if (testing_dictionary.hasElementOrNullMember())
    element_or_null_member_ = testing_dictionary.elementOrNullMember();
  object_member_ = testing_dictionary.objectMember();
  object_or_null_member_with_default_ =
      testing_dictionary.objectOrNullMemberWithDefault();
  if (testing_dictionary.hasDoubleOrStringMember())
    double_or_string_member_ = testing_dictionary.doubleOrStringMember();
  if (testing_dictionary.hasDoubleOrStringSequenceMember())
    double_or_string_sequence_member_ =
        testing_dictionary.doubleOrStringSequenceMember();
  event_target_or_null_member_ = testing_dictionary.eventTargetOrNullMember();
  if (testing_dictionary.hasDictionaryMember()) {
    NonThrowableExceptionState exception_state;
    dictionary_member_properties_ =
        testing_dictionary.dictionaryMember().GetOwnPropertiesAsStringHashMap(
            exception_state);
  }
  if (testing_dictionary.hasInternalEnumOrInternalEnumSequenceMember()) {
    internal_enum_or_internal_enum_sequence_ =
        testing_dictionary.internalEnumOrInternalEnumSequenceMember();
  }
  any_member_ = testing_dictionary.anyMember();
}

void DictionaryTest::get(InternalDictionary& result) {
  if (long_member_)
    result.setLongMember(long_member_.value());
  if (long_member_with_clamp_)
    result.setLongMemberWithClamp(long_member_with_clamp_.value());
  if (long_member_with_enforce_range_) {
    result.setLongMemberWithEnforceRange(
        long_member_with_enforce_range_.value());
  }
  result.setLongMemberWithDefault(long_member_with_default_);
  if (long_or_null_member_)
    result.setLongOrNullMember(long_or_null_member_.value());
  if (long_or_null_member_with_default_) {
    result.setLongOrNullMemberWithDefault(
        long_or_null_member_with_default_.value());
  }
  if (boolean_member_)
    result.setBooleanMember(boolean_member_.value());
  if (double_member_)
    result.setDoubleMember(double_member_.value());
  if (unrestricted_double_member_)
    result.setUnrestrictedDoubleMember(unrestricted_double_member_.value());
  result.setStringMember(string_member_);
  result.setStringMemberWithDefault(string_member_with_default_);
  result.setByteStringMember(byte_string_member_);
  result.setUsvStringMember(usv_string_member_);
  if (string_sequence_member_)
    result.setStringSequenceMember(string_sequence_member_.value());
  result.setStringSequenceMemberWithDefault(
      string_sequence_member_with_default_);
  if (string_sequence_or_null_member_) {
    result.setStringSequenceOrNullMember(
        string_sequence_or_null_member_.value());
  }
  result.setEnumMember(enum_member_);
  result.setEnumMemberWithDefault(enum_member_with_default_);
  result.setEnumOrNullMember(enum_or_null_member_);
  if (element_member_)
    result.setElementMember(element_member_);
  if (element_or_null_member_)
    result.setElementOrNullMember(element_or_null_member_);
  result.setObjectMember(object_member_);
  result.setObjectOrNullMemberWithDefault(object_or_null_member_with_default_);
  if (!double_or_string_member_.IsNull())
    result.setDoubleOrStringMember(double_or_string_member_);
  if (double_or_string_sequence_member_) {
    result.setDoubleOrStringSequenceMember(
        double_or_string_sequence_member_.value());
  }
  result.setEventTargetOrNullMember(event_target_or_null_member_);
  result.setInternalEnumOrInternalEnumSequenceMember(
      internal_enum_or_internal_enum_sequence_);
  result.setAnyMember(any_member_);
}

ScriptValue DictionaryTest::getDictionaryMemberProperties(
    ScriptState* script_state) {
  if (!dictionary_member_properties_)
    return ScriptValue();
  V8ObjectBuilder builder(script_state);
  HashMap<String, String> properties = dictionary_member_properties_.value();
  for (HashMap<String, String>::iterator it = properties.begin();
       it != properties.end(); ++it)
    builder.AddString(it->key, it->value);
  return builder.GetScriptValue();
}

void DictionaryTest::setDerived(const InternalDictionaryDerived& derived) {
  DCHECK(derived.hasRequiredBooleanMember());
  set(derived);
  if (derived.hasDerivedStringMember())
    derived_string_member_ = derived.derivedStringMember();
  derived_string_member_with_default_ =
      derived.derivedStringMemberWithDefault();
  required_boolean_member_ = derived.requiredBooleanMember();
}

void DictionaryTest::getDerived(InternalDictionaryDerived& result) {
  get(result);
  result.setDerivedStringMember(derived_string_member_);
  result.setDerivedStringMemberWithDefault(derived_string_member_with_default_);
  result.setRequiredBooleanMember(required_boolean_member_);
}

void DictionaryTest::setDerivedDerived(
    const InternalDictionaryDerivedDerived& derived) {
  setDerived(derived);
  if (derived.hasDerivedDerivedStringMember())
    derived_derived_string_member_ = derived.derivedDerivedStringMember();
}

void DictionaryTest::getDerivedDerived(
    InternalDictionaryDerivedDerived& result) {
  getDerived(result);
  result.setDerivedDerivedStringMember(derived_derived_string_member_);
}

String DictionaryTest::stringFromIterable(
    ScriptState* script_state,
    Dictionary iterable,
    ExceptionState& exception_state) const {
  StringBuilder result;
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  ScriptIterator iterator = GetIterator(iterable, execution_context);
  if (iterator.IsNull())
    return g_empty_string;

  bool first_loop = true;
  while (iterator.Next(execution_context, exception_state)) {
    if (exception_state.HadException())
      return g_empty_string;

    if (first_loop)
      first_loop = false;
    else
      result.Append(',');

    v8::Local<v8::Value> value;
    if (iterator.GetValue().ToLocal(&value))
      result.Append(ToCoreString(value->ToString()));
  }

  return result.ToString();
}

void DictionaryTest::Reset() {
  long_member_ = base::nullopt;
  long_member_with_clamp_ = base::nullopt;
  long_member_with_enforce_range_ = base::nullopt;
  long_member_with_default_ = -1;  // This value should not be returned.
  long_or_null_member_ = base::nullopt;
  long_or_null_member_with_default_ = base::nullopt;
  boolean_member_ = base::nullopt;
  double_member_ = base::nullopt;
  unrestricted_double_member_ = base::nullopt;
  string_member_ = String();
  string_member_with_default_ = String("Should not be returned");
  string_sequence_member_ = base::nullopt;
  string_sequence_member_with_default_.Fill("Should not be returned", 1);
  string_sequence_or_null_member_ = base::nullopt;
  enum_member_ = String();
  enum_member_with_default_ = String();
  enum_or_null_member_ = String();
  element_member_ = nullptr;
  element_or_null_member_ = nullptr;
  object_member_ = ScriptValue();
  object_or_null_member_with_default_ = ScriptValue();
  double_or_string_member_ = DoubleOrString();
  event_target_or_null_member_ = nullptr;
  derived_string_member_ = String();
  derived_string_member_with_default_ = String();
  required_boolean_member_ = false;
  dictionary_member_properties_ = base::nullopt;
  internal_enum_or_internal_enum_sequence_ =
      InternalEnumOrInternalEnumSequence();
  any_member_ = ScriptValue();
}

void DictionaryTest::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_member_);
  visitor->Trace(element_or_null_member_);
  visitor->Trace(double_or_string_sequence_member_);
  visitor->Trace(event_target_or_null_member_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
