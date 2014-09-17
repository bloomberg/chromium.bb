// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/aura_desktop_capturer.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace remoting {

namespace {

// DesktopFrame implementation used by screen capture on ChromeOS.
// Frame data is stored in a SkBitmap.
class SkiaBitmapDesktopFrame : public webrtc::DesktopFrame {
 public:
  static SkiaBitmapDesktopFrame* Create(scoped_ptr<SkBitmap> bitmap);
  virtual ~SkiaBitmapDesktopFrame();

 private:
  SkiaBitmapDesktopFrame(webrtc::DesktopSize size,
                         int stride,
                         uint8_t* data,
                         scoped_ptr<SkBitmap> bitmap);

  scoped_ptr<SkBitmap> bitmap_;

  DISALLOW_COPY_AND_ASSIGN(SkiaBitmapDesktopFrame);
};

// static
SkiaBitmapDesktopFrame* SkiaBitmapDesktopFrame::Create(
    scoped_ptr<SkBitmap> bitmap) {

  webrtc::DesktopSize size(bitmap->width(), bitmap->height());
  DCHECK_EQ(kRGBA_8888_SkColorType, bitmap->info().colorType())
      << "DesktopFrame objects always hold RGBA data.";

  uint8_t* bitmap_data = reinterpret_cast<uint8_t*>(bitmap->getPixels());

  SkiaBitmapDesktopFrame* result = new SkiaBitmapDesktopFrame(
      size, bitmap->rowBytes(), bitmap_data, bitmap.Pass());

  return result;
}

SkiaBitmapDesktopFrame::SkiaBitmapDesktopFrame(webrtc::DesktopSize size,
                                               int stride,
                                               uint8_t* data,
                                               scoped_ptr<SkBitmap> bitmap)
    : DesktopFrame(size, stride, data, NULL), bitmap_(bitmap.Pass()) {
}

SkiaBitmapDesktopFrame::~SkiaBitmapDesktopFrame() {
}

}  // namespace

AuraDesktopCapturer::AuraDesktopCapturer()
    : callback_(NULL), desktop_window_(NULL), weak_factory_(this) {
}

AuraDesktopCapturer::~AuraDesktopCapturer() {
}

void AuraDesktopCapturer::Start(webrtc::DesktopCapturer::Callback* callback) {
  if (ash::Shell::HasInstance()) {
    // TODO(kelvinp): Use ash::Shell::GetAllRootWindows() when multiple monitor
    // support is implemented.
    desktop_window_ = ash::Shell::GetPrimaryRootWindow();
    DCHECK(desktop_window_) << "Failed to retrieve the Aura Shell root window";
  }

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
  DCHECK(result->HasBitmap());

  scoped_ptr<SkBitmap> bitmap = result->TakeBitmap();

  scoped_ptr<webrtc::DesktopFrame> frame(
      SkiaBitmapDesktopFrame::Create(bitmap.Pass()));

  // |VideoScheduler| will not encode the frame if |updated_region| is empty.
  const webrtc::DesktopRect& rect = webrtc::DesktopRect::MakeWH(
      frame->size().width(), frame->size().height());

  // TODO(kelvinp): Set Frame DPI according to the screen resolution.
  // See cc::Layer::contents_scale_(x|y)() and frame->set_depi().
  frame->mutable_updated_region()->SetRect(rect);

  callback_->OnCaptureCompleted(frame.release());
}

}  // namespace remoting
