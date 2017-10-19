// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SequenceTest_h
#define SequenceTest_h

#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/double_or_double_sequence.h"
#include "core/dom/Element.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class SequenceTest final : public GarbageCollectedFinalized<SequenceTest>,
                           public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SequenceTest* create() { return new SequenceTest; }
  ~SequenceTest();

  Vector<Vector<String>> identityByteStringSequenceSequence(
      const Vector<Vector<String>>& arg) const;
  Vector<double> identityDoubleSequence(const Vector<double>& arg) const;
  Vector<String> identityFoodEnumSequence(const Vector<String>& arg) const;
  Vector<int32_t> identityLongSequence(const Vector<int32_t>& arg) const;
  Nullable<Vector<uint8_t>> identityOctetSequenceOrNull(
      const Nullable<Vector<uint8_t>>& arg) const;

  HeapVector<Member<Element>> getElementSequence() const;
  void setElementSequence(const HeapVector<Member<Element>>& arg);

  bool unionReceivedSequence(const DoubleOrDoubleSequence& arg);

  void Trace(blink::Visitor*);

 private:
  SequenceTest();

  HeapVector<Member<Element>> element_sequence_;
};

}  // namespace blink

#endif  // SequenceTest_h
