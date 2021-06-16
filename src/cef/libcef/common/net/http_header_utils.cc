// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/net/http_header_utils.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

using net::HttpResponseHeaders;

namespace HttpHeaderUtils {

std::string GenerateHeaders(const HeaderMap& map) {
  std::string headers;

  for (HeaderMap::const_iterator header = map.begin(); header != map.end();
       ++header) {
    const CefString& key = header->first;
    const CefString& value = header->second;

    if (!key.empty()) {
      // Delimit with "\r\n".
      if (!headers.empty())
        headers += "\r\n";

      headers += std::string(key) + ": " + std::string(value);
    }
  }

  return headers;
}

void ParseHeaders(const std::string& header_str, HeaderMap& map) {
  // Parse the request header values
  for (net::HttpUtil::HeadersIterator i(header_str.begin(), header_str.end(),
                                        "\n\r");
       i.GetNext();) {
    map.insert(std::make_pair(i.name(), i.values()));
  }
}

void MakeASCIILower(std::string* str) {
  std::transform(str->begin(), str->end(), str->begin(), ::tolower);
}

HeaderMap::iterator FindHeaderInMap(const std::string& nameLower,
                                    HeaderMap& map) {
  for (auto it = map.begin(); it != map.end(); ++it) {
    if (base::EqualsCaseInsensitiveASCII(it->first.ToString(), nameLower))
      return it;
  }

  return map.end();
}

}  // namespace HttpHeaderUtils
