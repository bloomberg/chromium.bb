// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/ImmutableCSSPropertyValueSet.h"

#include "core/css/MutableCSSPropertyValueSet.h"
#include "core/css/parser/AtRuleDescriptors.h"

namespace blink {

size_t ImmutableCSSPropertyValueSet::SizeWithPropertyCount(unsigned count) {
  return sizeof(ImmutableCSSPropertyValueSet) - sizeof(void*) +
         sizeof(Member<CSSValue>) * count +
         sizeof(CSSPropertyValueMetadata) * count;
}

ImmutableCSSPropertyValueSet* ImmutableCSSPropertyValueSet::Create(
    const CSSPropertyValue* properties,
    unsigned count,
    CSSParserMode css_parser_mode) {
  DCHECK_LE(count, static_cast<unsigned>(kMaxArraySize));
  void* slot =
      ThreadHeap::Allocate<CSSPropertyValueSet>(SizeWithPropertyCount(count));
  return new (slot)
      ImmutableCSSPropertyValueSet(properties, count, css_parser_mode);
}

ImmutableCSSPropertyValueSet* CSSPropertyValueSet::ImmutableCopyIfNeeded()
    const {
  if (!IsMutable()) {
    return ToImmutableCSSPropertyValueSet(
        const_cast<CSSPropertyValueSet*>(this));
  }
  const MutableCSSPropertyValueSet* mutable_this =
      ToMutableCSSPropertyValueSet(this);
  return ImmutableCSSPropertyValueSet::Create(
      mutable_this->property_vector_.data(),
      mutable_this->property_vector_.size(), CssParserMode());
}

ImmutableCSSPropertyValueSet::ImmutableCSSPropertyValueSet(
    const CSSPropertyValue* properties,
    unsigned length,
    CSSParserMode css_parser_mode)
    : CSSPropertyValueSet(css_parser_mode, length) {
  CSSPropertyValueMetadata* metadata_array =
      const_cast<CSSPropertyValueMetadata*>(this->MetadataArray());
  Member<const CSSValue>* value_array =
      const_cast<Member<const CSSValue>*>(this->ValueArray());
  for (unsigned i = 0; i < array_size_; ++i) {
    metadata_array[i] = properties[i].Metadata();
    value_array[i] = properties[i].Value();
  }
}

template <typename T>
int ImmutableCSSPropertyValueSet::FindPropertyIndex(T property) const {
  uint16_t id = GetConvertedCSSPropertyID(property);
  for (int n = array_size_ - 1; n >= 0; --n) {
    if (IsPropertyMatch(MetadataArray()[n], *ValueArray()[n], id, property))
      return n;
  }

  return -1;
}
template CORE_EXPORT int ImmutableCSSPropertyValueSet::FindPropertyIndex(
    CSSPropertyID) const;
template CORE_EXPORT int ImmutableCSSPropertyValueSet::FindPropertyIndex(
    AtomicString) const;
template CORE_EXPORT int ImmutableCSSPropertyValueSet::FindPropertyIndex(
    AtRuleDescriptorID) const;

void ImmutableCSSPropertyValueSet::TraceAfterDispatch(blink::Visitor* visitor) {
  const Member<const CSSValue>* values = ValueArray();
  for (unsigned i = 0; i < array_size_; i++)
    visitor->Trace(values[i]);
  CSSPropertyValueSet::TraceAfterDispatch(visitor);
}

}  // namespace blink
