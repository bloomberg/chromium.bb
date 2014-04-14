// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(USE_X11)
#include <X11/Xlib.h>
#endif

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/gfx/screen.h"
#include "ui/gl/gl_surface.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/wm/core/wm_state.h"

#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif
#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

int main(int argc, char** argv) {
#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  CommandLine::Init(argc, argv);

  base::AtExitManager at_exit;

#if defined(USE_X11)
  // This demo uses InProcessContextFactory which uses X on a separate Gpu
  // thread.
  XInitThreads();
#endif

  gfx::GLSurface::InitializeOneOff();

  // The ContextFactory must exist before any Compositors are created.
  scoped_ptr<ui::InProcessContextFactory> context_factory(
      new ui::InProcessContextFactory());
  ui::ContextFactory::SetInstance(context_factory.get());

  base::MessageLoopForUI message_loop;

  base::i18n::InitializeICU();

  base::FilePath pak_dir;
  PathService::Get(base::DIR_MODULE, &pak_dir);

  base::FilePath pak_file;
  pak_file = pak_dir.Append(FILE_PATH_LITERAL("ui_test.pak"));

  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);

  aura::Env::CreateInstance();

  ui::InitializeInputMethodForTesting();

  {
    views::DesktopTestViewsDelegate views_delegate;
    wm::WMState wm_state;

#if !defined(OS_CHROMEOS)
    scoped_ptr<gfx::Screen> desktop_screen(views::CreateDesktopScreen());
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                   desktop_screen.get());
#endif

    views::examples::ShowExamplesWindow(
        views::examples::QUIT_ON_CLOSE,
        NULL,
        scoped_ptr<ScopedVector<views::examples::ExampleBase> >());

    base::RunLoop().Run();

    ui::ResourceBundle::CleanupSharedInstance();
  }

  ui::ShutdownInputMethod();

  aura::Env::DeleteInstance();

  return 0;
}
