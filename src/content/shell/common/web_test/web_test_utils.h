// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helpful layout_test utility functions, built in both content_shell and
// test_runner.

#ifndef CONTENT_SHELL_COMMON_WEB_TEST_WEB_TEST_UTILS_H_
#define CONTENT_SHELL_COMMON_WEB_TEST_WEB_TEST_UTILS_H_

class SkBitmap;
namespace blink {
struct WebRect;
}

namespace content {
namespace web_test_utils {

void DrawSelectionRect(const SkBitmap& bitmap, const blink::WebRect& wr);

}  // namespace web_test_utils
}  // namespace content

#endif  // CONTENT_SHELL_COMMON_WEB_TEST_WEB_TEST_UTILS_H_
