// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/daemon_controller.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"

namespace remoting {

namespace {

class DaemonControllerMac : public remoting::DaemonController {
 public:
  DaemonControllerMac();

  virtual State GetState() OVERRIDE;
  virtual bool SetPin(const std::string& pin) OVERRIDE;
  virtual bool Start() OVERRIDE;
  virtual bool Stop() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DaemonControllerMac);
};

DaemonControllerMac::DaemonControllerMac() {
}

remoting::DaemonController::State DaemonControllerMac::GetState() {
  return remoting::DaemonController::STATE_NOT_IMPLEMENTED;
}

bool DaemonControllerMac::SetPin(const std::string& pin) {
  NOTIMPLEMENTED();
  return false;
}

bool DaemonControllerMac::Start() {
  NOTIMPLEMENTED();
  return false;
}

bool DaemonControllerMac::Stop() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace

DaemonController* remoting::DaemonController::Create() {
  return new DaemonControllerMac();
}

}  // namespace remoting
