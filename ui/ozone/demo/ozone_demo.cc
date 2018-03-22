// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/trace_event/trace_event.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/demo/window_manager.h"
#include "ui/ozone/public/ozone_platform.h"

const char kHelp[] = "help";

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;

  base::debug::EnableInProcessStackDumping();

  // Initialize logging so we can enable VLOG messages.
  logging::LoggingSettings settings;
  logging::InitLogging(settings);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kHelp)) {
    std::cout <<
        "Usage:\n\n"
        "  --enable-drm-atomic         Use the atomic KMS API\n"
        "  --disable-gpu               Force software rendering\n"
        "  --disable-surfaceless       Don't use surfaceless EGL\n"
        "  --window-size=WIDTHxHEIGHT  Specify window size\n"
        "  --use-ddl                   Use SkDeferredDisplayList\n"
        "  --partial-primary-plane     "
        "Use smaller than fullscreen primary plane\n"
        "  --enable-overlay            Use an overlay plane\n"
        "  --disable-primary-plane     Don't use the primary plane\n";

    // TODO(hoegsberg): We should add a little more help text about how these
    // options interact and depend on each other.

    exit(EXIT_SUCCESS);
  }

  // Initialize tracing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTraceToConsole)) {
    base::trace_event::TraceConfig trace_config =
        tracing::GetConfigForTraceToConsole();
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        trace_config, base::trace_event::TraceLog::RECORDING_MODE);
  }

  // Build UI thread message loop. This is used by platform
  // implementations for event polling & running background tasks.
  base::MessageLoopForUI message_loop;
  base::TaskScheduler::CreateAndStartWithDefaultParams("OzoneDemo");

  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForUI(params);
  ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()
      ->SetCurrentLayoutByName("us");

  base::RunLoop run_loop;

  ui::WindowManager window_manager(run_loop.QuitClosure());

  run_loop.Run();

  return 0;
}
