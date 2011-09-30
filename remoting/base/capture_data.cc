// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/capture_data.h"

namespace remoting {

DataPlanes::DataPlanes() {
  for (int i = 0; i < kPlaneCount; ++i) {
    data[i] = NULL;
    strides[i] = 0;
  }
}

CaptureData::CaptureData(const DataPlanes &data_planes,
                         const gfx::Size& size,
                         media::VideoFrame::Format format)
    : data_planes_(data_planes),
      size_(size),
      pixel_format_(format),
      capture_time_ms_(0),
      client_sequence_number_(0) {
}

CaptureData::~CaptureData() {}

}  // namespace remoting
