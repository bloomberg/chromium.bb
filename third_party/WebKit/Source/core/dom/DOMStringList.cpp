/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/DOMStringList.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/frame/UseCounter.h"
#include <algorithm>

namespace blink {

String DOMStringList::anonymousIndexedGetter(unsigned index) const {
  if (index >= m_strings.size())
    return String();
  return m_strings[index];
}

String DOMStringList::item(ScriptState* scriptState, unsigned index) const {
  switch (m_source) {
    case DOMStringList::IndexedDB:
      UseCounter::count(
          scriptState->getExecutionContext(),
          UseCounter::DOMStringList_Item_AttributeGetter_IndexedDB);
      break;
    case DOMStringList::Location:
      UseCounter::count(
          scriptState->getExecutionContext(),
          UseCounter::DOMStringList_Item_AttributeGetter_Location);
      break;
    default:
      NOTREACHED();
  }

  return anonymousIndexedGetter(index);
}

bool DOMStringList::contains(ScriptState* scriptState,
                             const String& string) const {
  switch (m_source) {
    case DOMStringList::IndexedDB:
      UseCounter::count(scriptState->getExecutionContext(),
                        UseCounter::DOMStringList_Contains_Method_IndexedDB);
      break;
    case DOMStringList::Location:
      UseCounter::count(scriptState->getExecutionContext(),
                        UseCounter::DOMStringList_Contains_Method_Location);
      break;
    default:
      NOTREACHED();
  }

  // FIXME: Currently, all consumers of DOMStringList store fairly small lists
  // and thus an O(n) algorithm is OK.  But this may need to be optimized if
  // larger amounts of data are stored in m_strings.
  for (const auto& item : m_strings) {
    if (item == string)
      return true;
  }
  return false;
}

void DOMStringList::sort() {
  std::sort(m_strings.begin(), m_strings.end(), WTF::codePointCompareLessThan);
}

}  // namespace blink
