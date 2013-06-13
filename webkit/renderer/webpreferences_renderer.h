// // Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_WEBPREFERENCES_RENDERER_H_
#define WEBKIT_RENDERER_WEBPREFERENCES_RENDERER_H_

#include "webkit/renderer/webkit_renderer_export.h"

namespace WebKit {
class WebView;
}

struct WebPreferences;

namespace webkit_glue {

WEBKIT_RENDERER_EXPORT void ApplyWebPreferences(
    const WebPreferences& prefs,
    WebKit::WebView* web_view);

}  // namespace webkit_glue

#endif  // WEBKIT_RENDERER_WEBPREFERENCES_RENDERER_H_
