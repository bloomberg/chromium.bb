// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/frame_generator_util.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
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
}  // namespace

std::unique_ptr<webrtc::DesktopFrame> LoadDesktopFrameFromPng(
    const char* name) {
  base::FilePath file_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
  file_path = file_path.AppendASCII("remoting");
  file_path = file_path.AppendASCII("test");
  file_path = file_path.AppendASCII("data");
  file_path = file_path.AppendASCII(name);

  std::string file_content;
  if (!base::ReadFileToString(file_path, &file_content))
    LOG(FATAL) << "Failed to read " << file_path.MaybeAsASCII()
               << ". Please run remoting/test/data/download.sh";
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(reinterpret_cast<const uint8_t*>(file_content.data()),
                        file_content.size(), &bitmap);
  std::unique_ptr<webrtc::DesktopFrame> frame(new webrtc::BasicDesktopFrame(
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
        frame->GetFrameDataAtPos(webrtc::DesktopVector(rect.left(), y)));
    for (int x = 0; x < rect.width(); ++x) {
      data[x] = color;
    }
  }
}

void DrawBarcode(int value, bool changed, webrtc::DesktopFrame* frame) {
  CHECK(value < (1 << kBarcodeBits));
  for (int i = 0; i < kBarcodeBits; ++i) {
    DrawRect(frame, webrtc::DesktopRect::MakeXYWH(i * kBarcodeCellWidth, 0,
                                                  kBarcodeCellWidth,
                                                  kBarcodeCellHeight),
             (value & 1) ? 0xffffffff : 0xff000000);
    value >>= 1;
  }
  if (changed) {
    frame->mutable_updated_region()->AddRect(webrtc::DesktopRect::MakeXYWH(
        0, 0, kBarcodeCellWidth * kBarcodeBits, kBarcodeCellHeight));
  }
}

int ReadBarcode(const webrtc::DesktopFrame& frame) {
  int result = 0;
  for (int i = kBarcodeBits - 1; i >= 0; --i) {
    // Sample barcode in the center of the cell for each bit.
    int x = i * kBarcodeCellWidth + kBarcodeCellWidth / 2;
    int y = kBarcodeCellHeight / 2;
    uint8_t* data = frame.GetFrameDataAtPos(webrtc::DesktopVector(x, y));
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
    result <<= 1;
    if (bit)
      result |= 1;
  }
  return result;
}

}  // namespace test
}  // namespace remoting
