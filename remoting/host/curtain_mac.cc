// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mac.h"
#include "base/logging.h"

namespace remoting {

void CurtainMac::EnableCurtainMode(bool enable) {
  NOTIMPLEMENTED();
}

Curtain* Curtain::Create() {
  return new CurtainMac();
}

}  // namespace remoting
