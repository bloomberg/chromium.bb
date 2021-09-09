// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sequence_test.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_doublesequence.h"

namespace blink {

SequenceTest::SequenceTest() = default;

SequenceTest::~SequenceTest() = default;

Vector<Vector<String>> SequenceTest::identityByteStringSequenceSequence(
    const Vector<Vector<String>>& arg) const {
  return arg;
}

Vector<double> SequenceTest::identityDoubleSequence(
    const Vector<double>& arg) const {
  return arg;
}

Vector<V8FoodEnum> SequenceTest::identityFoodEnumSequence(
    const Vector<V8FoodEnum>& arg) const {
  return arg;
}

Vector<int32_t> SequenceTest::identityLongSequence(
    const Vector<int32_t>& arg) const {
  return arg;
}

absl::optional<Vector<uint8_t>> SequenceTest::identityOctetSequenceOrNull(
    const absl::optional<Vector<uint8_t>>& arg) const {
  return arg;
}

HeapVector<Member<Element>> SequenceTest::getElementSequence() const {
  return element_sequence_;
}

void SequenceTest::setElementSequence(const HeapVector<Member<Element>>& arg) {
  element_sequence_ = arg;
}

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
bool SequenceTest::unionReceivedSequence(
    const V8UnionDoubleOrDoubleSequence* arg) {
  return arg->IsDoubleSequence();
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
bool SequenceTest::unionReceivedSequence(const DoubleOrDoubleSequence& arg) {
  return arg.IsDoubleSequence();
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

void SequenceTest::Trace(Visitor* visitor) const {
  visitor->Trace(element_sequence_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
