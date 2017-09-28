/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#ifndef SecurityOriginHash_h
#define SecurityOriginHash_h

#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

struct SecurityOriginHash {
  STATIC_ONLY(SecurityOriginHash);
  static unsigned GetHash(SecurityOrigin* origin) {
    unsigned hash_codes[4] = {
        origin->Protocol().Impl() ? origin->Protocol().Impl()->GetHash() : 0,
        origin->Host().Impl() ? origin->Host().Impl()->GetHash() : 0,
        origin->Port(),
        (origin->GetSuborigin()->GetName().Impl())
            ? origin->GetSuborigin()->GetName().Impl()->GetHash()
            : 0};
    return StringHasher::HashMemory<sizeof(hash_codes)>(hash_codes);
  }
  static unsigned GetHash(const RefPtr<SecurityOrigin>& origin) {
    return GetHash(origin.get());
  }

  static bool Equal(SecurityOrigin* a, SecurityOrigin* b) {
    if (!a || !b)
      return a == b;

    if (a == b)
      return true;

    if (!a->IsSameSchemeHostPortAndSuborigin(b))
      return false;

    if (a->DomainWasSetInDOM() != b->DomainWasSetInDOM())
      return false;

    if (a->DomainWasSetInDOM() && a->Domain() != b->Domain())
      return false;

    return true;
  }
  static bool Equal(SecurityOrigin* a, const RefPtr<SecurityOrigin>& b) {
    return Equal(a, b.get());
  }
  static bool Equal(const RefPtr<SecurityOrigin>& a, SecurityOrigin* b) {
    return Equal(a.get(), b);
  }
  static bool Equal(const RefPtr<SecurityOrigin>& a,
                    const RefPtr<SecurityOrigin>& b) {
    return Equal(a.get(), b.get());
  }

  static const bool safe_to_compare_to_empty_or_deleted = false;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<RefPtr<blink::SecurityOrigin>> {
  typedef blink::SecurityOriginHash Hash;
};

}  // namespace WTF

#endif  // SecurityOriginHash_h
