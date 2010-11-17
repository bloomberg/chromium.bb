// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task.h"
#include "remoting/host/host_stub_fake.h"

namespace remoting {

void HostStubFake::SuggestResolution(
    const protocol::SuggestResolutionRequest* msg, Task* done) {
  done->Run();
  delete done;
}

}  // namespace remoting
