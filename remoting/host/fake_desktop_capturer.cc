// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/fake_desktop_capturer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

// FakeDesktopCapturer generates a white picture of size kWidth x kHeight
// with a rectangle of size kBoxWidth x kBoxHeight. The rectangle moves kSpeed
// pixels per frame along both axes, and bounces off the sides of the screen.
static const int kWidth = FakeDesktopCapturer::kWidth;
static const int kHeight = FakeDesktopCapturer::kHeight;
static const int kBoxWidth = 140;
static const int kBoxHeight = 140;
static const int kSpeed = 20;

COMPILE_ASSERT(kBoxWidth < kWidth && kBoxHeight < kHeight, bad_box_size);
COMPILE_ASSERT((kBoxWidth % kSpeed == 0) && (kWidth % kSpeed == 0) &&
               (kBoxHeight % kSpeed == 0) && (kHeight % kSpeed == 0),
               sizes_must_be_multiple_of_kSpeed);

namespace {

class DefaultFrameGenerator
    : public base::RefCountedThreadSafe<DefaultFrameGenerator> {
 public:
  DefaultFrameGenerator()
      : bytes_per_row_(0),
        box_pos_x_(0),
        box_pos_y_(0),
        box_speed_x_(kSpeed),
        box_speed_y_(kSpeed),
        first_frame_(true) {}

  scoped_ptr<webrtc::DesktopFrame> GenerateFrame(
      webrtc::DesktopCapturer::Callback* callback);

 private:
  friend class base::RefCountedThreadSafe<DefaultFrameGenerator>;
  ~DefaultFrameGenerator() {}

  webrtc::DesktopSize size_;
  int bytes_per_row_;
  int box_pos_x_;
  int box_pos_y_;
  int box_speed_x_;
  int box_speed_y_;
  bool first_frame_;

  DISALLOW_COPY_AND_ASSIGN(DefaultFrameGenerator);
};

scoped_ptr<webrtc::DesktopFrame> DefaultFrameGenerator::GenerateFrame(
    webrtc::DesktopCapturer::Callback* callback) {
  const int kBytesPerPixel = webrtc::DesktopFrame::kBytesPerPixel;
  int buffer_size = kWidth * kHeight * kBytesPerPixel;
  webrtc::SharedMemory* shared_memory =
      callback->CreateSharedMemory(buffer_size);
  scoped_ptr<webrtc::DesktopFrame> frame;
  if (shared_memory) {
    frame.reset(new webrtc::SharedMemoryDesktopFrame(
        webrtc::DesktopSize(kWidth, kHeight), bytes_per_row_, shared_memory));
  } else {
    frame.reset(
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(kWidth, kHeight)));
  }

  // Move the box.
  bool old_box_pos_x = box_pos_x_;
  box_pos_x_ += box_speed_x_;
  if (box_pos_x_ + kBoxWidth >= kWidth || box_pos_x_ == 0)
    box_speed_x_ = -box_speed_x_;

  bool old_box_pos_y = box_pos_y_;
  box_pos_y_ += box_speed_y_;
  if (box_pos_y_ + kBoxHeight >= kHeight || box_pos_y_ == 0)
    box_speed_y_ = -box_speed_y_;

  memset(frame->data(), 0xff, kHeight * frame->stride());

  // Draw rectangle with the following colors in its corners:
  //     cyan....yellow
  //     ..............
  //     blue.......red
  uint8* row = frame->data() +
      (box_pos_y_ * size_.width() + box_pos_x_) * kBytesPerPixel;
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
    row += frame->stride();
  }

  if (first_frame_) {
    frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeXYWH(0, 0, kWidth, kHeight));
    first_frame_ = false;
  } else {
    frame->mutable_updated_region()->SetRect(webrtc::DesktopRect::MakeXYWH(
        old_box_pos_x, old_box_pos_y, kBoxWidth, kBoxHeight));
    frame->mutable_updated_region()->AddRect(webrtc::DesktopRect::MakeXYWH(
        box_pos_x_, box_pos_y_, kBoxWidth, kBoxHeight));
  }

  return frame.Pass();
}

}  // namespace

FakeDesktopCapturer::FakeDesktopCapturer()
    : callback_(NULL) {
  frame_generator_ = base::Bind(&DefaultFrameGenerator::GenerateFrame,
                                new DefaultFrameGenerator());
}

FakeDesktopCapturer::~FakeDesktopCapturer() {}

void FakeDesktopCapturer::set_frame_generator(
    const FrameGenerator& frame_generator) {
  DCHECK(!callback_);
  frame_generator_ = frame_generator;
}

void FakeDesktopCapturer::Start(Callback* callback) {
  DCHECK(!callback_);
  DCHECK(callback);
  callback_ = callback;
}

void FakeDesktopCapturer::Capture(const webrtc::DesktopRegion& region) {
  base::Time capture_start_time = base::Time::Now();
  scoped_ptr<webrtc::DesktopFrame> frame = frame_generator_.Run(callback_);
  frame->set_capture_time_ms(
      (base::Time::Now() - capture_start_time).InMillisecondsRoundedUp());
  callback_->OnCaptureCompleted(frame.release());
}

}  // namespace remoting
