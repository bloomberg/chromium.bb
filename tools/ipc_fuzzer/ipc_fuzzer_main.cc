// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <list>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/global_descriptors.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "chrome/common/chrome_switches.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_switches.h"

namespace {

class IpcFuzzerProcess : public IPC::Listener {
 public:
  IpcFuzzerProcess();
  virtual ~IpcFuzzerProcess();

  // Set up command line, logging, IO thread.
  void Initialize(int argc, const char **argv);

  // Open a channel to the browser process. It will think we are a renderer.
  void OpenChannel();

  // Extract messages from a file specified by --ipc-fuzzer-testcase=.
  bool OpenTestcase();

  // Trigger the sending of messages to the browser.
  void StartSendingMessages();

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
  scoped_ptr<base::MemoryMappedFile> testcase_map_;
  std::list<IPC::Message*> messages_;

  DISALLOW_COPY_AND_ASSIGN(IpcFuzzerProcess);
};

IpcFuzzerProcess::IpcFuzzerProcess()
    : main_loop_(base::MessageLoop::TYPE_DEFAULT),
      io_thread_("Chrome_ChildIOThread"),
      shutdown_event_(true, false) {
}

IpcFuzzerProcess::~IpcFuzzerProcess() {
  channel_.reset();
  STLDeleteElements(&messages_);
}

void IpcFuzzerProcess::Initialize(int argc, const char **argv) {
  CommandLine::Init(argc, argv);

  // Log to default destination.
  logging::SetMinLogLevel(logging::LOG_ERROR);
  logging::InitLogging(logging::LoggingSettings());

  io_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

#if defined(OS_POSIX)
  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  g_fds->Set(kPrimaryIPCChannel,
             kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor);
#endif
}

void IpcFuzzerProcess::OpenChannel() {
  std::string channel_name =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessChannelID);

  channel_.reset(
      new IPC::ChannelProxy(channel_name,
                            IPC::Channel::MODE_CLIENT,
                            this,
                            io_thread_.message_loop_proxy()));
}

bool IpcFuzzerProcess::ExtractMessages(const char *data, size_t len) {
  const char* end = data + len;

  while (data < end) {
    const char* message_tail = IPC::Message::FindNext(data, end);
    if (!message_tail)
      break;

    size_t len = message_tail - data;
    if (len > INT_MAX) {
      LOG(ERROR) << "Message too large";
      break;
    }

    IPC::Message* message = new IPC::Message(data, len);
    messages_.push_back(message);
    data = message_tail;
  }

  if (data < end) {
    unsigned long left = end - data;
    LOG(ERROR) << left << " bytes left while extracting messages";
    return false;
  }

  return true;
}

bool IpcFuzzerProcess::OpenTestcase() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (!command_line.HasSwitch(switches::kIpcFuzzerTestcase)) {
    LOG(ERROR) << "No IPC fuzzer testcase specified";
    return false;
  }

  base::FilePath path =
      command_line.GetSwitchValuePath(switches::kIpcFuzzerTestcase);
  testcase_map_.reset(new base::MemoryMappedFile());
  if (!testcase_map_->Initialize(path)) {
    LOG(ERROR) << "Failed to map testcase: " << path.value();
    return false;
  }

  const char* data = reinterpret_cast<const char *>(testcase_map_->data());
  size_t len = testcase_map_->length();

  return ExtractMessages(data, len);
}

void IpcFuzzerProcess::SendNextMessage() {
  if (messages_.empty()) {
    base::MessageLoop::current()->Quit();
    return;
  }

  IPC::Message* message = messages_.front();
  messages_.pop_front();

  channel_->Send(message);
}

void IpcFuzzerProcess::StartSendingMessages() {
  timer_.reset(new base::Timer(false, true));
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromMilliseconds(1),
                base::Bind(&IpcFuzzerProcess::SendNextMessage,
                           base::Unretained(this)));
}

bool IpcFuzzerProcess::OnMessageReceived(const IPC::Message& msg) {
  return true;
}

void IpcFuzzerProcess::OnChannelError() {
  LOG(ERROR) << "Channel error, quitting";
  base::MessageLoop::current()->Quit();
}

}  // namespace

int main(int argc, const char **argv) {
  IpcFuzzerProcess fuzzer;
  fuzzer.Initialize(argc, argv);
  fuzzer.OpenChannel();

  if (!fuzzer.OpenTestcase())
    return 0;

  fuzzer.StartSendingMessages();

  base::MessageLoop::current()->Run();
  return 0;
}
