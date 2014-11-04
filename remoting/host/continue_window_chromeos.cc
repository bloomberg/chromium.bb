// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

namespace remoting {

namespace {

// A place holder implementation for the ContinueWindow on
// ChromeOS. Remote assistance on Chrome OS is currently under a flag so it is
// secure to leave it unimplemented for now.
class ContinueWindowAura : public ContinueWindow {
 public:
  ContinueWindowAura();
  ~ContinueWindowAura() override;

 protected:
  // ContinueWindow interface.
  void ShowUi() override;
  void HideUi() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContinueWindowAura);
};

ContinueWindowAura::ContinueWindowAura() {
}

ContinueWindowAura::~ContinueWindowAura() {
}

void ContinueWindowAura::ShowUi() {
  // TODO(kelvinp): Implement this on Chrome OS (See crbug.com/424908).
  NOTIMPLEMENTED();
}

void ContinueWindowAura::HideUi() {
  // TODO(kelvinp): Implement this on Chrome OS (See crbug.com/424908).
  NOTIMPLEMENTED();
}

}  // namespace

// static
scoped_ptr<HostWindow> HostWindow::CreateContinueWindow() {
  return make_scoped_ptr(new ContinueWindowAura());
}

}  // namespace remoting
