// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImmutableCSSPropertyValueSet_h
#define ImmutableCSSPropertyValueSet_h

#include "core/css/CSSPropertyValueSet.h"

namespace blink {

class CORE_EXPORT ImmutableCSSPropertyValueSet : public CSSPropertyValueSet {
 public:
  static size_t SizeWithPropertyCount(unsigned count);
  static ImmutableCSSPropertyValueSet*
  Create(const CSSPropertyValue* properties, unsigned count, CSSParserMode);

  unsigned PropertyCount() const { return array_size_; }

  const Member<const CSSValue>* ValueArray() const;
  const CSSPropertyValueMetadata* MetadataArray() const;

  template <typename T>  // CSSPropertyID or AtomicString
  int FindPropertyIndex(T property) const;

  void TraceAfterDispatch(blink::Visitor*);

  void* operator new(std::size_t, void* location) { return location; }

  void* storage_;

 private:
  ImmutableCSSPropertyValueSet(const CSSPropertyValue*,
                               unsigned count,
                               CSSParserMode);
};

inline const Member<const CSSValue>* ImmutableCSSPropertyValueSet::ValueArray()
    const {
  return reinterpret_cast<const Member<const CSSValue>*>(
      const_cast<const void**>(&(this->storage_)));
}

inline const CSSPropertyValueMetadata*
ImmutableCSSPropertyValueSet::MetadataArray() const {
  return reinterpret_cast<const CSSPropertyValueMetadata*>(
      &reinterpret_cast<const char*>(
          &(this->storage_))[array_size_ * sizeof(Member<CSSValue>)]);
}

DEFINE_TYPE_CASTS(ImmutableCSSPropertyValueSet,
                  CSSPropertyValueSet,
                  set,
                  !set->IsMutable(),
                  !set.IsMutable());

}  // namespace blink

#endif  // ImmutableCSSPropertyValueSet_h
