// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DictionaryTest_h
#define DictionaryTest_h

#include "bindings/core/v8/DoubleOrString.h"
#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/Element.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class InternalDictionary;
class InternalDictionaryDerived;
class InternalDictionaryDerivedDerived;
class ScriptState;

class DictionaryTest : public GarbageCollectedFinalized<DictionaryTest>,
                       public ScriptWrappable {
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

  DECLARE_TRACE();

 private:
  DictionaryTest();

  void Reset();

  // The reason to use Nullable<T> is convenience; we use Nullable<T> here to
  // record whether the member field is set or not. Some members are not
  // wrapped with Nullable because:
  //  - |longMemberWithDefault| has a non-null default value
  //  - String and PtrTypes can express whether they are null
  Nullable<int> long_member_;
  Nullable<int> long_member_with_clamp_;
  Nullable<int> long_member_with_enforce_range_;
  int long_member_with_default_;
  Nullable<int> long_or_null_member_;
  Nullable<int> long_or_null_member_with_default_;
  Nullable<bool> boolean_member_;
  Nullable<double> double_member_;
  Nullable<double> unrestricted_double_member_;
  String string_member_;
  String string_member_with_default_;
  String byte_string_member_;
  String usv_string_member_;
  Nullable<Vector<String>> string_sequence_member_;
  Vector<String> string_sequence_member_with_default_;
  Nullable<Vector<String>> string_sequence_or_null_member_;
  String enum_member_;
  String enum_member_with_default_;
  String enum_or_null_member_;
  Member<Element> element_member_;
  Member<Element> element_or_null_member_;
  ScriptValue object_member_;
  ScriptValue object_or_null_member_with_default_;
  DoubleOrString double_or_string_member_;
  Nullable<HeapVector<DoubleOrString>> double_or_string_sequence_member_;
  Member<EventTarget> event_target_or_null_member_;
  String derived_string_member_;
  String derived_string_member_with_default_;
  String derived_derived_string_member_;
  bool required_boolean_member_;
  Nullable<HashMap<String, String>> dictionary_member_properties_;
  ScriptValue prefix_get_member_;
};

}  // namespace blink

#endif  // DictionaryTest_h
