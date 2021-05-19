// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_HTTP2_PLATFORM_API_HTTP2_STRING_UTILS_H_
#define QUICHE_HTTP2_PLATFORM_API_HTTP2_STRING_UTILS_H_

#include <string>
#include <type_traits>
#include <utility>

#include "absl/strings/string_view.h"
#include "net/http2/platform/impl/http2_string_utils_impl.h"

namespace http2 {

template <typename... Args>
inline std::string Http2StringPrintf(const Args&... args) {
  return Http2StringPrintfImpl(std::forward<const Args&>(args)...);
}

inline std::string Http2HexEncode(const void* bytes, size_t size) {
  return Http2HexEncodeImpl(bytes, size);
}

inline std::string Http2HexDecode(absl::string_view data) {
  return Http2HexDecodeImpl(data);
}

inline std::string Http2HexDump(absl::string_view data) {
  return Http2HexDumpImpl(data);
}

}  // namespace http2

#endif  // QUICHE_HTTP2_PLATFORM_API_HTTP2_STRING_UTILS_H_
