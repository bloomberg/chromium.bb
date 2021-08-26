// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_UTIL_H_
#define CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_UTIL_H_

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class ToRenderFrameHost;
class WebContents;
}  // namespace content

namespace gfx {
class Point;
}  // namespace gfx

namespace pdf_extension_test_util {

// Ensures, inside the given `frame`, that a PDF has either finished
// loading or prompted a password. The result indicates success if the PDF loads
// successfully, otherwise it indicates failure. If it doesn't finish loading,
// the test will hang.
//
// Tests that attempt to send mouse/pointer events should pass `true` for
// `wait_for_hit_test_data`, otherwise the necessary hit test data may not be
// available by the time this function returns. (This behavior is the default,
// since the delay should be small.)
testing::AssertionResult EnsurePDFHasLoaded(
    const content::ToRenderFrameHost& frame,
    bool wait_for_hit_test_data = true) WARN_UNUSED_RESULT;

gfx::Point ConvertPageCoordToScreenCoord(content::WebContents* contents,
                                         const gfx::Point& point);

}  // namespace pdf_extension_test_util

#endif  // CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_UTIL_H_
