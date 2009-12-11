// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#undef LOG

#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsMessageData.h"
#include "webkit/glue/devtools/devtools_message_data.h"
#include "webkit/glue/glue_util.h"

DevToolsMessageData::DevToolsMessageData(
    const WebKit::WebDevToolsMessageData& data)
    : class_name(webkit_glue::WebStringToStdString(data.className)),
      method_name(webkit_glue::WebStringToStdString(data.methodName)) {
  for (size_t i = 0; i < data.arguments.size(); i++)
    arguments.push_back(webkit_glue::WebStringToStdString(data.arguments[i]));
}

WebKit::WebDevToolsMessageData
    DevToolsMessageData::ToWebDevToolsMessageData() const {
  WebKit::WebDevToolsMessageData result;
  result.className = webkit_glue::StdStringToWebString(class_name);
  result.methodName = webkit_glue::StdStringToWebString(method_name);
  WebKit::WebVector<WebKit::WebString> web_args(arguments.size());
  for (size_t i = 0; i < arguments.size(); i++)
    web_args[i] = webkit_glue::StdStringToWebString(arguments[i]);
  result.arguments.swap(web_args);
  return result;
}