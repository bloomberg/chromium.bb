// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContentSecurityPolicyStructTraits_h
#define ContentSecurityPolicyStructTraits_h

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/WebKit/public/platform/WebContentSecurityPolicy.h"
#include "third_party/WebKit/public/platform/content_security_policy.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<::blink::mojom::ContentSecurityPolicyType,
                  ::blink::WebContentSecurityPolicyType> {
  static ::blink::mojom::ContentSecurityPolicyType ToMojom(
      ::blink::WebContentSecurityPolicyType input) {
    switch (input) {
      case ::blink::kWebContentSecurityPolicyTypeReport:
        return ::blink::mojom::ContentSecurityPolicyType::kReport;
      case ::blink::kWebContentSecurityPolicyTypeEnforce:
        return ::blink::mojom::ContentSecurityPolicyType::kEnforce;
    }
    NOTREACHED();
    return ::blink::mojom::ContentSecurityPolicyType::kReport;
  }

  static bool FromMojom(::blink::mojom::ContentSecurityPolicyType input,
                        ::blink::WebContentSecurityPolicyType* output) {
    switch (input) {
      case ::blink::mojom::ContentSecurityPolicyType::kReport:
        *output = ::blink::kWebContentSecurityPolicyTypeReport;
        return true;
      case ::blink::mojom::ContentSecurityPolicyType::kEnforce:
        *output = ::blink::kWebContentSecurityPolicyTypeEnforce;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // ContentSecurityPolicyStructTraits_h
