// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_EVENT_EXECUTOR_H_
#define REMOTING_HOST_EVENT_EXECUTOR_H_

class MessageLoopForUI;

namespace remoting {

class Capturer;

namespace protocol {
class InputStub;
}  // namespace protocol

// Creates default event executor for the current platform.
protocol::InputStub* CreateEventExecutor(MessageLoopForUI* message_loop,
                                         Capturer* capturer);

}  // namespace remoting

#endif  // REMOTING_HOST_EVENT_EXECUTOR_H_
