// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/fake_screen_capturer.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

// FakeScreenCapturer generates a white picture of size kWidth x kHeight
// with a rectangle of size kBoxWidth x kBoxHeight. The rectangle moves kSpeed
// pixels per frame along both axes, and bounces off the sides of the screen.
static const int kWidth = FakeScreenCapturer::kWidth;
static const int kHeight = FakeScreenCapturer::kHeight;
static const int kBoxWidth = 140;
static const int kBoxHeight = 140;
static const int kSpeed = 20;

COMPILE_ASSERT(kBoxWidth < kWidth && kBoxHeight < kHeight, bad_box_size);
COMPILE_ASSERT((kBoxWidth % kSpeed == 0) && (kWidth % kSpeed == 0) &&
               (kBoxHeight % kSpeed == 0) && (kHeight % kSpeed == 0),
               sizes_must_be_multiple_of_kSpeed);

FakeScreenCapturer::FakeScreenCapturer()
    : callback_(NULL),
      mouse_shape_observer_(NULL),
      bytes_per_row_(0),
      box_pos_x_(0),
      box_pos_y_(0),
      box_speed_x_(kSpeed),
      box_speed_y_(kSpeed) {
  ScreenConfigurationChanged();
}

FakeScreenCapturer::~FakeScreenCapturer() {
}

void FakeScreenCapturer::Start(Callback* callback) {
  DCHECK(!callback_);
  DCHECK(callback);
  callback_ = callback;
}

void FakeScreenCapturer::Capture(const webrtc::DesktopRegion& region) {
  base::Time capture_start_time = base::Time::Now();

  queue_.MoveToNextFrame();

  if (!queue_.current_frame()) {
    int buffer_size = size_.height() * bytes_per_row_;
    webrtc::SharedMemory* shared_memory =
        callback_->CreateSharedMemory(buffer_size);
    scoped_ptr<webrtc::DesktopFrame> frame;
    webrtc::DesktopSize frame_size(size_.width(), size_.height());
    if (shared_memory) {
      frame.reset(new webrtc::SharedMemoryDesktopFrame(
          frame_size, bytes_per_row_, shared_memory));
    } else {
      frame.reset(new webrtc::BasicDesktopFrame(frame_size));
    }
    queue_.ReplaceCurrentFrame(frame.release());
  }

  DCHECK(queue_.current_frame());
  GenerateImage();

  queue_.current_frame()->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeSize(size_));
  queue_.current_frame()->set_capture_time_ms(
      (base::Time::Now() - capture_start_time).InMillisecondsRoundedUp());

  callback_->OnCaptureCompleted(queue_.current_frame()->Share());
}

void FakeScreenCapturer::SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) {
  DCHECK(!mouse_shape_observer_);
  DCHECK(mouse_shape_observer);
  mouse_shape_observer_ = mouse_shape_observer;
}

bool FakeScreenCapturer::GetScreenList(ScreenList* screens) {
  NOTIMPLEMENTED();
  return false;
}

bool FakeScreenCapturer::SelectScreen(webrtc::ScreenId id) {
  NOTIMPLEMENTED();
  return false;
}

void FakeScreenCapturer::GenerateImage() {
  webrtc::DesktopFrame* frame = queue_.current_frame();

  const int kBytesPerPixel = webrtc::DesktopFrame::kBytesPerPixel;

  memset(frame->data(), 0xff,
         size_.width() * size_.height() * kBytesPerPixel);

  uint8* row = frame->data() +
      (box_pos_y_ * size_.width() + box_pos_x_) * kBytesPerPixel;

  box_pos_x_ += box_speed_x_;
  if (box_pos_x_ + kBoxWidth >= size_.width() || box_pos_x_ == 0)
    box_speed_x_ = -box_speed_x_;

  box_pos_y_ += box_speed_y_;
  if (box_pos_y_ + kBoxHeight >= size_.height() || box_pos_y_ == 0)
    box_speed_y_ = -box_speed_y_;

  // Draw rectangle with the following colors in its corners:
  //     cyan....yellow
  //     ..............
  //     blue.......red
  for (int y = 0; y < kBoxHeight; ++y) {
    for (int x = 0; x < kBoxWidth; ++x) {
      int r = x * 255 / kBoxWidth;
      int g = y * 255 / kBoxHeight;
      int b = 255 - (x * 255 / kBoxWidth);
      row[x * kBytesPerPixel] = r;
      row[x * kBytesPerPixel + 1] = g;
      row[x * kBytesPerPixel + 2] = b;
      row[x * kBytesPerPixel + 3] = 0xff;
    }
    row += bytes_per_row_;
  }
}

void FakeScreenCapturer::ScreenConfigurationChanged() {
  size_.set(kWidth, kHeight);
  queue_.Reset();
  bytes_per_row_ = size_.width() * webrtc::DesktopFrame::kBytesPerPixel;
}

}  // namespace remoting
