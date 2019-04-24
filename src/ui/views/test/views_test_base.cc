// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_base.h"

#include <utility>

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/views/test/platform_test_helper.h"
#include "ui/views/test/test_platform_native_widget.h"

#if BUILDFLAG(ENABLE_MUS)
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "services/ws/common/switches.h"  // nogncheck
#include "ui/aura/env.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/views/test/platform_test_helper_mus.h"
#include "ui/views/views_delegate.h"
#endif

#if defined(USE_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"
#elif defined(OS_MACOSX)
#include "ui/views/widget/native_widget_mac.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util_internal.h"
#endif

namespace views {

namespace {

bool InitializeVisuals() {
#if defined(USE_X11)
  bool has_compositing_manager = false;
  int depth = 0;
  bool using_argb_visual;

  if (depth > 0)
    return has_compositing_manager;

  // testing/xvfb.py runs xvfb and xcompmgr.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  has_compositing_manager = env->HasVar("_CHROMIUM_INSIDE_XVFB");
  ui::XVisualManager::GetInstance()->ChooseVisualForWindow(
      has_compositing_manager, nullptr, &depth, nullptr, &using_argb_visual);

  if (using_argb_visual)
    EXPECT_EQ(32, depth);

  return using_argb_visual;
#else
  return false;
#endif
}

#if BUILDFLAG(ENABLE_MUS)
std::unique_ptr<PlatformTestHelper> CreatePlatformTestHelper() {
  return std::make_unique<PlatformTestHelperMus>();
}
#endif  // BUILDFLAG(ENABLE_MUS)

}  // namespace

ViewsTestBase::ViewsTestBase() = default;

ViewsTestBase::~ViewsTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called super class's SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called super class's TearDown";
}

void ViewsTestBase::SetUp() {
  if (!scoped_task_environment_) {
    scoped_task_environment_ = std::make_unique<ScopedTaskEnvironment>(
        ScopedTaskEnvironment::MainThreadType::UI);
  }

  has_compositing_manager_ = InitializeVisuals();

#if BUILDFLAG(ENABLE_MUS)
  at_exit_manager_ = std::make_unique<base::ShadowingAtExitManager>();
  if (!aura::Env::HasInstance()) {
    env_ = aura::Env::CreateInstance(is_mus() ? aura::Env::Mode::MUS
                                              : aura::Env::Mode::LOCAL);
  }
#endif

  testing::Test::SetUp();
  ui::MaterialDesignController::Initialize();
  setup_called_ = true;
  if (!views_delegate_for_setup_)
    views_delegate_for_setup_.reset(new TestViewsDelegate());

#if BUILDFLAG(ENABLE_MUS)
  if (is_mus()) {
    // Set the mash feature flag, but don't override single process mash.
    if (!features::IsSingleProcessMash()) {
      feature_list_.InitAndEnableFeature(features::kMash);
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kEnableFeatures, features::kMash.name);
    }

    PlatformTestHelper::set_factory(
        base::BindRepeating(&CreatePlatformTestHelper));

    mojo::core::Init();
    ipc_thread_ = std::make_unique<base::Thread>("IPC thread");
    ipc_thread_->StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
    ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
        ipc_thread_->task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
  }
#else
  if (native_widget_type_ == NativeWidgetType::kDesktop) {
    ViewsDelegate::GetInstance()->set_native_widget_factory(base::BindRepeating(
        &ViewsTestBase::CreateNativeWidgetForTest, base::Unretained(this)));
  }
#endif

  test_helper_.reset(
      new ScopedViewsTestHelper(std::move(views_delegate_for_setup_)));
}

void ViewsTestBase::TearDown() {
  if (interactive_setup_called_)
    ui::ResourceBundle::CleanupSharedInstance();
  ui::Clipboard::DestroyClipboardForCurrentThread();

  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunPendingMessages();
  teardown_called_ = true;
  testing::Test::TearDown();
  test_helper_.reset();

#if BUILDFLAG(ENABLE_MUS)
  ipc_support_.reset();
  ipc_thread_.reset();
  if (is_mus())
    PlatformTestHelper::set_factory({});
  env_.reset();
  at_exit_manager_.reset();
#endif
}

void ViewsTestBase::SetUpForInteractiveTests() {
  DCHECK(!setup_called_);
  interactive_setup_called_ = true;

#if BUILDFLAG(ENABLE_MUS)
  bool init_mojo = !is_mus();
#else
  bool init_mojo = true;
#endif
  if (init_mojo) {
    // Mojo is initialized here similar to how each browser test case
    // initializes Mojo when starting. This only works because each
    // interactive_ui_test runs in a new process.
    mojo::core::Init();
  }

  gl::GLSurfaceTestSupport::InitializeOneOff();
  ui::RegisterPathProvider();
  base::FilePath ui_test_pak_path;
  ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
}

void ViewsTestBase::RunPendingMessages() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

Widget::InitParams ViewsTestBase::CreateParams(
    Widget::InitParams::Type type) {
  Widget::InitParams params(type);
  params.context = GetContext();
  return params;
}

bool ViewsTestBase::HasCompositingManager() const {
  return has_compositing_manager_;
}

void ViewsTestBase::SimulateNativeDestroy(Widget* widget) {
  test_helper_->platform_test_helper()->SimulateNativeDestroy(widget);
}

gfx::NativeWindow ViewsTestBase::GetContext() {
  return test_helper_->GetContext();
}

NativeWidget* ViewsTestBase::CreateNativeWidgetForTest(
    const Widget::InitParams& init_params,
    internal::NativeWidgetDelegate* delegate) {
#if defined(OS_MACOSX)
  return new test::TestPlatformNativeWidget<NativeWidgetMac>(delegate, false,
                                                             nullptr);
#elif defined(USE_AURA)
  // For widgets that have a modal parent, don't force a native widget type.
  // This logic matches DesktopTestViewsDelegate as well as ChromeViewsDelegate.
  if (init_params.parent &&
      init_params.type != views::Widget::InitParams::TYPE_MENU &&
      init_params.type != views::Widget::InitParams::TYPE_TOOLTIP) {
    // Returning null results in using the platform default, which is
    // NativeWidgetAura.
    return nullptr;
  }

  if (native_widget_type_ == NativeWidgetType::kDesktop) {
    return new test::TestPlatformNativeWidget<DesktopNativeWidgetAura>(
        delegate, false, nullptr);
  }

  return new test::TestPlatformNativeWidget<NativeWidgetAura>(delegate, true,
                                                              nullptr);
#else
  NOTREACHED();
  return nullptr;
#endif
}

void ViewsTestWithDesktopNativeWidget::SetUp() {
  set_native_widget_type(NativeWidgetType::kDesktop);
  ViewsTestBase::SetUp();
}

}  // namespace views
