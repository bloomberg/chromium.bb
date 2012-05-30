// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_FOCUS_CHANGE_OBSERVER_H_
#define UI_AURA_FOCUS_CHANGE_OBSERVER_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace aura {

class Window;

class AURA_EXPORT FocusChangeObserver {
 public:
  // Called when |window| gains focus.
  virtual void OnWindowFocused(Window* window) = 0;

 protected:
  virtual ~FocusChangeObserver() {}
};

}  // namespace aura

#endif  // UI_AURA_FOCUS_CHANGE_OBSERVER_H_
