// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is an application of a minimal host process in a Chromoting
// system. It serves the purpose of gluing different pieces together
// to make a functional host process for testing.
//
// It peforms the following functionality:
// 1. Connect to the GTalk network and register the machine as a host.
// 2. Accepts connection through libjingle.
// 3. Receive mouse / keyboard events through libjingle.
// 4. Sends screen capture through libjingle.

#include <iostream>
#include <string>
#include <stdlib.h>

#include "build/build_config.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/nss_util.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/thread.h"
#include "remoting/base/encoder_verbatim.h"
#include "remoting/base/encoder_zlib.h"
#include "remoting/host/capturer_fake.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/json_host_config.h"

#if defined(OS_WIN)
#include "remoting/host/capturer_gdi.h"
#include "remoting/host/event_executor_win.h"
#elif defined(OS_LINUX)
#include "remoting/host/capturer_linux.h"
#include "remoting/host/event_executor_linux.h"
#elif defined(OS_MACOSX)
#include "remoting/host/capturer_mac.h"
#include "remoting/host/event_executor_mac.h"
#endif

#if defined(OS_WIN)
const std::wstring kDefaultConfigPath = L".ChromotingConfig.json";
const wchar_t kHomeDrive[] = L"HOMEDRIVE";
const wchar_t kHomePath[] = L"HOMEPATH";
const wchar_t* GetEnvironmentVar(const wchar_t* x) { return _wgetenv(x); }
#else
const std::string kDefaultConfigPath = ".ChromotingConfig.json";
static char* GetEnvironmentVar(const char* x) { return getenv(x); }
#endif

void ShutdownTask(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

const std::string kFakeSwitchName = "fake";
const std::string kConfigSwitchName = "config";
const std::string kVerbatimSwitchName = "verbatim";

int main(int argc, char** argv) {
  // Needed for the Mac, so we don't leak objects when threads are created.
  base::ScopedNSAutoreleasePool pool;

  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  base::AtExitManager exit_manager;

  base::EnsureNSPRInit();

  scoped_ptr<remoting::Capturer> capturer;
  scoped_ptr<remoting::Encoder> encoder;
  scoped_ptr<remoting::EventExecutor> event_handler;
#if defined(OS_WIN)
  capturer.reset(new remoting::CapturerGdi());
  event_handler.reset(new remoting::EventExecutorWin(capturer.get()));
#elif defined(OS_LINUX)
  capturer.reset(new remoting::CapturerLinux());
  event_handler.reset(new remoting::EventExecutorLinux(capturer.get()));
#elif defined(OS_MACOSX)
  capturer.reset(new remoting::CapturerMac());
  event_handler.reset(new remoting::EventExecutorMac(capturer.get()));
#endif
  encoder.reset(new remoting::EncoderZlib());

  // Check the argument to see if we should use a fake capturer and encoder.
  bool fake = cmd_line->HasSwitch(kFakeSwitchName);
  bool verbatim = cmd_line->HasSwitch(kVerbatimSwitchName);

#if defined(OS_WIN)
  std::wstring path = GetEnvironmentVar(kHomeDrive);
  path += GetEnvironmentVar(kHomePath);
#else
  std::string path = GetEnvironmentVar(base::env_vars::kHome);
#endif
  FilePath config_path(path);
  config_path = config_path.Append(kDefaultConfigPath);
  if (cmd_line->HasSwitch(kConfigSwitchName)) {
    config_path = cmd_line->GetSwitchValuePath(kConfigSwitchName);
  }

  if (fake) {
    // Inject a fake capturer.
    LOG(INFO) << "Using a fake capturer.";
    capturer.reset(new remoting::CapturerFake());
  }

  if (verbatim) {
    LOG(INFO) << "Using the verbatim encoder.";
    encoder.reset(new remoting::EncoderVerbatim());
  }

  base::Thread file_io_thread("FileIO");
  file_io_thread.Start();

  scoped_refptr<remoting::JsonHostConfig> config(
      new remoting::JsonHostConfig(
          config_path, file_io_thread.message_loop_proxy()));

  if (!config->Read()) {
    LOG(ERROR) << "Failed to read configuration file " << config_path.value();
    return 1;
  }

  // Allocate a chromoting context and starts it.
  remoting::ChromotingHostContext context;
  context.Start();

  // Construct a chromoting host.
  scoped_refptr<remoting::ChromotingHost> host =
      new remoting::ChromotingHost(&context,
                                   config,
                                   capturer.release(),
                                   encoder.release(),
                                   event_handler.release());

  // Let the chromoting host run until the shutdown task is executed.
  MessageLoop message_loop(MessageLoop::TYPE_UI);
  host->Start(NewRunnableFunction(&ShutdownTask, &message_loop));
  message_loop.Run();

  // And then stop the chromoting context.
  context.Stop();
  file_io_thread.Stop();
  return 0;
}
