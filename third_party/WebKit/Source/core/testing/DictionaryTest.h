// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DictionaryTest_h
#define DictionaryTest_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/double_or_string.h"
#include "bindings/core/v8/internal_enum_or_internal_enum_sequence.h"
#include "core/dom/Element.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class InternalDictionary;
class InternalDictionaryDerived;
class InternalDictionaryDerivedDerived;
class ScriptState;

class DictionaryTest : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DictionaryTest* Create() { return new DictionaryTest(); }
  virtual ~DictionaryTest();

  // Stores all members into corresponding fields
  void set(const InternalDictionary&);
  // Sets each member of the given TestDictionary from fields
  void get(InternalDictionary&);
  // Returns properties of the latest |dictionaryMember| which was set via
  // set().
  ScriptValue getDictionaryMemberProperties(ScriptState*);

  void setDerived(const InternalDictionaryDerived&);
  void getDerived(InternalDictionaryDerived&);

  void setDerivedDerived(const InternalDictionaryDerivedDerived&);
  void getDerivedDerived(InternalDictionaryDerivedDerived&);

  String stringFromIterable(ScriptState*,
                            Dictionary iterable,
                            ExceptionState&) const;

  void Trace(blink::Visitor*);

 private:
  DictionaryTest();

  void Reset();

  // The reason to use Optional<T> is convenience; we use Optional<T> here to
  // record whether the member field is set or not. Some members are not
  // wrapped with Optional because:
  //  - |longMemberWithDefault| has a non-null default value
  //  - String and PtrTypes can express whether they are null
  Optional<int> long_member_;
  Optional<int> long_member_with_clamp_;
  Optional<int> long_member_with_enforce_range_;
  int long_member_with_default_;
  Optional<int> long_or_null_member_;
  Optional<int> long_or_null_member_with_default_;
  Optional<bool> boolean_member_;
  Optional<double> double_member_;
  Optional<double> unrestricted_double_member_;
  String string_member_;
  String string_member_with_default_;
  String byte_string_member_;
  String usv_string_member_;
  Optional<Vector<String>> string_sequence_member_;
  Vector<String> string_sequence_member_with_default_;
  Optional<Vector<String>> string_sequence_or_null_member_;
  String enum_member_;
  String enum_member_with_default_;
  String enum_or_null_member_;
  Member<Element> element_member_;
  Member<Element> element_or_null_member_;
  ScriptValue object_member_;
  ScriptValue object_or_null_member_with_default_;
  DoubleOrString double_or_string_member_;
  Optional<HeapVector<DoubleOrString>> double_or_string_sequence_member_;
  Member<EventTarget> event_target_or_null_member_;
  String derived_string_member_;
  String derived_string_member_with_default_;
  String derived_derived_string_member_;
  bool required_boolean_member_;
  Optional<HashMap<String, String>> dictionary_member_properties_;
  InternalEnumOrInternalEnumSequence internal_enum_or_internal_enum_sequence_;
  ScriptValue any_member_;
};

}  // namespace blink

#endif  // DictionaryTest_h
