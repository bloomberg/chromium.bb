/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/indexeddb/IDBKey.h"

#include <algorithm>

namespace blink {

IDBKey::~IDBKey() {}

DEFINE_TRACE(IDBKey) {
  visitor->Trace(array_);
}

bool IDBKey::IsValid() const {
  if (type_ == kInvalidType)
    return false;

  if (type_ == kArrayType) {
    for (size_t i = 0; i < array_.size(); i++) {
      if (!array_[i]->IsValid())
        return false;
    }
  }

  return true;
}

// Safely compare numbers (signed/unsigned ints/floats/doubles).
template <typename T>
static int CompareNumbers(const T& a, const T& b) {
  if (a < b)
    return -1;
  if (b < a)
    return 1;
  return 0;
}

int IDBKey::Compare(const IDBKey* other) const {
  DCHECK(other);
  if (type_ != other->type_)
    return type_ > other->type_ ? -1 : 1;

  switch (type_) {
    case kArrayType:
      for (size_t i = 0; i < array_.size() && i < other->array_.size(); ++i) {
        if (int result = array_[i]->Compare(other->array_[i].Get()))
          return result;
      }
      return CompareNumbers(array_.size(), other->array_.size());
    case kBinaryType:
      if (int result =
              memcmp(binary_->Data(), other->binary_->Data(),
                     std::min(binary_->size(), other->binary_->size())))
        return result < 0 ? -1 : 1;
      return CompareNumbers(binary_->size(), other->binary_->size());
    case kStringType:
      return CodePointCompare(string_, other->string_);
    case kDateType:
    case kNumberType:
      return CompareNumbers(number_, other->number_);
    case kInvalidType:
    case kTypeEnumMax:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

bool IDBKey::IsLessThan(const IDBKey* other) const {
  DCHECK(other);
  return Compare(other) == -1;
}

bool IDBKey::IsEqual(const IDBKey* other) const {
  if (!other)
    return false;

  return !Compare(other);
}

IDBKey::KeyArray IDBKey::ToMultiEntryArray() const {
  DCHECK_EQ(type_, kArrayType);
  KeyArray result;
  result.ReserveCapacity(array_.size());
  std::copy_if(array_.begin(), array_.end(), std::back_inserter(result),
               [](const Member<IDBKey> key) { return key->IsValid(); });

  // Remove duplicates using std::sort/std::unique rather than a hashtable to
  // avoid the complexity of implementing DefaultHash<IDBKey>.
  std::sort(result.begin(), result.end(),
            [](const Member<IDBKey> a, const Member<IDBKey> b) {
              return a->IsLessThan(b);
            });
  const auto end = std::unique(result.begin(), result.end());
  DCHECK_LE(static_cast<size_t>(end - result.begin()), result.size());
  result.resize(end - result.begin());
  return result;
}

}  // namespace blink
