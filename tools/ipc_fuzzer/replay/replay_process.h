// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_IPC_FUZZER_REPLAY_REPLAY_PROCESS_H_
#define TOOLS_IPC_FUZZER_REPLAY_REPLAY_PROCESS_H_

#include <list>

#include "base/files/memory_mapped_file.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"

namespace ipc_fuzzer {

class ReplayProcess : public IPC::Listener {
 public:
  ReplayProcess();
  virtual ~ReplayProcess();

  // Set up command line, logging, IO thread. Returns true on success, false
  // otherwise.
  bool Initialize(int argc, const char** argv);

  // Open a channel to the browser process. It will think we are a renderer.
  void OpenChannel();

  // Extract messages from a file specified by --ipc-fuzzer-testcase=
  // Returns true on success, false otherwise.
  bool OpenTestcase();

  // Send messages to the browser.
  void Run();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
  bool ExtractMessages(const char *data, size_t len);
  void SendNextMessage();

  scoped_ptr<IPC::ChannelProxy> channel_;
  base::MessageLoop main_loop_;
  base::Thread io_thread_;
  base::WaitableEvent shutdown_event_;
  scoped_ptr<base::Timer> timer_;
  scoped_ptr<base::MemoryMappedFile> mapped_testcase_;
  std::list<IPC::Message*> messages_;

  DISALLOW_COPY_AND_ASSIGN(ReplayProcess);
};

}  // namespace ipc_fuzzer

#endif  // TOOLS_IPC_FUZZER_REPLAY_REPLAY_PROCESS_H_
