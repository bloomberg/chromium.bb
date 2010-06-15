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

#include "build/build_config.h"

#if defined(OS_POSIX)
#include <termios.h>
#endif  // defined (OS_POSIX)

#include "base/at_exit.h"
#include "base/waitable_event.h"
#include "remoting/host/capturer_fake.h"
#include "remoting/host/encoder_verbatim.h"
#include "remoting/host/host_config.h"
#include "remoting/host/chromoting_host.h"

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

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;

  std::string username;
  std::string auth_token;

  // Check the argument to see if we should use a fake capturer and encoder.
  bool fake = false;
  // True if the JID was specified on the command line.
  bool has_jid = false;
  // True if the auth token was specified on the command line.
  bool has_auth = false;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--fake") {
      fake = true;
    } else if (arg == "--jid") {
      if (++i >= argc) {
        std::cerr << "Expected JID to follow --jid option" << std::endl;
        return 1;
      }
      has_jid = true;
      username = argv[i];
    } else if (arg == "--auth") {
      if (++i >= argc) {
        std::cerr << "Expected auth token to follow --auth option" << std::endl;
        return 1;
      }
      has_auth = true;
      auth_token = argv[i];
    }
  }

  // Prompt user for username and auth token.
  if (!has_jid) {
    std::cout << "JID: ";
    std::cin >> username;
  }
  if (!has_auth) {
    std::cout << "Auth Token: ";
    std::cin >> auth_token;
  }

  scoped_ptr<remoting::Capturer> capturer;
  scoped_ptr<remoting::Encoder> encoder;
  scoped_ptr<remoting::EventExecutor> executor;
#if defined(OS_WIN)
  capturer.reset(new remoting::CapturerGdi());
  executor.reset(new remoting::EventExecutorWin());
#elif defined(OS_LINUX)
  capturer.reset(new remoting::CapturerLinux());
  executor.reset(new remoting::EventExecutorLinux());
#elif defined(OS_MACOSX)
  capturer.reset(new remoting::CapturerMac());
  executor.reset(new remoting::EventExecutorMac());
#endif
  encoder.reset(new remoting::EncoderVerbatim());

  if (fake) {
    // Inject a fake capturer.
    capturer.reset(new remoting::CapturerFake());
  }

  // TODO(sergeyu): Implement HostConfigStorage and use it here to load
  // settings.
  scoped_refptr<remoting::HostConfig> config(new remoting::HostConfig());
  config->set_xmpp_login(username);
  config->set_xmpp_auth_token(auth_token);
  config->set_host_id("foo");

  // Construct a chromoting host with username and auth_token.
  base::WaitableEvent host_done(false, false);
  scoped_refptr<remoting::ChromotingHost> host =
      new remoting::ChromotingHost(config,
                                   capturer.release(),
                                   encoder.release(),
                                   executor.release(),
                                   &host_done);
  host->Run();
  host_done.Wait();
  return 0;
}
