// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode.h"

#include "base/logging.h"

namespace remoting {

class CurtainModeWin : public CurtainMode {
 public:
  CurtainModeWin() {}
  // Overriden from CurtainMode.
  virtual void SetActivated(bool activated) OVERRIDE {
    NOTIMPLEMENTED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CurtainModeWin);
};

// static
scoped_ptr<CurtainMode> CurtainMode::Create(
    const base::Closure& on_session_activate,
    const base::Closure& on_error) {
  return scoped_ptr<CurtainMode>(
      new CurtainModeWin());
}

}  // namespace remoting
