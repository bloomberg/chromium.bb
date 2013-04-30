// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlwin.h>
#include <ole2.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "remoting/host/setup/win/host_configurer_window.h"
#include "remoting/host/url_request_context.h"

class HostConfigurerModule
    : public ATL::CAtlExeModuleT<HostConfigurerModule> {
};

HostConfigurerModule _AtlModule;

// An app that runs a HostConfigurerWindow.
int WINAPI WinMain(HINSTANCE instance_handle, HINSTANCE prev_instance_handle,
                   LPSTR cmd_line, int cmd)
{
  // google_apis::GetOAuth2ClientID/Secret need the next line.
  // On Windows, CommandLine::Init ignores its arguments, and parses
  // GetCommandLineW directly, so we can pass it dummy arguments.
  CommandLine::Init(0, NULL);

  // Register and initialize common controls.
  INITCOMMONCONTROLSEX info;
  info.dwSize = sizeof(info);
  info.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&info);

  // This object instance is required by Chrome code (for example,
  // FilePath, LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  // Provide message loops and threads for the URLRequestContextGetter.
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  base::Thread io_thread("IO thread");
  base::Thread::Options io_thread_options(base::MessageLoop::TYPE_IO, 0);
  io_thread.StartWithOptions(io_thread_options);

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_(
      new remoting::URLRequestContextGetter(
          message_loop.message_loop_proxy(), io_thread.message_loop_proxy()));

  OleInitialize(NULL);

  // Run a HostConfigurerWindow.
  remoting::HostConfigurerWindow host_configurer_window(
      url_request_context_getter_, message_loop.message_loop_proxy(),
      message_loop.QuitClosure());
  host_configurer_window.Create(NULL);

  base::RunLoop run_loop;
  run_loop.Run();

  io_thread.Stop();

  OleUninitialize();
}
