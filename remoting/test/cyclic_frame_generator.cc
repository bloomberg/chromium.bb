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

const int kBarcodeCellWidth = 8;
const int kBarcodeCellHeight = 8;
const int kBarcodeBits = 10;
const int kBarcodeBlackThreshold = 85;
const int kBarcodeWhiteThreshold = 170;

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

void DrawRect(webrtc::DesktopFrame* frame,
              webrtc::DesktopRect rect,
              uint32_t color) {
  for (int y = rect.top(); y < rect.bottom(); ++y) {
    uint32_t* data = reinterpret_cast<uint32_t*>(
        frame->data() + y * frame->stride() +
        rect.left() * webrtc::DesktopFrame::kBytesPerPixel);
    for (int x = 0; x < rect.width(); ++x) {
      data[x] = color;
    }
  }
}

}  // namespace
CyclicFrameGenerator::FrameInfo::FrameInfo() = default;

CyclicFrameGenerator::FrameInfo::FrameInfo(int frame_id,
                                           FrameType type,
                                           base::TimeTicks timestamp)
    : frame_id(frame_id), type(type), timestamp(timestamp) {}

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
    webrtc::SharedMemoryFactory* shared_memory_factory) {
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
    DrawRect(frame.get(), cursor_rect, 0);
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

  // Increment frame_id for non-empty frames.
  int frame_id = (last_frame_type_ == FrameType::EMPTY) ? last_frame_id_
                                                        : last_frame_id_ + 1;

  // Render barcode.
  if (draw_barcode_) {
    uint32_t value = static_cast<uint32_t>(frame_id);
    CHECK(value < (1U << kBarcodeBits));
    for (int i = 0; i < kBarcodeBits; ++i) {
      DrawRect(frame.get(), webrtc::DesktopRect::MakeXYWH(i * kBarcodeCellWidth,
                                                          0, kBarcodeCellWidth,
                                                          kBarcodeCellHeight),
               (value & 1) ? 0xffffffff : 0xff000000);
      value >>= 1;
    }

    if (frame_id != last_frame_id_) {
      frame->mutable_updated_region()->AddRect(webrtc::DesktopRect::MakeXYWH(
          0, 0, kBarcodeCellWidth * kBarcodeBits, kBarcodeCellHeight));
    }
  }

  // Add non-empty frames to |generated_frames_info_|.
  if (last_frame_type_ != FrameType::EMPTY) {
    generated_frames_info_[frame_id] =
        FrameInfo(frame_id, last_frame_type_, clock_->NowTicks());
  }

  last_reference_frame_ = reference_frame;
  last_cursor_state_ = cursor_state;
  last_frame_id_ = frame_id;

  return frame;
}

CyclicFrameGenerator::FrameInfo CyclicFrameGenerator::IdentifyFrame(
    webrtc::DesktopFrame* frame) {
  CHECK(draw_barcode_);
  int frame_id = 0;
  for (int i = kBarcodeBits - 1; i >= 0; --i) {
    // Sample barcode in the center of the cell for each bit.
    int x = i * kBarcodeCellWidth + kBarcodeCellWidth / 2;
    int y = kBarcodeCellHeight / 2;
    uint8_t* data = (frame->data() + y * frame->stride() +
                     x * webrtc::DesktopFrame::kBytesPerPixel);
    int b = data[0];
    int g = data[1];
    int r = data[2];
    bool bit = 0;
    if (b > kBarcodeWhiteThreshold && g > kBarcodeWhiteThreshold &&
        r > kBarcodeWhiteThreshold) {
      bit = 1;
    } else if (b < kBarcodeBlackThreshold && g < kBarcodeBlackThreshold &&
        r < kBarcodeBlackThreshold) {
      bit = 0;
    } else {
      LOG(FATAL) << "Invalid barcode.";
    }
    frame_id <<= 1;
    if (bit)
      frame_id |= 1;
  }

  if (!generated_frames_info_.count(frame_id))
    LOG(FATAL) << "Barcode contains unknown frame_id: " << frame_id;

  return generated_frames_info_[frame_id];
}

}  // namespace test
}  // namespace remoting
