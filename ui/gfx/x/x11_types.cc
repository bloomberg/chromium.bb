// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/x11_types.h"

#include "base/message_loop/message_loop.h"

namespace gfx {

XDisplay* GetXDisplay() {
  return base::MessagePumpForUI::GetDefaultXDisplay();
}

}  // namespace gfx

