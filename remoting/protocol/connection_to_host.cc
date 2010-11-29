// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/connection_to_host.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/video_reader.h"
#include "remoting/protocol/video_stub.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

ConnectionToHost::ConnectionToHost() {
}

ConnectionToHost::~ConnectionToHost() {
}

InputStub* ConnectionToHost::input_stub() {
  return input_stub_.get();
}

HostStub* ConnectionToHost::host_stub() {
  return host_stub_.get();
}

}  // namespace protocol
}  // namespace remoting
