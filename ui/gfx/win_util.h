// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_UTIL_H_
#define UI_GFX_WIN_UTIL_H_
#pragma once

namespace gfx {

// Returns true if Direct2d is available, false otherwise.
bool Direct2dIsAvailable();

// Returns true if DirectWrite is available, false otherwise.
bool DirectWriteIsAvailable();

}  // namespace gfx;

#endif  // UI_GFX_WIN_UTIL_H_

