// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "build/build_config.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/display/screen.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/test/desktop_test_views_delegate.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/wm/core/wm_state.h"
#endif

#if !defined(OS_CHROMEOS) && defined(USE_AURA)
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#include "ui/gfx/win/direct_write.h"
#endif

#if defined(USE_X11)
#include "ui/gfx/x/x11_connection.h"  // nogncheck
#endif

base::LazyInstance<base::TestDiscardableMemoryAllocator>::DestructorAtExit
    g_discardable_memory_allocator = LAZY_INSTANCE_INITIALIZER;

int main(int argc, char** argv) {
#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  base::CommandLine::Init(argc, argv);

  base::AtExitManager at_exit;

#if defined(USE_X11)
  // This demo uses InProcessContextFactory which uses X on a separate Gpu
  // thread.
  gfx::InitializeThreadedX11();
#endif

  gl::init::InitializeGLOneOff();

  // The ContextFactory must exist before any Compositors are created.
  viz::HostFrameSinkManager host_frame_sink_manager_;
  viz::FrameSinkManagerImpl frame_sink_manager_;
  auto context_factory = base::MakeUnique<ui::InProcessContextFactory>(
      &host_frame_sink_manager_, &frame_sink_manager_);
  context_factory->set_use_test_surface(false);

  base::test::ScopedTaskEnvironment scoped_task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::UI);

  base::i18n::InitializeICU();

  ui::RegisterPathProvider();

  base::FilePath ui_test_pak_path;
  CHECK(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

  base::DiscardableMemoryAllocator::SetInstance(
      g_discardable_memory_allocator.Pointer());

  base::PowerMonitor power_monitor(
      base::WrapUnique(new base::PowerMonitorDeviceSource));

#if defined(OS_WIN)
  gfx::win::MaybeInitializeDirectWrite();
#endif

#if defined(USE_AURA)
  std::unique_ptr<aura::Env> env = aura::Env::CreateInstance();
  aura::Env::GetInstance()->set_context_factory(context_factory.get());
  aura::Env::GetInstance()->set_context_factory_private(context_factory.get());
#endif
  ui::InitializeInputMethodForTesting();
  ui::MaterialDesignController::Initialize();

  {
    views::DesktopTestViewsDelegate views_delegate;
#if defined(USE_AURA)
    wm::WMState wm_state;
#endif
#if !defined(OS_CHROMEOS) && defined(USE_AURA)
    std::unique_ptr<display::Screen> desktop_screen(
        views::CreateDesktopScreen());
    display::Screen::SetScreenInstance(desktop_screen.get());
#endif

    views::examples::ShowExamplesWindow(views::examples::QUIT_ON_CLOSE);

    base::RunLoop().Run();

    ui::ResourceBundle::CleanupSharedInstance();
  }

  ui::ShutdownInputMethod();

#if defined(USE_AURA)
  env.reset();
#endif

  return 0;
}
