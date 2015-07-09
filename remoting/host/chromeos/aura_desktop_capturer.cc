// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/aura_desktop_capturer.h"

#include "base/bind.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "remoting/host/chromeos/skia_bitmap_desktop_frame.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

namespace remoting {

AuraDesktopCapturer::AuraDesktopCapturer()
    : callback_(nullptr), desktop_window_(nullptr), weak_factory_(this) {
}

AuraDesktopCapturer::~AuraDesktopCapturer() {
}

void AuraDesktopCapturer::Start(webrtc::DesktopCapturer::Callback* callback) {
#if defined(USE_ASH)
  if (ash::Shell::HasInstance()) {
    // TODO(kelvinp): Use ash::Shell::GetAllRootWindows() when multiple monitor
    // support is implemented.
    desktop_window_ = ash::Shell::GetPrimaryRootWindow();
    DCHECK(desktop_window_) << "Failed to retrieve the Aura Shell root window";
  }
#endif

  DCHECK(!callback_) << "Start() can only be called once";
  callback_ = callback;
  DCHECK(callback_);
}

void AuraDesktopCapturer::Capture(const webrtc::DesktopRegion&) {
  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateBitmapRequest(
          base::Bind(
              &AuraDesktopCapturer::OnFrameCaptured,
              weak_factory_.GetWeakPtr()));

  gfx::Rect window_rect(desktop_window_->bounds().size());

  request->set_area(window_rect);
  desktop_window_->layer()->RequestCopyOfOutput(request.Pass());
}

void AuraDesktopCapturer::OnFrameCaptured(
    scoped_ptr<cc::CopyOutputResult> result) {
  if (result->IsEmpty()) {
    callback_->OnCaptureCompleted(nullptr);
    return;
  }

  DCHECK(result->HasBitmap());

  scoped_ptr<SkBitmap> bitmap = result->TakeBitmap();

  scoped_ptr<webrtc::DesktopFrame> frame(
      SkiaBitmapDesktopFrame::Create(bitmap.Pass()));

  // |VideoFramePump| will not encode the frame if |updated_region| is empty.
  const webrtc::DesktopRect& rect = webrtc::DesktopRect::MakeWH(
      frame->size().width(), frame->size().height());

  // TODO(kelvinp): Set Frame DPI according to the screen resolution.
  // See cc::Layer::contents_scale_(x|y)() and frame->set_depi().
  frame->mutable_updated_region()->SetRect(rect);

  callback_->OnCaptureCompleted(frame.release());
}

}  // namespace remoting
