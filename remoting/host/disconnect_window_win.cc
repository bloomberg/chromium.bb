// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/disconnect_window.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace {
class DisconnectWindowWin : public remoting::DisconnectWindow {
 public:
  DisconnectWindowWin() {}
  virtual void Show(remoting::ChromotingHost* host,
                    const std::string& username) OVERRIDE;
  virtual void Hide() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisconnectWindowWin);
};
}  // namespace

void DisconnectWindowWin::Show(remoting::ChromotingHost* host,
                                 const std::string& username) {
  NOTIMPLEMENTED();
}

void DisconnectWindowWin::Hide() {
  NOTIMPLEMENTED();
}

remoting::DisconnectWindow* remoting::DisconnectWindow::Create() {
  return new DisconnectWindowWin;
}
