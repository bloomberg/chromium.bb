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
#include "chrome/common/chrome_switches.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_switches.h"

namespace ipc_fuzzer {

ReplayProcess::ReplayProcess()
    : io_thread_("Chrome_ChildIOThread"),
      shutdown_event_(true, false),
      message_index_(0) {
}

ReplayProcess::~ReplayProcess() {
  channel_.reset();
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

  channel_ = IPC::ChannelProxy::Create(channel_name,
                                       IPC::Channel::MODE_CLIENT,
                                       this,
                                       io_thread_.message_loop_proxy());
}

bool ReplayProcess::OpenTestcase() {
  base::FilePath path = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kIpcFuzzerTestcase);
  return MessageFile::Read(path, &messages_);
}

void ReplayProcess::SendNextMessage() {
  if (message_index_ >= messages_.size()) {
    base::MessageLoop::current()->Quit();
    return;
  }

  // Take next message and release it from vector.
  IPC::Message* message = messages_[message_index_];
  messages_[message_index_++] = NULL;

  if (!channel_->Send(message)) {
    LOG(ERROR) << "ChannelProxy::Send() failed after "
               << message_index_ << " messages";
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
  LOG(ERROR) << "Channel error, quitting after "
             << message_index_ << " messages";
  base::MessageLoop::current()->Quit();
}

}  // namespace ipc_fuzzer
