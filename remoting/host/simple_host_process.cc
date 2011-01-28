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
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/nss_util.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "media/base/media.h"
#include "remoting/base/tracer.h"
#include "remoting/host/capturer_fake.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/json_host_config.h"
#include "remoting/proto/video.pb.h"

using remoting::ChromotingHost;
using remoting::protocol::CandidateSessionConfig;
using remoting::protocol::ChannelConfig;
using std::string;
using std::wstring;

#if defined(OS_WIN)
const wchar_t kDefaultConfigPath[] = L".ChromotingConfig.json";
const wchar_t kHomeDrive[] = L"HOMEDRIVE";
const wchar_t kHomePath[] = L"HOMEPATH";
// TODO(sergeyu): Use environment utils from base/environment.h.
const wchar_t* GetEnvironmentVar(const wchar_t* x) { return _wgetenv(x); }
#else
const char kDefaultConfigPath[] = ".ChromotingConfig.json";
static char* GetEnvironmentVar(const char* x) { return getenv(x); }
#endif

void ShutdownTask(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

const char kFakeSwitchName[] = "fake";
const char kConfigSwitchName[] = "config";
const char kVideoSwitchName[] = "video";

const char kVideoSwitchValueVerbatim[] = "verbatim";
const char kVideoSwitchValueZip[] = "zip";
const char kVideoSwitchValueVp8[] = "vp8";
const char kVideoSwitchValueVp8Rtp[] = "vp8rtp";


int main(int argc, char** argv) {
  // Needed for the Mac, so we don't leak objects when threads are created.
  base::mac::ScopedNSAutoreleasePool pool;

  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  base::AtExitManager exit_manager;
  base::EnsureNSPRInit();

  // Allocate a chromoting context and starts it.
  remoting::ChromotingHostContext context;
  context.Start();


#if defined(OS_WIN)
  wstring home_path = GetEnvironmentVar(kHomeDrive);
  home_path += GetEnvironmentVar(kHomePath);
#else
  string home_path = GetEnvironmentVar(base::env_vars::kHome);
#endif
  FilePath config_path(home_path);
  config_path = config_path.Append(kDefaultConfigPath);
  if (cmd_line->HasSwitch(kConfigSwitchName)) {
    config_path = cmd_line->GetSwitchValuePath(kConfigSwitchName);
  }

  base::Thread file_io_thread("FileIO");
  file_io_thread.Start();

  scoped_refptr<remoting::JsonHostConfig> config(
      new remoting::JsonHostConfig(
          config_path, file_io_thread.message_loop_proxy()));

  if (!config->Read()) {
    LOG(ERROR) << "Failed to read configuration file " << config_path.value();
    context.Stop();
    return 1;
  }

  FilePath module_path;
  PathService::Get(base::DIR_MODULE, &module_path);
  CHECK(media::InitializeMediaLibrary(module_path))
      << "Cannot load media library";

  // Construct a chromoting host.
  scoped_refptr<ChromotingHost> host;

  bool fake = cmd_line->HasSwitch(kFakeSwitchName);
  if (fake) {
    remoting::Capturer* capturer =
        new remoting::CapturerFake(context.main_message_loop());
    remoting::protocol::InputStub* input_stub =
        CreateEventExecutor(context.main_message_loop(), capturer);
    host = ChromotingHost::Create(
        &context, config, capturer, input_stub);
  } else {
    host = ChromotingHost::Create(&context, config);
  }

  if (cmd_line->HasSwitch(kVideoSwitchName)) {
    string video_codec = cmd_line->GetSwitchValueASCII(kVideoSwitchName);
    scoped_ptr<CandidateSessionConfig> config(
        CandidateSessionConfig::CreateDefault());
    config->mutable_video_configs()->clear();

    ChannelConfig::TransportType transport = ChannelConfig::TRANSPORT_STREAM;
    ChannelConfig::Codec codec;
    if (video_codec == kVideoSwitchValueVerbatim) {
      codec = ChannelConfig::CODEC_VERBATIM;
    } else if (video_codec == kVideoSwitchValueZip) {
      codec = ChannelConfig::CODEC_ZIP;
    } else if (video_codec == kVideoSwitchValueVp8) {
      codec = ChannelConfig::CODEC_VP8;
    } else if (video_codec == kVideoSwitchValueVp8Rtp) {
      transport = ChannelConfig::TRANSPORT_SRTP;
      codec = ChannelConfig::CODEC_VP8;
    } else {
      LOG(ERROR) << "Unknown video codec: " << video_codec;
      context.Stop();
      return 1;
    }
    config->mutable_video_configs()->push_back(ChannelConfig(
        transport, remoting::protocol::kDefaultStreamVersion, codec));
    host->set_protocol_config(config.release());
  }

  // Let the chromoting host run until the shutdown task is executed.
  MessageLoop message_loop(MessageLoop::TYPE_UI);
  host->Start(NewRunnableFunction(&ShutdownTask, &message_loop));
  message_loop.Run();

  // And then stop the chromoting context.
  context.Stop();
  file_io_thread.Stop();
  return 0;
}
