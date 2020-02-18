// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_SECURITY_TEST_UTIL_H_
#define IPC_IPC_SECURITY_TEST_UTIL_H_

#include "base/macros.h"

namespace IPC {

class ChannelProxy;
class Message;

class IpcSecurityTestUtil {
 public:
  // Enables testing of security exploit scenarios where a compromised child
  // process can send a malicious message of an arbitrary type.
  //
  // This function will post the message to the IPC channel's thread, where it
  // is offered to the channel's listeners. Afterwards, a reply task is posted
  // back to the current thread. This function blocks until the reply task is
  // received. For messages forwarded back to the current thread, we won't
  // return until after the message has been handled here.
  //
  // Use this only for testing security bugs in a browsertest; other uses are
  // likely perilous. Unit tests should be using IPC::TestSink which has an
  // OnMessageReceived method you can call directly. Non-security browsertests
  // should just exercise the child process's normal codepaths to send messages.
  static void PwnMessageReceived(ChannelProxy* channel, const Message& message);

 private:
  IpcSecurityTestUtil();  // Not instantiable.

  DISALLOW_COPY_AND_ASSIGN(IpcSecurityTestUtil);
};

}  // namespace IPC

#endif  // IPC_IPC_SECURITY_TEST_UTIL_H_
