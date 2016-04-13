// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "remoting/host/host_window.h"

namespace remoting {

namespace {

class DisconnectWindowAndroid : public HostWindow {
 public:
  DisconnectWindowAndroid() {}

  // HostWindow interface.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override {
    NOTIMPLEMENTED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DisconnectWindowAndroid);
};

}  // namespace

// static
std::unique_ptr<HostWindow> HostWindow::CreateDisconnectWindow() {
  return base::WrapUnique(new DisconnectWindowAndroid());
}

}  // namespace remoting
