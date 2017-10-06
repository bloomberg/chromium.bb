// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AddressSpaceStructTraits_h
#define AddressSpaceStructTraits_h

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/WebKit/public/platform/WebAddressSpace.h"
#include "third_party/WebKit/public/platform/address_space.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<::blink::mojom::AddressSpace, ::blink::WebAddressSpace> {
  static ::blink::mojom::AddressSpace ToMojom(::blink::WebAddressSpace input) {
    switch (input) {
      case ::blink::kWebAddressSpaceLocal:
        return ::blink::mojom::AddressSpace::kLocal;
      case ::blink::kWebAddressSpacePrivate:
        return ::blink::mojom::AddressSpace::kPrivate;
      case ::blink::kWebAddressSpacePublic:
        return ::blink::mojom::AddressSpace::kPublic;
    }
    NOTREACHED();
    return ::blink::mojom::AddressSpace::kLocal;
  }

  static bool FromMojom(::blink::mojom::AddressSpace input,
                        ::blink::WebAddressSpace* output) {
    switch (input) {
      case ::blink::mojom::AddressSpace::kLocal:
        *output = ::blink::kWebAddressSpaceLocal;
        return true;
      case ::blink::mojom::AddressSpace::kPrivate:
        *output = ::blink::kWebAddressSpacePrivate;
        return true;
      case ::blink::mojom::AddressSpace::kPublic:
        *output = ::blink::kWebAddressSpacePublic;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // AddressSpaceStructTraits_h
