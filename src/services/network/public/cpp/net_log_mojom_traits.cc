// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/net_log_mojom_traits.h"

namespace mojo {

// static
bool EnumTraits<network::mojom::NetLogCaptureMode, net::NetLogCaptureMode>::
    FromMojom(network::mojom::NetLogCaptureMode capture_mode,
              net::NetLogCaptureMode* out) {
  switch (capture_mode) {
    case network::mojom::NetLogCaptureMode::DEFAULT:
      *out = net::NetLogCaptureMode::Default();
      return true;
    case network::mojom::NetLogCaptureMode::INCLUDE_COOKIES_AND_CREDENTIALS:
      *out = net::NetLogCaptureMode::IncludeCookiesAndCredentials();
      return true;
    case network::mojom::NetLogCaptureMode::INCLUDE_SOCKET_BYTES:
      *out = net::NetLogCaptureMode::IncludeSocketBytes();
      return true;
  }
  return false;
}

// static
network::mojom::NetLogCaptureMode
EnumTraits<network::mojom::NetLogCaptureMode, net::NetLogCaptureMode>::ToMojom(
    net::NetLogCaptureMode capture_mode) {
  if (capture_mode.include_cookies_and_credentials())
    return network::mojom::NetLogCaptureMode::INCLUDE_COOKIES_AND_CREDENTIALS;
  if (capture_mode.include_socket_bytes())
    return network::mojom::NetLogCaptureMode::INCLUDE_SOCKET_BYTES;

  // TODO(eroman): Should fail if unrecognized mode rather than defaulting.
  return network::mojom::NetLogCaptureMode::DEFAULT;
}

}  // namespace mojo
