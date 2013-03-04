// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode.h"

#include "base/logging.h"

namespace remoting {

class CurtainModeWin : public CurtainMode {
 public:
  CurtainModeWin(const base::Closure& on_error) : on_error_(on_error) {}
  // Overriden from CurtainMode.
  virtual void SetActivated(bool activated) OVERRIDE {
    // Curtain-mode is not currently implemented for Windows.
    if (activated) {
      LOG(ERROR) << "Curtain-mode is not yet supported on Windows.";
      on_error_.Run();
    }
  }

 private:
  base::Closure on_error_;

  DISALLOW_COPY_AND_ASSIGN(CurtainModeWin);
};

// static
scoped_ptr<CurtainMode> CurtainMode::Create(
    const base::Closure& on_session_activate,
    const base::Closure& on_error) {
  return scoped_ptr<CurtainMode>(
      new CurtainModeWin(on_error));
}

}  // namespace remoting
