// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/DictionaryTest.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/dom/ExecutionContext.h"
#include "core/testing/InternalDictionary.h"
#include "core/testing/InternalDictionaryDerived.h"
#include "core/testing/InternalDictionaryDerivedDerived.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

DictionaryTest::DictionaryTest() : required_boolean_member_(false) {}

DictionaryTest::~DictionaryTest() {}

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
  prefix_get_member_ = testing_dictionary.getPrefixGetMember();
}

void DictionaryTest::get(InternalDictionary& result) {
  if (long_member_)
    result.setLongMember(long_member_.Get());
  if (long_member_with_clamp_)
    result.setLongMemberWithClamp(long_member_with_clamp_.Get());
  if (long_member_with_enforce_range_)
    result.setLongMemberWithEnforceRange(long_member_with_enforce_range_.Get());
  result.setLongMemberWithDefault(long_member_with_default_);
  if (long_or_null_member_)
    result.setLongOrNullMember(long_or_null_member_.Get());
  if (long_or_null_member_with_default_)
    result.setLongOrNullMemberWithDefault(
        long_or_null_member_with_default_.Get());
  if (boolean_member_)
    result.setBooleanMember(boolean_member_.Get());
  if (double_member_)
    result.setDoubleMember(double_member_.Get());
  if (unrestricted_double_member_)
    result.setUnrestrictedDoubleMember(unrestricted_double_member_.Get());
  result.setStringMember(string_member_);
  result.setStringMemberWithDefault(string_member_with_default_);
  result.setByteStringMember(byte_string_member_);
  result.setUsvStringMember(usv_string_member_);
  if (string_sequence_member_)
    result.setStringSequenceMember(string_sequence_member_.Get());
  result.setStringSequenceMemberWithDefault(
      string_sequence_member_with_default_);
  if (string_sequence_or_null_member_)
    result.setStringSequenceOrNullMember(string_sequence_or_null_member_.Get());
  result.setEnumMember(enum_member_);
  result.setEnumMemberWithDefault(enum_member_with_default_);
  result.setEnumOrNullMember(enum_or_null_member_);
  if (element_member_)
    result.setElementMember(element_member_);
  if (element_or_null_member_)
    result.setElementOrNullMember(element_or_null_member_);
  result.setObjectMember(object_member_);
  result.setObjectOrNullMemberWithDefault(object_or_null_member_with_default_);
  if (!double_or_string_member_.isNull())
    result.setDoubleOrStringMember(double_or_string_member_);
  if (!double_or_string_sequence_member_.IsNull())
    result.setDoubleOrStringSequenceMember(
        double_or_string_sequence_member_.Get());
  result.setEventTargetOrNullMember(event_target_or_null_member_);
  result.setPrefixGetMember(prefix_get_member_);
}

ScriptValue DictionaryTest::getDictionaryMemberProperties(
    ScriptState* script_state) {
  if (!dictionary_member_properties_)
    return ScriptValue();
  V8ObjectBuilder builder(script_state);
  HashMap<String, String> properties = dictionary_member_properties_.Get();
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
  DictionaryIterator iterator = iterable.GetIterator(execution_context);
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
  long_member_ = nullptr;
  long_member_with_clamp_ = nullptr;
  long_member_with_enforce_range_ = nullptr;
  long_member_with_default_ = -1;  // This value should not be returned.
  long_or_null_member_ = nullptr;
  long_or_null_member_with_default_ = nullptr;
  boolean_member_ = nullptr;
  double_member_ = nullptr;
  unrestricted_double_member_ = nullptr;
  string_member_ = String();
  string_member_with_default_ = String("Should not be returned");
  string_sequence_member_ = nullptr;
  string_sequence_member_with_default_.Fill("Should not be returned", 1);
  string_sequence_or_null_member_ = nullptr;
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
  dictionary_member_properties_ = nullptr;
  prefix_get_member_ = ScriptValue();
}

DEFINE_TRACE(DictionaryTest) {
  visitor->Trace(element_member_);
  visitor->Trace(element_or_null_member_);
  visitor->Trace(double_or_string_sequence_member_);
  visitor->Trace(event_target_or_null_member_);
}

}  // namespace blink
