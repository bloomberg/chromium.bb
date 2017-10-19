// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RecordTest_h
#define RecordTest_h

#include <utility>
#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/boolean_or_byte_string_byte_string_record.h"
#include "bindings/core/v8/float_or_string_element_record.h"
#include "core/dom/Element.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class RecordTest final : public GarbageCollectedFinalized<RecordTest>,
                         public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RecordTest* Create() { return new RecordTest; }
  ~RecordTest();

  void setStringLongRecord(const Vector<std::pair<String, int32_t>>& arg);
  Vector<std::pair<String, int32_t>> getStringLongRecord();

  void setNullableStringLongRecord(
      const Nullable<Vector<std::pair<String, int32_t>>>& arg);
  Nullable<Vector<std::pair<String, int32_t>>> getNullableStringLongRecord();

  Vector<std::pair<String, String>> GetByteStringByteStringRecord();
  void setByteStringByteStringRecord(
      const Vector<std::pair<String, String>>& arg);

  void setStringElementRecord(
      const HeapVector<std::pair<String, Member<Element>>>& arg);
  HeapVector<std::pair<String, Member<Element>>> getStringElementRecord();

  using NestedRecordType =
      Vector<std::pair<String, Vector<std::pair<String, bool>>>>;
  void setUSVStringUSVStringBooleanRecordRecord(const NestedRecordType& arg);
  NestedRecordType getUSVStringUSVStringBooleanRecordRecord();

  Vector<std::pair<String, Vector<String>>>
  returnStringByteStringSequenceRecord();

  bool unionReceivedARecord(const BooleanOrByteStringByteStringRecord& arg);

  void setFloatOrStringElementRecord(const FloatOrStringElementRecord&){};

  void Trace(blink::Visitor*);

 private:
  RecordTest();

  Vector<std::pair<String, int32_t>> string_long_record_;
  Nullable<Vector<std::pair<String, int32_t>>> nullable_string_long_record_;
  Vector<std::pair<String, String>> byte_string_byte_string_record_;
  HeapVector<std::pair<String, Member<Element>>> string_element_record_;
  NestedRecordType usv_string_usv_string_boolean_record_record_;
};

}  // namespace blink

#endif  // RecordTest_h
