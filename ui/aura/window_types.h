// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TYPES_H_
#define UI_AURA_WINDOW_TYPES_H_
#pragma once

namespace aura {

// This file is obsolete. Please do not add any new types.
// This still exists as Window::Init uses Type_Control.
// TODO(ben|oshima): Figure out how to clean this up.

const int kWindowType_None = 0;
const int kWindowType_Control = 1;

const int kWindowType_Max = 2;

}  // namespace aura

#endif  // UI_AURA_WINDOW_TYPES_H_
