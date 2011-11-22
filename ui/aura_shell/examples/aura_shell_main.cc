// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "ui/aura/desktop.h"
#include "ui/aura_shell/examples/toplevel_window.h"
#include "ui/aura_shell/launcher/launcher_types.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/aura_shell/shell_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/test/compositor_test_support.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

class AppListWindow : public views::WidgetDelegateView {
 public:
  AppListWindow() {
  }

  // static
  static views::Widget* Create() {
    AppListWindow* app_list = new AppListWindow;

    views::Widget::InitParams widget_params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget_params.delegate = app_list;
    widget_params.keep_on_top = true;
    widget_params.transparent = true;

    views::Widget* widget = new views::Widget;
    widget->Init(widget_params);
    widget->SetContentsView(app_list);
    return widget;
  }

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(SkColorSetARGB(0x4F, 0xFF, 0, 0), bounds());
  }
};

class ShellDelegateImpl : public aura_shell::ShellDelegate {
 public:
  ShellDelegateImpl() {
  }

  virtual void CreateNewWindow() OVERRIDE {
    aura_shell::examples::ToplevelWindow::CreateParams create_params;
    create_params.can_resize = true;
    create_params.can_maximize = true;
    aura_shell::examples::ToplevelWindow::CreateToplevelWindow(create_params);
  }

  virtual views::Widget* CreateStatusArea() OVERRIDE {
    return aura_shell::internal::CreateStatusArea();
  }

  virtual void RequestAppListWidget(
      const SetWidgetCallback& callback) OVERRIDE {
    callback.Run(AppListWindow::Create());
  }

  virtual void LauncherItemClicked(
      const aura_shell::LauncherItem& item) OVERRIDE {
    item.window->Activate();
  }

  virtual bool ConfigureLauncherItem(aura_shell::LauncherItem* item) OVERRIDE {
    static int image_count = 0;
    item->tab_images.resize(image_count + 1);
    for (int i = 0; i < image_count + 1; ++i) {
      item->tab_images[i].image.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
      item->tab_images[i].image.allocPixels();
      item->tab_images[i].image.eraseARGB(255,
                                          i == 0 ? 255 : 0,
                                          i == 1 ? 255 : 0,
                                          i == 2 ? 255 : 0);
    }
    image_count = (image_count + 1) % 3;
    return true;  // Makes the entry show up in the launcher.
  }
};

}  // namespace

namespace aura_shell {
namespace examples {

void InitWindowTypeLauncher();

}  // namespace examples
}  // namespace aura_shell

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  icu_util::Initialize();
  ResourceBundle::InitSharedInstance("en-US");

  // Create the message-loop here before creating the desktop.
  MessageLoop message_loop(MessageLoop::TYPE_UI);
  ui::CompositorTestSupport::Initialize();

  aura_shell::Shell::CreateInstance(new ShellDelegateImpl);

  aura_shell::examples::InitWindowTypeLauncher();

  aura::Desktop::GetInstance()->Run();

  delete aura::Desktop::GetInstance();

  ui::CompositorTestSupport::Terminate();

  return 0;
}
