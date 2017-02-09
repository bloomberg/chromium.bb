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
  visitor->trace(m_array);
}

bool IDBKey::isValid() const {
  if (m_type == InvalidType)
    return false;

  if (m_type == ArrayType) {
    for (size_t i = 0; i < m_array.size(); i++) {
      if (!m_array[i]->isValid())
        return false;
    }
  }

  return true;
}

// Safely compare numbers (signed/unsigned ints/floats/doubles).
template <typename T>
static int compareNumbers(const T& a, const T& b) {
  if (a < b)
    return -1;
  if (b < a)
    return 1;
  return 0;
}

int IDBKey::compare(const IDBKey* other) const {
  ASSERT(other);
  if (m_type != other->m_type)
    return m_type > other->m_type ? -1 : 1;

  switch (m_type) {
    case ArrayType:
      for (size_t i = 0; i < m_array.size() && i < other->m_array.size(); ++i) {
        if (int result = m_array[i]->compare(other->m_array[i].get()))
          return result;
      }
      return compareNumbers(m_array.size(), other->m_array.size());
    case BinaryType:
      if (int result =
              memcmp(m_binary->data(), other->m_binary->data(),
                     std::min(m_binary->size(), other->m_binary->size())))
        return result < 0 ? -1 : 1;
      return compareNumbers(m_binary->size(), other->m_binary->size());
    case StringType:
      return codePointCompare(m_string, other->m_string);
    case DateType:
    case NumberType:
      return compareNumbers(m_number, other->m_number);
    case InvalidType:
    case TypeEnumMax:
      ASSERT_NOT_REACHED();
      return 0;
  }

  ASSERT_NOT_REACHED();
  return 0;
}

bool IDBKey::isLessThan(const IDBKey* other) const {
  ASSERT(other);
  return compare(other) == -1;
}

bool IDBKey::isEqual(const IDBKey* other) const {
  if (!other)
    return false;

  return !compare(other);
}

IDBKey::KeyArray IDBKey::toMultiEntryArray() const {
  DCHECK_EQ(m_type, ArrayType);
  KeyArray result;
  result.reserveCapacity(m_array.size());
  std::copy_if(m_array.begin(), m_array.end(), std::back_inserter(result),
               [](const Member<IDBKey> key) { return key->isValid(); });

  // Remove duplicates using std::sort/std::unique rather than a hashtable to
  // avoid the complexity of implementing DefaultHash<IDBKey>.
  std::sort(result.begin(), result.end(),
            [](const Member<IDBKey> a, const Member<IDBKey> b) {
              return a->isLessThan(b);
            });
  const auto end = std::unique(result.begin(), result.end());
  DCHECK_LE(static_cast<size_t>(end - result.begin()), result.size());
  result.resize(end - result.begin());
  return result;
}

}  // namespace blink
