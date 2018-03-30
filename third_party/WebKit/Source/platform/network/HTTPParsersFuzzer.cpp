// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/HTTPParsers.h"

#include <string>

#include "base/logging.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "platform/wtf/text/AtomicString.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support;

  blink::CommaDelimitedHeaderSet set;
  double delay;
  String url;
  blink::ResourceResponse response;
  size_t end;
  String report_url;
  String failure_reason;
  unsigned failure_position = 0;

  std::string terminated(reinterpret_cast<const char*>(data), size);
  blink::IsValidHTTPToken(terminated.c_str());
  blink::ParseCacheControlDirectives(terminated.c_str(), AtomicString());
  blink::ParseCommaDelimitedHeader(terminated.c_str(), set);
  blink::ParseHTTPRefresh(terminated.c_str(), nullptr, delay, url);
  blink::ParseMultipartHeadersFromBody(terminated.c_str(), terminated.size(),
                                       &response, &end);
  blink::ParseServerTimingHeader(terminated.c_str());
  blink::ParseContentTypeOptionsHeader(terminated.c_str());
  blink::ParseXSSProtectionHeader(terminated.c_str(), failure_reason,
                                  failure_position, report_url);
  return 0;
}
