// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/clipboard_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "net/base/escape.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace webkit_clipboard {

std::string URLToMarkup(const WebKit::WebURL& url,
                        const WebKit::WebString& title) {
  std::string markup("<a href=\"");
  markup.append(url.spec());
  markup.append("\">");
  // TODO(darin): HTML escape this
  markup.append(net::EscapeForHTML(UTF16ToUTF8(title)));
  markup.append("</a>");
  return markup;
}

std::string URLToImageMarkup(const WebKit::WebURL& url,
                             const WebKit::WebString& title) {
  std::string markup("<img src=\"");
  markup.append(url.spec());
  markup.append("\"");
  if (!title.isEmpty()) {
    markup.append(" alt=\"");
    markup.append(net::EscapeForHTML(UTF16ToUTF8(title)));
    markup.append("\"");
  }
  markup.append("/>");
  return markup;
}

}  // namespace webkit_clipboard
