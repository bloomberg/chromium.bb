// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_CURSOR_UTILS_H_
#define WEBKIT_RENDERER_CURSOR_UTILS_H_

#include "webkit/common/cursors/webcursor.h"
#include "webkit/renderer/webkit_renderer_export.h"

namespace WebKit {
struct WebCursorInfo;
};

namespace webkit_glue {

// Adapts our cursor info to WebKit::WebCursorInfo.
WEBKIT_RENDERER_EXPORT bool GetWebKitCursorInfo(
    const WebCursor& cursor,
    WebKit::WebCursorInfo* webkit_cursor_info);

// Adapts WebKit::CursorInfo to our cursor.
WEBKIT_RENDERER_EXPORT void InitializeCursorFromWebKitCursorInfo(
    WebCursor* cursor,
    const WebKit::WebCursorInfo& webkit_cursor_info);

}  // namespace webkit_glue

#endif  // WEBKIT_RENDERER_CURSOR_UTILS_H_
