// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DEBUG_UTILS_H_
#define UI_VIEWS_DEBUG_UTILS_H_
#pragma once

namespace views {

class View;

#ifndef NDEBUG

// Log the view hierarchy.
void PrintViewHierarchy(const View* view);

// Log the focus traversal hierarchy.
void PrintFocusHierarchy(const View* view);

#endif  // NDEBUG

}  // namespace views

#endif  // UI_VIEWS_DEBUG_UTILS_H_
