// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TRANSIENT_WINDOW_CONTROLLER_H_
#define UI_VIEWS_COREWM_TRANSIENT_WINDOW_CONTROLLER_H_

#include "ui/aura/client/transient_window_client.h"
#include "ui/views/views_export.h"

namespace views {
namespace corewm {

// TransientWindowClient implementation. Uses TransientWindowManager to handle
// tracking transient per window.
class VIEWS_EXPORT TransientWindowController
    : public aura::client::TransientWindowClient {
 public:
  TransientWindowController();
  virtual ~TransientWindowController();

  // TransientWindowClient:
  virtual void AddTransientChild(aura::Window* parent,
                                 aura::Window* child) OVERRIDE;
  virtual void RemoveTransientChild(aura::Window* parent,
                                    aura::Window* child) OVERRIDE;
  virtual aura::Window* GetTransientParent(aura::Window* window) OVERRIDE;
  virtual const aura::Window* GetTransientParent(
      const aura::Window* window) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TransientWindowController);
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TRANSIENT_WINDOW_CONTROLLER_H_
