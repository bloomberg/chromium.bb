// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/message_loop.h"
#include "ui/compositor/test/compositor_test_support.h"
#include "ui/viewer/viewer_process.h"

// Mainline routine for running as the viewer process.
int ViewerProcessMain() {
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
  ui::CompositorTestSupport::Initialize();

  main_message_loop.set_thread_name("MainThread");
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  VLOG(1) << "Viewer process launched: "
          << CommandLine::ForCurrentProcess()->GetCommandLineString();

  base::PlatformThread::SetName("CrViewerMain");

  // TODO(scottmg): Uniquize this process?

  ViewerProcess viewer_process;
  if (viewer_process.Initialize(&main_message_loop, *command_line)) {
    MessageLoopForUI::current()->Run();
  } else {
    LOG(ERROR) << "Viewer process failed to initialize";
  }
  viewer_process.Teardown();
  return 0;
}
