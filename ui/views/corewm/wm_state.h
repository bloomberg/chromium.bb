// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_WM_STATE_H_
#define UI_VIEWS_COREWM_WM_STATE_H_

#include "base/memory/scoped_ptr.h"
#include "ui/views/views_export.h"

namespace views {
namespace corewm {

class TransientWindowController;
class TransientWindowStackingClient;

// Installs state needed by the window manager.
class VIEWS_EXPORT WMState {
 public:
  WMState();
  ~WMState();

  // WindowStackingClient:
 private:
  scoped_ptr<TransientWindowStackingClient> window_stacking_client_;
  scoped_ptr<TransientWindowController> transient_window_client_;

  DISALLOW_COPY_AND_ASSIGN(WMState);
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_WM_STATE_H_
