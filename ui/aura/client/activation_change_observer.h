// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_ACTIVATION_CHANGE_OBSERVER_H_
#define UI_AURA_CLIENT_ACTIVATION_CHANGE_OBSERVER_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace aura {
class Window;

namespace client {

class AURA_EXPORT ActivationChangeObserver {
 public:
  // Called when |active| gains focus, or there is no active window
  // (|active| is NULL in this case.) |old_active| refers to the
  // previous active window or NULL if there was no previously active
  // window.
  virtual void OnWindowActivated(Window* active, Window* old_active) = 0;

 protected:
  virtual ~ActivationChangeObserver() {}
};

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_ACTIVATION_CHANGE_OBSERVER_H_
