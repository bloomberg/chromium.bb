// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/views/linux_ui/linux_ui.h"
#endif

#if BUILDFLAG(ENABLE_NATIVE_WINDOW_NAV_BUTTONS)
#include "chrome/browser/ui/views/frame/desktop_linux_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/desktop_linux_browser_frame_view_layout.h"
#include "chrome/browser/ui/views/nav_button_provider.h"
#endif

namespace chrome {

namespace {

OpaqueBrowserFrameView* CreateOpaqueBrowserFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view) {
#if BUILDFLAG(ENABLE_NATIVE_WINDOW_NAV_BUTTONS)
  std::unique_ptr<views::NavButtonProvider> nav_button_provider;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (ThemeServiceFactory::GetForProfile(browser_view->browser()->profile())
          ->UsingSystemTheme() &&
      views::LinuxUI::instance()) {
    nav_button_provider = views::LinuxUI::instance()->CreateNavButtonProvider();
  }
#endif
  if (nav_button_provider) {
    return new DesktopLinuxBrowserFrameView(
        frame, browser_view,
        new DesktopLinuxBrowserFrameViewLayout(nav_button_provider.get()),
        std::move(nav_button_provider));
  }
#endif
  return new OpaqueBrowserFrameView(frame, browser_view,
                                    new OpaqueBrowserFrameViewLayout());
}

}  // namespace

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view) {
#if defined(OS_WIN)
  if (frame->ShouldUseNativeFrame())
    return new GlassBrowserFrameView(frame, browser_view);
#endif
  OpaqueBrowserFrameView* view =
      CreateOpaqueBrowserFrameView(frame, browser_view);
  view->InitViews();
  return view;
}

}  // namespace chrome
