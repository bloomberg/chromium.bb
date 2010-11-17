// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A fake HostStub implementation that does not do anything.

#ifndef REMOTING_PROTOCOL_HOST_STUB_FAKE_H_
#define REMOTING_PROTOCOL_HOST_STUB_FAKE_H_

#include "base/basictypes.h"
#include "remoting/protocol/host_stub.h"

namespace remoting {

class HostStubFake : public protocol::HostStub {
 public:
  HostStubFake() {}
  virtual ~HostStubFake() {}

  virtual void SuggestResolution(
      const protocol::SuggestResolutionRequest* msg, Task* done);

 private:
  DISALLOW_COPY_AND_ASSIGN(HostStubFake);
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_STUB_FAKE_H_
