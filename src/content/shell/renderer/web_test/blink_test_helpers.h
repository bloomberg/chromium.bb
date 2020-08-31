// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_HELPERS_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_HELPERS_H_

#include "base/strings/string_piece.h"
#include "third_party/blink/public/platform/web_url.h"

#include <string>

namespace content {
struct TestPreferences;
struct WebPreferences;

// The TestRunner library keeps its settings in a TestPreferences object.
// The content_shell, however, uses WebPreferences. This method exports the
// settings from the TestRunner library which are relevant for web tests.
void ExportWebTestSpecificPreferences(const TestPreferences& from,
                                      WebPreferences* to);

// Replaces file:///tmp/web_tests/ with the actual path to the web_tests
// directory, or rewrite URLs generated from absolute path links in
// web-platform-tests.
blink::WebURL RewriteWebTestsURL(base::StringPiece utf8_url, bool is_wpt_mode);

// The same as RewriteWebTestsURL() unless the resource is a path starting
// with /tmp/, then return a file URL to a temporary file.
blink::WebURL RewriteFileURLToLocalResource(base::StringPiece resource);

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_HELPERS_H_
