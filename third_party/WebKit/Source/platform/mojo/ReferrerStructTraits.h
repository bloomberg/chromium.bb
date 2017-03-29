// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReferrerStructTraits_h
#define ReferrerStructTraits_h

#include "platform/weborigin/Referrer.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "public/platform/ReferrerPolicyEnumTraits.h"
#include "public/platform/WebReferrerPolicy.h"
#include "wtf/Assertions.h"

namespace mojo {

template <>
struct StructTraits<blink::mojom::ReferrerDataView, blink::Referrer> {
  static blink::KURL url(const blink::Referrer& referrer) {
    if (referrer.referrer == blink::Referrer::noReferrer())
      return blink::KURL();

    return blink::KURL(blink::KURL(), referrer.referrer);
  }

  // Equality of values is asserted in //Source/web/AssertMatchingEnums.cpp.
  static blink::WebReferrerPolicy policy(const blink::Referrer& referrer) {
    return static_cast<blink::WebReferrerPolicy>(referrer.referrerPolicy);
  }

  static bool Read(blink::mojom::ReferrerDataView data, blink::Referrer* out) {
    blink::KURL referrer;
    blink::WebReferrerPolicy webReferrerPolicy;
    if (!data.ReadUrl(&referrer) || !data.ReadPolicy(&webReferrerPolicy))
      return false;

    out->referrerPolicy = static_cast<blink::ReferrerPolicy>(webReferrerPolicy);
    out->referrer = AtomicString(referrer.getString());

    // Mimics the ASSERT() done in the blink::Referrer constructor.
    return referrer.isValid() || out->referrer == blink::Referrer::noReferrer();
  }
};

}  // namespace mojo

#endif  // ReferrerStructTraits_h
