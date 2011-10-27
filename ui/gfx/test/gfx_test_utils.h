// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GFX_TEST_UTILS_H_
#define UI_GFX_GFX_TEST_UTILS_H_
#pragma once

namespace ui {
namespace gfx_test_utils {

// Called by tests that want to use a mocked compositor.
// Implementation is no-oped on platforms that don't use a compositor.
void SetupTestCompositor();

}  // namespace gfx_test_utils
}  // namespace ui

#endif  // UI_GFX_GFX_TEST_UTILS_H_
