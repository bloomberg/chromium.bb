// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TRANSIENT_WINDOW_OBSERVER_H_
#define UI_VIEWS_COREWM_TRANSIENT_WINDOW_OBSERVER_H_

#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace views {
namespace corewm {

class VIEWS_EXPORT TransientWindowObserver {
 public:
  // Called when a transient child is added to |window|.
  virtual void OnTransientChildAdded(aura::Window* window,
                                     aura::Window* transient) = 0;

  // Called when a transient child is removed from |window|.
  virtual void OnTransientChildRemoved(aura::Window* window,
                                       aura::Window* transient) = 0;

 protected:
  virtual ~TransientWindowObserver() {}
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TRANSIENT_WINDOW_OBSERVER_H_
