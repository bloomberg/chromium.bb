// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "third_party/blink/renderer/platform/network/http_request_headers_mojom_traits.h"

namespace mojo {

// static
WTF::Vector<network::mojom::blink::HttpRequestHeaderKeyValuePair>
StructTraits<network::mojom::HttpRequestHeadersDataView,
             blink::HTTPHeaderMap>::headers(const blink::HTTPHeaderMap& map) {
  std::unique_ptr<blink::CrossThreadHTTPHeaderMapData> headers = map.CopyData();
  WTF::Vector<network::mojom::blink::HttpRequestHeaderKeyValuePair> headers_out;
  for (const auto& header : *headers) {
    headers_out.emplace_back(header.first, header.second);
  }
  return headers_out;
}

// static
bool StructTraits<
    network::mojom::HttpRequestHeadersDataView,
    blink::HTTPHeaderMap>::Read(network::mojom::HttpRequestHeadersDataView data,
                                blink::HTTPHeaderMap* out) {
  WTF::Vector<network::mojom::blink::HttpRequestHeaderKeyValuePairPtr> headers;
  if (!data.ReadHeaders(&headers)) {
    return false;
  }
  out->Clear();
  for (const auto& header : headers) {
    out->Set(AtomicString(header->key), AtomicString(header->value));
  }
  return true;
}

}  // namespace mojo
