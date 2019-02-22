// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller_views.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views_bridge_mac/bridged_native_widget_impl.h"

@implementation FullscreenToolbarControllerViews

- (id)initWithBrowserView:(BrowserView*)browserView {
  if ((self = [super initWithDelegate:self]))
    browserView_ = browserView;

  return self;
}

- (void)layoutToolbar {
  browserView_->Layout();
  [super layoutToolbar];
}

- (BOOL)isInAnyFullscreenMode {
  return browserView_->IsFullscreen();
}

- (BOOL)isFullscreenTransitionInProgress {
  views::BridgedNativeWidgetImpl* bridge_widget =
      views::BridgedNativeWidgetImpl::GetFromNativeWindow([self window]);
  return bridge_widget->in_fullscreen_transition();
}

- (NSWindow*)window {
  NSWindow* ns_window = browserView_->GetNativeWindow();
  if (!ns_view_) {
    ns_view_.reset(
        [views::BridgedNativeWidgetImpl::GetFromNativeWindow(ns_window)
                ->ns_view() retain]);
  }
  return ns_window;
}

- (BOOL)isInImmersiveFullscreen {
  // TODO: support immersive fullscreen mode https://crbug.com/863047.
  return false;
}

@end
