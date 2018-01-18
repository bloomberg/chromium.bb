// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_URL_REQUEST_STRUCT_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_URL_REQUEST_STRUCT_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/base/request_priority.h"
#include "services/network/public/interfaces/url_loader.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<network::mojom::RequestPriority, net::RequestPriority> {
  static network::mojom::RequestPriority ToMojom(net::RequestPriority priority);
  static bool FromMojom(network::mojom::RequestPriority in,
                        net::RequestPriority* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_URL_REQUEST_STRUCT_TRAITS_H_
