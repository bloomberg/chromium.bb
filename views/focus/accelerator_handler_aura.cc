// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/focus/accelerator_handler.h"

namespace views {

AcceleratorHandler::AcceleratorHandler() {
}

bool AcceleratorHandler::Dispatch(const MSG& msg) {
#if defined(OS_WIN)
  TranslateMessage(&msg);
  DispatchMessage(&msg);
#endif
  return true;
}

}  // namespace views
