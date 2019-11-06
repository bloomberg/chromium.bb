// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_linux.h"

#include <memory>

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "ui/views/linux_ui/linux_ui.h"

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewLinux, public:

OpaqueBrowserFrameViewLinux::OpaqueBrowserFrameViewLinux(
    OpaqueBrowserFrameView* view,
    OpaqueBrowserFrameViewLayout* layout)
    : view_(view), layout_(layout) {
  views::LinuxUI* ui = views::LinuxUI::instance();
  if (ui)
    ui->AddWindowButtonOrderObserver(this);
}

OpaqueBrowserFrameViewLinux::~OpaqueBrowserFrameViewLinux() {
  views::LinuxUI* ui = views::LinuxUI::instance();
  if (ui)
    ui->RemoveWindowButtonOrderObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewLinux,
//     views::WindowButtonOrderObserver implementation:

void OpaqueBrowserFrameViewLinux::OnWindowButtonOrderingChange(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  layout_->SetButtonOrdering(leading_buttons, trailing_buttons);

  // We can receive OnWindowButtonOrderingChange events before we've been added
  // to a Widget. We need a Widget because layout crashes due to dependencies
  // on a ui::ThemeProvider().
  if (view_->GetWidget()) {
    // A relayout on |view_| is insufficient because it would neglect
    // a relayout of the tabstrip.  Do a full relayout to handle the
    // frame buttons as well as open tabs.
    views::View* root_view = view_->GetWidget()->GetRootView();
    root_view->Layout();
    root_view->SchedulePaint();
  }
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewObserver:

// static
std::unique_ptr<OpaqueBrowserFrameViewPlatformSpecific>
OpaqueBrowserFrameViewPlatformSpecific::Create(
    OpaqueBrowserFrameView* view,
    OpaqueBrowserFrameViewLayout* layout) {
  return std::make_unique<OpaqueBrowserFrameViewLinux>(view, layout);
}
