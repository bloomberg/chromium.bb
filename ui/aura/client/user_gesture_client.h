// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_USER_GESTURE_CLIENT_H_
#define UI_AURA_CLIENT_USER_GESTURE_CLIENT_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace aura {
class RootWindow;
namespace client {

// An interface for handling user gestures that aren't handled by the standard
// event path.
class AURA_EXPORT UserGestureClient {
 public:
  enum Gesture {
    GESTURE_BACK = 0,
    GESTURE_FORWARD,
  };

  // Returns true if the gesture was handled and false otherwise.
  virtual bool OnUserGesture(Gesture gesture) = 0;

 protected:
  virtual ~UserGestureClient() {}
};

// Sets/gets the client for handling gestures on the specified root window.
AURA_EXPORT void SetUserGestureClient(RootWindow* root_window,
                                      UserGestureClient* client);
AURA_EXPORT UserGestureClient* GetUserGestureClient(RootWindow* root_window);

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_USER_GESTURE_CLIENT_H_
