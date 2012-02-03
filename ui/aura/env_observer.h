// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ENV_OBSERVER_H_
#define UI_AURA_ENV_OBSERVER_H_
#pragma once

#include "base/observer_list.h"
#include "ui/aura/aura_export.h"

namespace aura {

class Window;

class AURA_EXPORT EnvObserver {
 public:
  // Called when |window| has been initialized.
  virtual void OnWindowInitialized(Window* window) = 0;

 protected:
  virtual ~EnvObserver() {}
};

}  // namespace aura

#endif  // UI_AURA_ENV_OBSERVER_H_
