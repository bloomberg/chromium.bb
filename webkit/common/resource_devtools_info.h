// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_RESOURCE_DEVTOOLS_INFO_H_
#define WEBKIT_COMMON_RESOURCE_DEVTOOLS_INFO_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "webkit/common/webkit_common_export.h"

namespace webkit_glue {

struct ResourceDevToolsInfo : base::RefCounted<ResourceDevToolsInfo> {
  typedef std::vector<std::pair<std::string, std::string> >
      HeadersVector;

  WEBKIT_COMMON_EXPORT ResourceDevToolsInfo();

  int32 http_status_code;
  std::string http_status_text;
  HeadersVector request_headers;
  HeadersVector response_headers;
  std::string request_headers_text;
  std::string response_headers_text;

 private:
  friend class base::RefCounted<ResourceDevToolsInfo>;
  WEBKIT_COMMON_EXPORT ~ResourceDevToolsInfo();
};

}  // namespace webkit_glue

#endif  // WEBKIT_COMMON_RESOURCE_DEVTOOLS_INFO_H_
