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
  virtual void GetConfig(const GetConfigCallback& callback) OVERRIDE;
  virtual void SetConfigAndStart(
      scoped_ptr<base::DictionaryValue> config) OVERRIDE;
  virtual void SetPin(const std::string& pin) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DaemonControllerMac);
};

DaemonControllerMac::DaemonControllerMac() {
}

DaemonController::State DaemonControllerMac::GetState() {
  return DaemonController::STATE_NOT_IMPLEMENTED;
}

void DaemonControllerMac::GetConfig(const GetConfigCallback& callback) {
  NOTIMPLEMENTED();
}

void DaemonControllerMac::SetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config) {
  NOTIMPLEMENTED();
}

void DaemonControllerMac::SetPin(const std::string& pin) {
  NOTIMPLEMENTED();
}

void DaemonControllerMac::Stop() {
  NOTIMPLEMENTED();
}

}  // namespace

DaemonController* remoting::DaemonController::Create() {
  return new DaemonControllerMac();
}

}  // namespace remoting
