// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/prefetched_signed_exchange_info.h"

namespace content {

PrefetchedSignedExchangeInfo::PrefetchedSignedExchangeInfo() = default;
PrefetchedSignedExchangeInfo::PrefetchedSignedExchangeInfo(
    const PrefetchedSignedExchangeInfo&) = default;
PrefetchedSignedExchangeInfo::PrefetchedSignedExchangeInfo(
    const GURL& outer_url,
    const net::SHA256HashValue& header_integrity,
    const GURL& inner_url,
    const network::ResourceResponseHead& inner_response,
    mojo::MessagePipeHandle loader_factory_handle)
    : outer_url(outer_url),
      header_integrity(header_integrity),
      inner_url(inner_url),
      inner_response(inner_response),
      loader_factory_handle(loader_factory_handle) {}
PrefetchedSignedExchangeInfo::~PrefetchedSignedExchangeInfo() = default;

}  // namespace content
