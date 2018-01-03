// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_PIXEL_TEST_H_
#define UI_VIEWS_TEST_VIEWS_PIXEL_TEST_H_

class SkBitmap;

namespace views {
namespace test {

// Selects a reference image under ui/views/test/data that matches |name|, then
// compares it to |bitmap|. Selects a platform-specific reference image if one
// is available. If there is neither a platform-specific image for the current
// platform, nor a platform-independent image, then sets |used_fallback| and
// uses a platform-specific image for a different platform.
//
// Note that "platform" is loosely defined, and may incorporate the OS version.
// The intent is to establish a set of reference images that ensure a level of
// test coverage in the default trybots that is appropriate for each test.
//
// Except when testing for platform-specific quirks, it is acceptable to have a
// single platform-specific reference image and ignore failures when the
// fallback is used on other platforms.
bool PixelTest(const SkBitmap& bitmap, const char* name, bool* used_fallback);

// Write the reference image to disk in the appropriate place for the current
// platform. This writes to the checkout location and should never be run on a
// bot.
void UpdateReferenceImage(const SkBitmap& bitmap, const char* name);

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_PIXEL_TEST_H_
