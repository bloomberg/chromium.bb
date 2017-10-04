// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/dictionary_impl.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef TestDictionary_h
#define TestDictionary_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/IDLDictionaryBase.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/double_or_string.h"
#include "bindings/core/v8/float_or_boolean.h"
#include "bindings/core/v8/long_or_boolean.h"
#include "bindings/core/v8/test_interface_2_or_uint8_array.h"
#include "bindings/tests/idls/core/TestInterface2.h"
#include "core/CoreExport.h"
#include "core/testing/InternalDictionary.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class EventTarget;
class TestInterfaceGarbageCollected;
class TestObject;
class TestInterfaceImplementation;
class Element;

class CORE_EXPORT TestDictionary : public IDLDictionaryBase {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  TestDictionary();
  virtual ~TestDictionary();
  TestDictionary(const TestDictionary&);
  TestDictionary& operator=(const TestDictionary&);

  bool hasAnyInRecordMember() const { return has_any_in_record_member_; }
  const Vector<std::pair<String, ScriptValue>>& anyInRecordMember() const {
    DCHECK(has_any_in_record_member_);
    return any_in_record_member_;
  }
  void setAnyInRecordMember(const Vector<std::pair<String, ScriptValue>>&);

  bool hasAnyMember() const { return !(any_member_.IsEmpty() || any_member_.IsNull() || any_member_.IsUndefined()); }
  ScriptValue anyMember() const {
    return any_member_;
  }
  void setAnyMember(ScriptValue);

  bool hasApplicableToTypeLongMember() const { return has_applicable_to_type_long_member_; }
  int32_t applicableToTypeLongMember() const {
    DCHECK(has_applicable_to_type_long_member_);
    return applicable_to_type_long_member_;
  }
  inline void setApplicableToTypeLongMember(int32_t);

  bool hasApplicableToTypeStringMember() const { return !applicable_to_type_string_member_.IsNull(); }
  const String& applicableToTypeStringMember() const {
    return applicable_to_type_string_member_;
  }
  inline void setApplicableToTypeStringMember(const String&);

  bool hasBooleanMember() const { return has_boolean_member_; }
  bool booleanMember() const {
    DCHECK(has_boolean_member_);
    return boolean_member_;
  }
  inline void setBooleanMember(bool);

  bool hasCreateMember() const { return has_create_member_; }
  bool createMember() const {
    DCHECK(has_create_member_);
    return create_member_;
  }
  inline void setCreateMember(bool);

  bool hasDictionaryMember() const { return !dictionary_member_.IsUndefinedOrNull(); }
  Dictionary dictionaryMember() const {
    return dictionary_member_;
  }
  void setDictionaryMember(Dictionary);

  bool hasDoubleOrNullMember() const { return has_double_or_null_member_; }
  double doubleOrNullMember() const {
    DCHECK(has_double_or_null_member_);
    return double_or_null_member_;
  }
  inline void setDoubleOrNullMember(double);
  inline void setDoubleOrNullMemberToNull();

  bool hasDoubleOrStringMember() const { return !double_or_string_member_.IsNull(); }
  const DoubleOrString& doubleOrStringMember() const {
    return double_or_string_member_;
  }
  void setDoubleOrStringMember(const DoubleOrString&);

  bool hasDoubleOrStringSequenceMember() const { return has_double_or_string_sequence_member_; }
  const HeapVector<DoubleOrString>& doubleOrStringSequenceMember() const {
    DCHECK(has_double_or_string_sequence_member_);
    return double_or_string_sequence_member_;
  }
  void setDoubleOrStringSequenceMember(const HeapVector<DoubleOrString>&);

  bool hasElementOrNullMember() const { return element_or_null_member_; }
  Element* elementOrNullMember() const {
    return element_or_null_member_;
  }
  inline void setElementOrNullMember(Element*);
  inline void setElementOrNullMemberToNull();

  bool hasEnumMember() const { return !enum_member_.IsNull(); }
  const String& enumMember() const {
    return enum_member_;
  }
  inline void setEnumMember(const String&);

  bool hasEnumSequenceMember() const { return has_enum_sequence_member_; }
  const Vector<String>& enumSequenceMember() const {
    DCHECK(has_enum_sequence_member_);
    return enum_sequence_member_;
  }
  void setEnumSequenceMember(const Vector<String>&);

  bool hasEventTargetMember() const { return event_target_member_; }
  EventTarget* eventTargetMember() const {
    return event_target_member_;
  }
  inline void setEventTargetMember(EventTarget*);

  bool hasGarbageCollectedRecordMember() const { return has_garbage_collected_record_member_; }
  const HeapVector<std::pair<String, Member<TestObject>>>& garbageCollectedRecordMember() const {
    DCHECK(has_garbage_collected_record_member_);
    return garbage_collected_record_member_;
  }
  void setGarbageCollectedRecordMember(const HeapVector<std::pair<String, Member<TestObject>>>&);

  bool hasInternalDictionarySequenceMember() const { return has_internal_dictionary_sequence_member_; }
  const HeapVector<InternalDictionary>& internalDictionarySequenceMember() const {
    DCHECK(has_internal_dictionary_sequence_member_);
    return internal_dictionary_sequence_member_;
  }
  void setInternalDictionarySequenceMember(const HeapVector<InternalDictionary>&);

  bool hasIsPublic() const { return has_is_public_; }
  bool isPublic() const {
    DCHECK(has_is_public_);
    return is_public_;
  }
  inline void setIsPublic(bool);

  bool hasLongMember() const { return has_long_member_; }
  int32_t longMember() const {
    DCHECK(has_long_member_);
    return long_member_;
  }
  inline void setLongMember(int32_t);

  bool hasObjectMember() const { return !(object_member_.IsEmpty() || object_member_.IsNull() || object_member_.IsUndefined()); }
  ScriptValue objectMember() const {
    return object_member_;
  }
  void setObjectMember(ScriptValue);

  bool hasObjectOrNullMember() const { return !(object_or_null_member_.IsEmpty() || object_or_null_member_.IsNull() || object_or_null_member_.IsUndefined()); }
  ScriptValue objectOrNullMember() const {
    return object_or_null_member_;
  }
  void setObjectOrNullMember(ScriptValue);
  void setObjectOrNullMemberToNull();

  bool hasOtherDoubleOrStringMember() const { return !other_double_or_string_member_.IsNull(); }
  const DoubleOrString& otherDoubleOrStringMember() const {
    return other_double_or_string_member_;
  }
  void setOtherDoubleOrStringMember(const DoubleOrString&);

  bool hasRecordMember() const { return has_record_member_; }
  const Vector<std::pair<String, int8_t>>& recordMember() const {
    DCHECK(has_record_member_);
    return record_member_;
  }
  void setRecordMember(const Vector<std::pair<String, int8_t>>&);

  bool hasRestrictedDoubleMember() const { return has_restricted_double_member_; }
  double restrictedDoubleMember() const {
    DCHECK(has_restricted_double_member_);
    return restricted_double_member_;
  }
  inline void setRestrictedDoubleMember(double);

  bool hasRuntimeMember() const { return has_runtime_member_; }
  bool runtimeMember() const {
    DCHECK(has_runtime_member_);
    return runtime_member_;
  }
  inline void setRuntimeMember(bool);

  bool hasStringMember() const { return !string_member_.IsNull(); }
  const String& stringMember() const {
    return string_member_;
  }
  inline void setStringMember(const String&);

  bool hasStringOrNullMember() const { return !string_or_null_member_.IsNull(); }
  const String& stringOrNullMember() const {
    return string_or_null_member_;
  }
  inline void setStringOrNullMember(const String&);
  inline void setStringOrNullMemberToNull();

  bool hasStringSequenceMember() const { return has_string_sequence_member_; }
  const Vector<String>& stringSequenceMember() const {
    DCHECK(has_string_sequence_member_);
    return string_sequence_member_;
  }
  void setStringSequenceMember(const Vector<String>&);

  bool hasTestInterface2OrUint8ArrayMember() const { return !test_interface_2_or_uint8_array_member_.IsNull(); }
  const TestInterface2OrUint8Array& testInterface2OrUint8ArrayMember() const {
    return test_interface_2_or_uint8_array_member_;
  }
  void setTestInterface2OrUint8ArrayMember(const TestInterface2OrUint8Array&);

  bool hasTestInterfaceGarbageCollectedMember() const { return test_interface_garbage_collected_member_; }
  TestInterfaceGarbageCollected* testInterfaceGarbageCollectedMember() const {
    return test_interface_garbage_collected_member_;
  }
  inline void setTestInterfaceGarbageCollectedMember(TestInterfaceGarbageCollected*);

  bool hasTestInterfaceGarbageCollectedOrNullMember() const { return test_interface_garbage_collected_or_null_member_; }
  TestInterfaceGarbageCollected* testInterfaceGarbageCollectedOrNullMember() const {
    return test_interface_garbage_collected_or_null_member_;
  }
  inline void setTestInterfaceGarbageCollectedOrNullMember(TestInterfaceGarbageCollected*);
  inline void setTestInterfaceGarbageCollectedOrNullMemberToNull();

  bool hasTestInterfaceGarbageCollectedSequenceMember() const { return has_test_interface_garbage_collected_sequence_member_; }
  const HeapVector<Member<TestInterfaceGarbageCollected>>& testInterfaceGarbageCollectedSequenceMember() const {
    DCHECK(has_test_interface_garbage_collected_sequence_member_);
    return test_interface_garbage_collected_sequence_member_;
  }
  void setTestInterfaceGarbageCollectedSequenceMember(const HeapVector<Member<TestInterfaceGarbageCollected>>&);

  bool hasTestInterfaceMember() const { return test_interface_member_; }
  TestInterfaceImplementation* testInterfaceMember() const {
    return test_interface_member_;
  }
  inline void setTestInterfaceMember(TestInterfaceImplementation*);

  bool hasTestInterfaceOrNullMember() const { return test_interface_or_null_member_; }
  TestInterfaceImplementation* testInterfaceOrNullMember() const {
    return test_interface_or_null_member_;
  }
  inline void setTestInterfaceOrNullMember(TestInterfaceImplementation*);
  inline void setTestInterfaceOrNullMemberToNull();

  bool hasTestInterfaceSequenceMember() const { return has_test_interface_sequence_member_; }
  const HeapVector<Member<TestInterfaceImplementation>>& testInterfaceSequenceMember() const {
    DCHECK(has_test_interface_sequence_member_);
    return test_interface_sequence_member_;
  }
  void setTestInterfaceSequenceMember(const HeapVector<Member<TestInterfaceImplementation>>&);

  bool hasTestObjectSequenceMember() const { return has_test_object_sequence_member_; }
  const HeapVector<Member<TestObject>>& testObjectSequenceMember() const {
    DCHECK(has_test_object_sequence_member_);
    return test_object_sequence_member_;
  }
  void setTestObjectSequenceMember(const HeapVector<Member<TestObject>>&);

  bool hasTreatNullAsStringSequenceMember() const { return has_treat_null_as_string_sequence_member_; }
  const Vector<String>& treatNullAsStringSequenceMember() const {
    DCHECK(has_treat_null_as_string_sequence_member_);
    return treat_null_as_string_sequence_member_;
  }
  void setTreatNullAsStringSequenceMember(const Vector<String>&);

  bool hasUint8ArrayMember() const { return uint8_array_member_; }
  NotShared<DOMUint8Array> uint8ArrayMember() const {
    return uint8_array_member_;
  }
  inline void setUint8ArrayMember(NotShared<DOMUint8Array>);

  bool hasUnionInRecordMember() const { return has_union_in_record_member_; }
  const HeapVector<std::pair<String, LongOrBoolean>>& unionInRecordMember() const {
    DCHECK(has_union_in_record_member_);
    return union_in_record_member_;
  }
  void setUnionInRecordMember(const HeapVector<std::pair<String, LongOrBoolean>>&);

  bool hasUnionWithTypedefs() const { return !union_with_typedefs_.IsNull(); }
  const FloatOrBoolean& unionWithTypedefs() const {
    return union_with_typedefs_;
  }
  void setUnionWithTypedefs(const FloatOrBoolean&);

  bool hasUnrestrictedDoubleMember() const { return has_unrestricted_double_member_; }
  double unrestrictedDoubleMember() const {
    DCHECK(has_unrestricted_double_member_);
    return unrestricted_double_member_;
  }
  inline void setUnrestrictedDoubleMember(double);

  v8::Local<v8::Value> ToV8Impl(v8::Local<v8::Object>, v8::Isolate*) const override;
  DECLARE_VIRTUAL_TRACE();

 private:
  bool has_any_in_record_member_ = false;
  bool has_applicable_to_type_long_member_ = false;
  bool has_boolean_member_ = false;
  bool has_create_member_ = false;
  bool has_double_or_null_member_ = false;
  bool has_double_or_string_sequence_member_ = false;
  bool has_enum_sequence_member_ = false;
  bool has_garbage_collected_record_member_ = false;
  bool has_internal_dictionary_sequence_member_ = false;
  bool has_is_public_ = false;
  bool has_long_member_ = false;
  bool has_record_member_ = false;
  bool has_restricted_double_member_ = false;
  bool has_runtime_member_ = false;
  bool has_string_sequence_member_ = false;
  bool has_test_interface_garbage_collected_sequence_member_ = false;
  bool has_test_interface_sequence_member_ = false;
  bool has_test_object_sequence_member_ = false;
  bool has_treat_null_as_string_sequence_member_ = false;
  bool has_union_in_record_member_ = false;
  bool has_unrestricted_double_member_ = false;

  Vector<std::pair<String, ScriptValue>> any_in_record_member_;
  ScriptValue any_member_;
  int32_t applicable_to_type_long_member_;
  String applicable_to_type_string_member_;
  bool boolean_member_;
  bool create_member_;
  Dictionary dictionary_member_;
  double double_or_null_member_;
  DoubleOrString double_or_string_member_;
  HeapVector<DoubleOrString> double_or_string_sequence_member_;
  Member<Element> element_or_null_member_;
  String enum_member_;
  Vector<String> enum_sequence_member_;
  Member<EventTarget> event_target_member_;
  HeapVector<std::pair<String, Member<TestObject>>> garbage_collected_record_member_;
  HeapVector<InternalDictionary> internal_dictionary_sequence_member_;
  bool is_public_;
  int32_t long_member_;
  ScriptValue object_member_;
  ScriptValue object_or_null_member_;
  DoubleOrString other_double_or_string_member_;
  Vector<std::pair<String, int8_t>> record_member_;
  double restricted_double_member_;
  bool runtime_member_;
  String string_member_;
  String string_or_null_member_;
  Vector<String> string_sequence_member_;
  TestInterface2OrUint8Array test_interface_2_or_uint8_array_member_;
  Member<TestInterfaceGarbageCollected> test_interface_garbage_collected_member_;
  Member<TestInterfaceGarbageCollected> test_interface_garbage_collected_or_null_member_;
  HeapVector<Member<TestInterfaceGarbageCollected>> test_interface_garbage_collected_sequence_member_;
  Member<TestInterfaceImplementation> test_interface_member_;
  Member<TestInterfaceImplementation> test_interface_or_null_member_;
  HeapVector<Member<TestInterfaceImplementation>> test_interface_sequence_member_;
  HeapVector<Member<TestObject>> test_object_sequence_member_;
  Vector<String> treat_null_as_string_sequence_member_;
  Member<DOMUint8Array> uint8_array_member_;
  HeapVector<std::pair<String, LongOrBoolean>> union_in_record_member_;
  FloatOrBoolean union_with_typedefs_;
  double unrestricted_double_member_;

  friend class V8TestDictionary;
};

void TestDictionary::setApplicableToTypeLongMember(int32_t value) {
  applicable_to_type_long_member_ = value;
  has_applicable_to_type_long_member_ = true;
}

void TestDictionary::setApplicableToTypeStringMember(const String& value) {
  applicable_to_type_string_member_ = value;
}

void TestDictionary::setBooleanMember(bool value) {
  boolean_member_ = value;
  has_boolean_member_ = true;
}

void TestDictionary::setCreateMember(bool value) {
  create_member_ = value;
  has_create_member_ = true;
}

void TestDictionary::setDoubleOrNullMember(double value) {
  double_or_null_member_ = value;
  has_double_or_null_member_ = true;
}
void TestDictionary::setDoubleOrNullMemberToNull() {
  has_double_or_null_member_ = false;
}

void TestDictionary::setElementOrNullMember(Element* value) {
  element_or_null_member_ = value;
}
void TestDictionary::setElementOrNullMemberToNull() {
  element_or_null_member_ = Member<Element>();
}

void TestDictionary::setEnumMember(const String& value) {
  enum_member_ = value;
}

void TestDictionary::setEventTargetMember(EventTarget* value) {
  event_target_member_ = value;
}

void TestDictionary::setIsPublic(bool value) {
  is_public_ = value;
  has_is_public_ = true;
}

void TestDictionary::setLongMember(int32_t value) {
  long_member_ = value;
  has_long_member_ = true;
}

void TestDictionary::setRestrictedDoubleMember(double value) {
  restricted_double_member_ = value;
  has_restricted_double_member_ = true;
}

void TestDictionary::setRuntimeMember(bool value) {
  runtime_member_ = value;
  has_runtime_member_ = true;
}

void TestDictionary::setStringMember(const String& value) {
  string_member_ = value;
}

void TestDictionary::setStringOrNullMember(const String& value) {
  string_or_null_member_ = value;
}
void TestDictionary::setStringOrNullMemberToNull() {
  string_or_null_member_ = String();
}

void TestDictionary::setTestInterfaceGarbageCollectedMember(TestInterfaceGarbageCollected* value) {
  test_interface_garbage_collected_member_ = value;
}

void TestDictionary::setTestInterfaceGarbageCollectedOrNullMember(TestInterfaceGarbageCollected* value) {
  test_interface_garbage_collected_or_null_member_ = value;
}
void TestDictionary::setTestInterfaceGarbageCollectedOrNullMemberToNull() {
  test_interface_garbage_collected_or_null_member_ = Member<TestInterfaceGarbageCollected>();
}

void TestDictionary::setTestInterfaceMember(TestInterfaceImplementation* value) {
  test_interface_member_ = value;
}

void TestDictionary::setTestInterfaceOrNullMember(TestInterfaceImplementation* value) {
  test_interface_or_null_member_ = value;
}
void TestDictionary::setTestInterfaceOrNullMemberToNull() {
  test_interface_or_null_member_ = Member<TestInterfaceImplementation>();
}

void TestDictionary::setUint8ArrayMember(NotShared<DOMUint8Array> value) {
  uint8_array_member_ = value.View();
}

void TestDictionary::setUnrestrictedDoubleMember(double value) {
  unrestricted_double_member_ = value;
  has_unrestricted_double_member_ = true;
}

}  // namespace blink

#endif  // TestDictionary_h
