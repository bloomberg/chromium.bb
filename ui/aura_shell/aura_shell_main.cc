// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/aura/desktop.h"
#include "ui/aura/desktop_host.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/rect.h"
#include "views/widget/widget.h"
#include "views/widget/widget_delegate.h"

#if !defined(OS_WIN)
#include "ui/aura/hit_test.h"
#endif

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  icu_util::Initialize();
  ResourceBundle::InitSharedInstance("en-US");

#if defined(USE_X11)
  base::MessagePumpX::DisableGtkMessagePump();
#endif

  // Create the message-loop here before creating the desktop.
  MessageLoop message_loop(MessageLoop::TYPE_UI);

  aura::Desktop::GetInstance()->Run();

  delete aura::Desktop::GetInstance();

  return 0;
}

