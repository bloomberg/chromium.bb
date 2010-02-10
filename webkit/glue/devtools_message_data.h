// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_MESSAGE_DATA_H_
#define WEBKIT_GLUE_DEVTOOLS_MESSAGE_DATA_H_

#include <string>
#include <vector>

namespace WebKit {
struct WebDevToolsMessageData;
}

struct DevToolsMessageData {
  DevToolsMessageData() {}
  explicit DevToolsMessageData(const WebKit::WebDevToolsMessageData&);
  WebKit::WebDevToolsMessageData ToWebDevToolsMessageData() const;

  std::string class_name;
  std::string method_name;
  std::vector<std::string> arguments;
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_MESSAGE_DATA_H_
