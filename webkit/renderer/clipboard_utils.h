// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef WEBKIT_RENDERER_CLIPBOARD_UTILS_H_
#define WEBKIT_RENDERER_CLIPBOARD_UTILS_H_

#include <string>
#include "third_party/WebKit/public/platform/WebClipboard.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "webkit/renderer/webkit_renderer_export.h"

using WebKit::WebString;
using WebKit::WebURL;

namespace webkit_clipboard {

WEBKIT_RENDERER_EXPORT std::string URLToMarkup(const WebURL& url,
                                               const WebString& title);
WEBKIT_RENDERER_EXPORT std::string URLToImageMarkup(const WebURL& url,
                                                    const WebString& title);
}  // webkit_clipboard

#endif  //  WEBKIT_RENDERER_CLIPBOARD_UTILS_H_
