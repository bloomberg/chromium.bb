// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_UTILS_H_
#define UI_AURA_TEST_AURA_TEST_UTILS_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace gfx {
class Point;
}

namespace aura {
class WindowEventDispatcher;
class WindowTreeHost;

namespace test {

const gfx::Point& QueryLatestMousePositionRequestInHost(WindowTreeHost* host);
void SetHostDispatcher(WindowTreeHost* host,
                       scoped_ptr<WindowEventDispatcher> dispatcher);

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_UTILS_H_
