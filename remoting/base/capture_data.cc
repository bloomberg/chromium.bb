// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
                         int width,
                         int height,
                         PixelFormat format) :
    data_planes_(data_planes), dirty_rects_(),
    width_(width), height_(height), pixel_format_(format) {
}

CaptureData::~CaptureData() {}

}  // namespace remoting
