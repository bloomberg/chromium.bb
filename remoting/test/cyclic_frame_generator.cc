// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/cyclic_frame_generator.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/time/default_tick_clock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/gfx/codec/png_codec.h"

namespace remoting {
namespace test {

namespace {

scoped_ptr<webrtc::DesktopFrame> LoadDesktopFrameFromPng(
    const base::FilePath& file_path) {
  std::string file_content;
  if (!base::ReadFileToString(file_path, &file_content))
    LOG(FATAL) << "Failed to read " << file_path.MaybeAsASCII()
               << ". Please run remoting/test/data/download.sh";
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(reinterpret_cast<const uint8_t*>(file_content.data()),
                        file_content.size(), &bitmap);
  scoped_ptr<webrtc::DesktopFrame> frame(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(bitmap.width(), bitmap.height())));
  bitmap.copyPixelsTo(frame->data(),
                      frame->stride() * frame->size().height(),
                      frame->stride());
  return frame;
}

}  // namespace

// static
scoped_refptr<CyclicFrameGenerator> CyclicFrameGenerator::Create() {
  base::FilePath test_data_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &test_data_path);
  test_data_path = test_data_path.Append(FILE_PATH_LITERAL("remoting"));
  test_data_path = test_data_path.Append(FILE_PATH_LITERAL("test"));
  test_data_path = test_data_path.Append(FILE_PATH_LITERAL("data"));

  std::vector<scoped_ptr<webrtc::DesktopFrame>> frames;
  frames.push_back(
      LoadDesktopFrameFromPng(test_data_path.AppendASCII("test_frame1.png")));
  frames.push_back(
      LoadDesktopFrameFromPng(test_data_path.AppendASCII("test_frame2.png")));
  return new CyclicFrameGenerator(std::move(frames));
}

CyclicFrameGenerator::CyclicFrameGenerator(
    std::vector<scoped_ptr<webrtc::DesktopFrame>> reference_frames)
    : reference_frames_(std::move(reference_frames)),
      clock_(&default_tick_clock_),
      started_time_(clock_->NowTicks()) {
  CHECK(!reference_frames_.empty());
  screen_size_ = reference_frames_[0]->size();
  for (const auto& frame : reference_frames_) {
    CHECK(screen_size_.equals(frame->size()))
        << "All reference frames should have the same size.";
  }
}
CyclicFrameGenerator::~CyclicFrameGenerator() {}

void CyclicFrameGenerator::SetTickClock(base::TickClock* tick_clock) {
  clock_ = tick_clock;
  started_time_ = clock_->NowTicks();
}

scoped_ptr<webrtc::DesktopFrame> CyclicFrameGenerator::GenerateFrame(
    webrtc::DesktopCapturer::Callback* callback) {
  base::TimeTicks now = clock_->NowTicks();
  int reference_frame =
      ((now - started_time_) / frame_cycle_period_) % reference_frames_.size();
  bool cursor_state = ((now - started_time_) / cursor_blink_period_) % 2;

  scoped_ptr<webrtc::DesktopFrame> frame(
      new webrtc::BasicDesktopFrame(screen_size_));
  frame->CopyPixelsFrom(*reference_frames_[reference_frame],
                        webrtc::DesktopVector(),
                        webrtc::DesktopRect::MakeSize(screen_size_));

  // Render the cursor.
  webrtc::DesktopRect cursor_rect =
      webrtc::DesktopRect::MakeXYWH(20, 20, 2, 20);
  if (cursor_state) {
    for (int y = cursor_rect.top(); y < cursor_rect.bottom(); ++y) {
      memset(frame->data() + y * frame->stride() +
                 cursor_rect.left() * webrtc::DesktopFrame::kBytesPerPixel,
             0, cursor_rect.width() * webrtc::DesktopFrame::kBytesPerPixel);
    }
  }

  if (last_reference_frame_ != reference_frame) {
    // The whole frame has changed.
    frame->mutable_updated_region()->AddRect(
        webrtc::DesktopRect::MakeSize(screen_size_));
    last_frame_type_ = FrameType::FULL;
  } else if (last_cursor_state_ != cursor_state) {
    // Cursor state has changed.
    frame->mutable_updated_region()->AddRect(cursor_rect);
    last_frame_type_ = FrameType::CURSOR;
  } else {
    // No changes.
    last_frame_type_ = FrameType::EMPTY;
  }
  last_reference_frame_ = reference_frame;
  last_cursor_state_ = cursor_state;

  return frame;
}

}  // namespace test
}  // namespace remoting
