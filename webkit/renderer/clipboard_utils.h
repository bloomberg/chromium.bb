// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_CLIPBOARD_UTILS_H_
#define WEBKIT_RENDERER_CLIPBOARD_UTILS_H_

#include <string>

#include "webkit/renderer/webkit_renderer_export.h"

namespace WebKit {
class WebString;
class WebURL;
}

namespace webkit_clipboard {

WEBKIT_RENDERER_EXPORT std::string URLToMarkup(const WebKit::WebURL& url,
                                               const WebKit::WebString& title);

WEBKIT_RENDERER_EXPORT std::string URLToImageMarkup(
    const WebKit::WebURL& url,
    const WebKit::WebString& title);

}  // namespace webkit_clipboard

#endif  //  WEBKIT_RENDERER_CLIPBOARD_UTILS_H_
