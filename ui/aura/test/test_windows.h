// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_WINDOWS_H_
#define UI_AURA_TEST_TEST_WINDOWS_H_
#pragma once

#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window_types.h"

namespace aura {
namespace test {

Window* CreateTestWindowWithId(int id, Window* parent);
Window* CreateTestWindowWithBounds(const gfx::Rect& bounds, Window* parent);
Window* CreateTestWindow(SkColor color,
                         int id,
                         const gfx::Rect& bounds,
                         Window* parent);
Window* CreateTestWindowWithDelegate(WindowDelegate* delegate,
                                     int id,
                                     const gfx::Rect& bounds,
                                     Window* parent);
Window* CreateTestWindowWithDelegateAndType(WindowDelegate* delegate,
                                            WindowType type,
                                            int id,
                                            const gfx::Rect& bounds,
                                            Window* parent);

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_WINDOWS_H_
