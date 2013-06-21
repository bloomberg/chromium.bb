// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "webkit/renderer/clipboard_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "net/base/escape.h"

namespace webkit_clipboard {

// Static
std::string URLToMarkup(const WebURL& url, const WebString& title) {
  std::string markup("<a href=\"");
  markup.append(url.spec());
  markup.append("\">");
  // TODO(darin): HTML escape this
  markup.append(net::EscapeForHTML(UTF16ToUTF8(title)));
  markup.append("</a>");
  return markup;
}

std::string URLToImageMarkup(const WebURL& url, const WebString& title) {
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

}  // webkit_clipboard

