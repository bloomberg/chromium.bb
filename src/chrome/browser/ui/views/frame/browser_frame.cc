// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/i18n/rtl.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"
#include "chrome/browser/ui/views/frame/system_menu_model_builder.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/hit_test.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/native_widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/ui/wm/desks/desks_helper.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user_manager.h"
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/display/screen.h"
#endif

#if defined(OS_LINUX)
#include "ui/views/linux_ui/linux_ui.h"
#endif

namespace {

bool IsUsingGtkTheme(Profile* profile) {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return ThemeServiceFactory::GetForProfile(profile)->UsingSystemTheme();
#else
  return false;
#endif
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

BrowserFrame::BrowserFrame(BrowserView* browser_view)
    : native_browser_frame_(nullptr),
      root_view_(nullptr),
      browser_frame_view_(nullptr),
      browser_view_(browser_view) {
  browser_view_->set_frame(this);
  set_is_secondary_widget(false);
  // Don't focus anything on creation, selecting a tab will set the focus.
  set_focus_on_creation(false);
}

BrowserFrame::~BrowserFrame() {}

void BrowserFrame::InitBrowserFrame() {
  native_browser_frame_ =
      NativeBrowserFrameFactory::CreateNativeBrowserFrame(this, browser_view_);
  views::Widget::InitParams params = native_browser_frame_->GetWidgetParams();
  params.name = "BrowserFrame";
  params.delegate = browser_view_;

  if (native_browser_frame_->ShouldRestorePreviousBrowserWidgetState()) {
    Browser* browser = browser_view_->browser();
    if (browser->is_type_normal() || browser->is_type_devtools() ||
        browser->is_type_app()) {
      // Typed panel/popup can only return a size once the widget has been
      // created.
      // DevTools counts as a popup, but DevToolsWindow::CreateDevToolsBrowser
      // ensures there is always a size available. Without this, the tools
      // launch on the wrong display and can have sizing issues when
      // repositioned to the saved bounds in Widget::SetInitialBounds.
      chrome::GetSavedWindowBoundsAndShowState(browser, &params.bounds,
                                               &params.show_state);

      params.workspace = browser->initial_workspace();
      params.visible_on_all_workspaces =
          browser->initial_visible_on_all_workspaces_state();
      const base::CommandLine& parsed_command_line =
          *base::CommandLine::ForCurrentProcess();

      if (parsed_command_line.HasSwitch(switches::kWindowWorkspace)) {
        params.workspace =
            parsed_command_line.GetSwitchValueASCII(switches::kWindowWorkspace);
      }
    }
  }

  Init(std::move(params));
  SelectNativeTheme();

  if (!native_browser_frame_->UsesNativeSystemMenu()) {
    DCHECK(non_client_view());
    non_client_view()->set_context_menu_controller(this);
  }
}

int BrowserFrame::GetMinimizeButtonOffset() const {
  return native_browser_frame_->GetMinimizeButtonOffset();
}

gfx::Rect BrowserFrame::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  // This can be invoked before |browser_frame_view_| has been set.
  return browser_frame_view_ ? browser_frame_view_->GetBoundsForTabStripRegion(
                                   tabstrip_minimum_size)
                             : gfx::Rect();
}

int BrowserFrame::GetTopInset() const {
  return browser_frame_view_->GetTopInset(false);
}

int BrowserFrame::GetThemeBackgroundXInset() const {
  return browser_frame_view_->GetThemeBackgroundXInset();
}

void BrowserFrame::UpdateThrobber(bool running) {
  browser_frame_view_->UpdateThrobber(running);
}

BrowserNonClientFrameView* BrowserFrame::GetFrameView() const {
  return browser_frame_view_;
}

bool BrowserFrame::UseCustomFrame() const {
  return native_browser_frame_->UseCustomFrame();
}

bool BrowserFrame::ShouldSaveWindowPlacement() const {
  return native_browser_frame_->ShouldSaveWindowPlacement();
}

bool BrowserFrame::ShouldDrawFrameHeader() const {
  return true;
}

void BrowserFrame::GetWindowPlacement(gfx::Rect* bounds,
                                      ui::WindowShowState* show_state) const {
  return native_browser_frame_->GetWindowPlacement(bounds, show_state);
}

content::KeyboardEventProcessingResult BrowserFrame::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return native_browser_frame_->PreHandleKeyboardEvent(event);
}

bool BrowserFrame::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return native_browser_frame_->HandleKeyboardEvent(event);
}

void BrowserFrame::OnBrowserViewInitViewsComplete() {
  browser_frame_view_->OnBrowserViewInitViewsComplete();
}

void BrowserFrame::UserChangedTheme(BrowserThemeChangeType theme_change_type) {
  // kWebAppTheme is triggered by web apps and will only change colors, not the
  // frame type; just refresh the theme on all views in the browser window.
  if (theme_change_type == BrowserThemeChangeType::kWebAppTheme) {
    ThemeChanged();
    return;
  }

  // When the browser theme changes, the NativeTheme may also change.
  // In Incognito, the usage of dark or normal hinges on the browser theme.
  if (theme_change_type == BrowserThemeChangeType::kBrowserTheme)
    SelectNativeTheme();

  if (!RegenerateFrameOnThemeChange(theme_change_type)) {
    // If RegenerateFrame() returns true, ThemeChanged() was implicitly called,
    // so no need to call it explicitly.
    ThemeChanged();
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, views::Widget overrides:

views::internal::RootView* BrowserFrame::CreateRootView() {
  root_view_ = new BrowserRootView(browser_view_, this);
  return root_view_;
}

std::unique_ptr<views::NonClientFrameView>
BrowserFrame::CreateNonClientFrameView() {
  auto browser_frame_view =
      chrome::CreateBrowserNonClientFrameView(this, browser_view_);
  browser_frame_view_ = browser_frame_view.get();
  return browser_frame_view;
}

bool BrowserFrame::GetAccelerator(int command_id,
                                  ui::Accelerator* accelerator) const {
  return browser_view_->GetAccelerator(command_id, accelerator);
}

const ui::ThemeProvider* BrowserFrame::GetThemeProvider() const {
  Browser* browser = browser_view_->browser();
  auto* app_controller = browser->app_controller();
  // Ignore GTK+ for web apps with window-controls-overlay as the
  // display_override so the web contents can blend with the overlay by using
  // the developer-provided theme color for a better experience. Context:
  // https://crbug.com/1219073.
  if (app_controller && (!IsUsingGtkTheme(browser->profile()) ||
                         app_controller->AppUsesWindowControlsOverlay())) {
    return app_controller->GetThemeProvider();
  }
  return &ThemeService::GetThemeProviderForProfile(browser->profile());
}

ui::ColorProviderManager::InitializerSupplier* BrowserFrame::GetCustomTheme()
    const {
  Browser* browser = browser_view_->browser();
  auto* app_controller = browser->app_controller();
  // Ignore GTK+ for web apps with window-controls-overlay as the
  // display_override so the web contents can blend with the overlay by using
  // the developer-provided theme color for a better experience. Context:
  // https://crbug.com/1219073.
  if (app_controller && (!IsUsingGtkTheme(browser->profile()) ||
                         app_controller->AppUsesWindowControlsOverlay())) {
    return app_controller->GetThemeSupplier();
  }
  return ThemeService::GetThemeSupplierForProfile(browser->profile());
}

void BrowserFrame::OnNativeWidgetWorkspaceChanged() {
  chrome::SaveWindowWorkspace(browser_view_->browser(), GetWorkspace());
  chrome::SaveWindowVisibleOnAllWorkspaces(browser_view_->browser(),
                                           IsVisibleOnAllWorkspaces());
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // If the window was sent to a different workspace, prioritize it if
  // it was sent to the current workspace and deprioritize it
  // otherwise.  This is done by MoveBrowsersInWorkspaceToFront()
  // which reorders the browsers such that the ones in the current
  // workspace appear before ones in other workspaces.
  auto workspace = display::Screen::GetScreen()->GetCurrentWorkspace();
  if (!workspace.empty())
    BrowserList::MoveBrowsersInWorkspaceToFront(workspace);
#endif
  Widget::OnNativeWidgetWorkspaceChanged();
}

void BrowserFrame::ShowContextMenuForViewImpl(views::View* source,
                                              const gfx::Point& p,
                                              ui::MenuSourceType source_type) {
  if (chrome::IsRunningInForcedAppMode())
    return;

  // Only show context menu if point is in unobscured parts of browser, i.e.
  // if NonClientHitTest returns :
  // - HTCAPTION: in title bar or unobscured part of tabstrip
  // - HTNOWHERE: as the name implies.
  gfx::Point point_in_view_coords(p);
  views::View::ConvertPointFromScreen(non_client_view(), &point_in_view_coords);
  int hit_test = non_client_view()->NonClientHitTest(point_in_view_coords);
  if (hit_test == HTCAPTION || hit_test == HTNOWHERE) {
    menu_runner_ = std::make_unique<views::MenuRunner>(
        GetSystemMenuModel(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
        base::BindRepeating(&BrowserFrame::OnMenuClosed,
                            base::Unretained(this)));
    menu_runner_->RunMenuAt(source->GetWidget(), nullptr,
                            gfx::Rect(p, gfx::Size(0, 0)),
                            views::MenuAnchorPosition::kTopLeft, source_type);
  }
}

ui::MenuModel* BrowserFrame::GetSystemMenuModel() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->GetLoggedInUsers().size() > 1) {
    // In Multi user mode, the number of users as well as the order of users
    // can change. Coming here we have more than one user and since the menu
    // model contains the user information, it must get updated to show any
    // changes happened since the last invocation.
    menu_model_builder_.reset();
  }
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* desks_helper = chromeos::DesksHelper::Get(GetNativeWindow());
  int current_num_desks = desks_helper ? desks_helper->GetNumberOfDesks() : -1;
  if (current_num_desks != num_desks_) {
    // Since the number of desks can change, the model must update to show any
    // changes happened since the last invocation.
    menu_model_builder_.reset();
    num_desks_ = current_num_desks;
  }
#endif
  if (!menu_model_builder_.get()) {
    menu_model_builder_ = std::make_unique<SystemMenuModelBuilder>(
        browser_view_, browser_view_->browser());
    menu_model_builder_->Init();
  }
  return menu_model_builder_->menu_model();
}

void BrowserFrame::SetTabDragKind(TabDragKind tab_drag_kind) {
  if (tab_drag_kind_ == tab_drag_kind)
    return;

  if (native_browser_frame_)
    native_browser_frame_->TabDraggingKindChanged(tab_drag_kind);

  bool was_dragging_any = tab_drag_kind_ != TabDragKind::kNone;
  bool is_dragging_any = tab_drag_kind != TabDragKind::kNone;
  if (was_dragging_any != is_dragging_any)
    browser_view_->TabDraggingStatusChanged(is_dragging_any);

  tab_drag_kind_ = tab_drag_kind;
}

void BrowserFrame::OnMenuClosed() {
  menu_runner_.reset();
}

void BrowserFrame::OnTouchUiChanged() {
  client_view()->InvalidateLayout();

  // For standard browser frame, if we do not invalidate the NonClientFrameView
  // the client window bounds will not be properly updated which could cause
  // visual artifacts. See crbug.com/1035959 for details.
  if (non_client_view()->frame_view()) {
    // Note that invalidating a view invalidates all of its ancestors, so it is
    // not necessary to also invalidate the NonClientView or RootView here.
    non_client_view()->frame_view()->InvalidateLayout();
  } else {
    non_client_view()->InvalidateLayout();
  }
  GetRootView()->Layout();
}

void BrowserFrame::SelectNativeTheme() {
  // Select between regular, dark and GTK theme.
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();

  if (browser_view_->browser()->profile()->IsIncognitoProfile()) {
    // If the flag is enabled, then no matter if we are using the default theme
    // or not we always use the dark ui instance.
    if (base::FeatureList::IsEnabled(
            features::kIncognitoBrandConsistencyForDesktop)) {
      SetNativeTheme(ui::NativeTheme::GetInstanceForDarkUI());
      return;
    }

    // Flag is disabled, fallback to using dark theme only if the incognito
    // profile is using a default theme.
    if (ThemeServiceFactory::GetForProfile(browser_view_->browser()->profile())
            ->UsingDefaultTheme()) {
      native_theme = ui::NativeTheme::GetInstanceForDarkUI();
    }
  }

#if defined(OS_LINUX)
  const views::LinuxUI* linux_ui = views::LinuxUI::instance();
  // Ignore GTK+ for web apps with window-controls-overlay as the
  // display_override so the web contents can blend with the overlay by using
  // the developer-provided theme color for a better experience. Context:
  // https://crbug.com/1219073.
  if (linux_ui && !browser_view_->AppUsesWindowControlsOverlay()) {
    native_theme = linux_ui->GetNativeTheme(GetNativeWindow());
  }
#endif

  SetNativeTheme(native_theme);
}

bool BrowserFrame::RegenerateFrameOnThemeChange(
    BrowserThemeChangeType theme_change_type) {
  bool need_regenerate = false;
  // TODO(crbug.com/1052397): Revisit the macro expression once build flag
  // switch of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // GTK and user theme changes can both change frame buttons, so the frame
  // always needs to be regenerated on Linux.
  need_regenerate = true;
#endif

#if defined(OS_WIN)
  // On Windows, DWM transition does not performed for a frame regeneration in
  // fullscreen mode, so do a lighweight theme change to refresh a bookmark bar
  // on new tab. (see crbug/1002480)
  need_regenerate |=
      theme_change_type == BrowserThemeChangeType::kBrowserTheme &&
      !IsFullscreen();
#else
  need_regenerate |= theme_change_type == BrowserThemeChangeType::kBrowserTheme;
#endif

  if (need_regenerate) {
    // This is a heavyweight theme change that requires regenerating the frame
    // as well as repainting the browser window.
    // No need to call ThemeChanged(). It will be implicitly called by
    // FrameTypeChanged().
    FrameTypeChanged();
    return true;
  }

  return false;
}
