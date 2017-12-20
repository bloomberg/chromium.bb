// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_ERROR_STATUS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_ERROR_STATUS_H_

#include "base/memory/scoped_refptr.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/interfaces/cors.mojom-shared.h"

namespace network {

struct CORSErrorStatus {
  // This constructor is used by generated IPC serialization code.
  // Should not use this explicitly.
  // TODO(toyoshim, yhirano): Exploring a way to make this private, and allows
  // only serialization code for mojo can access.
  CORSErrorStatus();

  CORSErrorStatus(const CORSErrorStatus& status);

  explicit CORSErrorStatus(network::mojom::CORSError error);
  CORSErrorStatus(
      network::mojom::CORSError error,
      scoped_refptr<net::HttpResponseHeaders> related_response_headers);

  ~CORSErrorStatus();

  bool operator==(const CORSErrorStatus& rhs) const;
  bool operator!=(const CORSErrorStatus& rhs) const { return !(*this == rhs); }

  network::mojom::CORSError cors_error;

  // Partial HTTP response headers including status line that will be useful to
  // generate a human readable error message.
  scoped_refptr<net::HttpResponseHeaders> related_response_headers;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_ERROR_STATUS_H_
