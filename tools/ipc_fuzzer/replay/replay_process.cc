// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/ipc_fuzzer/replay/replay_process.h"

#include <limits.h>
#include <string>
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/posix/global_descriptors.h"
#include "base/stl_util.h"
#include "chrome/common/chrome_switches.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_switches.h"

namespace ipc_fuzzer {

ReplayProcess::ReplayProcess()
    : main_loop_(base::MessageLoop::TYPE_DEFAULT),
      io_thread_("Chrome_ChildIOThread"),
      shutdown_event_(true, false) {
}

ReplayProcess::~ReplayProcess() {
  channel_.reset();
  STLDeleteElements(&messages_);
}

bool ReplayProcess::Initialize(int argc, const char** argv) {
  CommandLine::Init(argc, argv);

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kIpcFuzzerTestcase)) {
    LOG(ERROR) << "This binary shouldn't be executed directly, "
               << "please use tools/ipc_fuzzer/play_testcase.py";
    return false;
  }

  // Log to default destination.
  logging::SetMinLogLevel(logging::LOG_ERROR);
  logging::InitLogging(logging::LoggingSettings());

  io_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  g_fds->Set(kPrimaryIPCChannel,
             kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor);
  return true;
}

void ReplayProcess::OpenChannel() {
  std::string channel_name =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessChannelID);

  channel_.reset(
      new IPC::ChannelProxy(channel_name,
                            IPC::Channel::MODE_CLIENT,
                            this,
                            io_thread_.message_loop_proxy()));
}

bool ReplayProcess::ExtractMessages(const char *data, size_t len) {
  const char* end = data + len;

  while (data < end) {
    const char* message_tail = IPC::Message::FindNext(data, end);
    if (!message_tail) {
      LOG(ERROR) << "Failed to extract message";
      return false;
    }

    size_t len = message_tail - data;
    if (len > INT_MAX) {
      LOG(ERROR) << "Message too large";
      return false;
    }

    IPC::Message* message = new IPC::Message(data, len);
    messages_.push_back(message);
    data = message_tail;
  }

  return true;
}

bool ReplayProcess::OpenTestcase() {
  base::FilePath path = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kIpcFuzzerTestcase);
  mapped_testcase_.reset(new base::MemoryMappedFile());
  if (!mapped_testcase_->Initialize(path)) {
    LOG(ERROR) << "Failed to map testcase: " << path.value();
    return false;
  }

  const char* data = reinterpret_cast<const char *>(mapped_testcase_->data());
  size_t len = mapped_testcase_->length();

  return ExtractMessages(data, len);
}

void ReplayProcess::SendNextMessage() {
  if (messages_.empty()) {
    base::MessageLoop::current()->Quit();
    return;
  }

  IPC::Message* message = messages_.front();
  messages_.pop_front();

  if (!channel_->Send(message)) {
    LOG(ERROR) << "ChannelProxy::Send() failed";
    base::MessageLoop::current()->Quit();
  }
}

void ReplayProcess::Run() {
  timer_.reset(new base::Timer(false, true));
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromMilliseconds(1),
                base::Bind(&ReplayProcess::SendNextMessage,
                           base::Unretained(this)));
  base::MessageLoop::current()->Run();
}

bool ReplayProcess::OnMessageReceived(const IPC::Message& msg) {
  return true;
}

void ReplayProcess::OnChannelError() {
  LOG(ERROR) << "Channel error, quitting";
  base::MessageLoop::current()->Quit();
}

}  // namespace ipc_fuzzer
