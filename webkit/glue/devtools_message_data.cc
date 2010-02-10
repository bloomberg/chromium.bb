// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/devtools_message_data.h"

#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsMessageData.h"

using WebKit::WebDevToolsMessageData;
using WebKit::WebString;
using WebKit::WebVector;

DevToolsMessageData::DevToolsMessageData(const WebDevToolsMessageData& data)
    : class_name(data.className.utf8()),
      method_name(data.methodName.utf8()) {
  for (size_t i = 0; i < data.arguments.size(); i++)
    arguments.push_back(data.arguments[i].utf8());
}

WebDevToolsMessageData DevToolsMessageData::ToWebDevToolsMessageData() const {
  WebDevToolsMessageData result;
  result.className = WebString::fromUTF8(class_name);
  result.methodName = WebString::fromUTF8(method_name);
  WebVector<WebString> web_args(arguments.size());
  for (size_t i = 0; i < arguments.size(); i++)
    web_args[i] = WebString::fromUTF8(arguments[i]);
  result.arguments.swap(web_args);
  return result;
}
