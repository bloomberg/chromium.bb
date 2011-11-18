// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_TEST_TEST_UTILS_H_
#define UI_GFX_COMPOSITOR_TEST_TEST_UTILS_H_
#pragma once

namespace gfx {
class Rect;
}

namespace ui {

class Transform;

void CheckApproximatelyEqual(const Transform& lhs, const Transform& rhs);
void CheckApproximatelyEqual(const gfx::Rect& lhs, const gfx::Rect& rhs);

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_TEST_TEST_UTILS_H_
