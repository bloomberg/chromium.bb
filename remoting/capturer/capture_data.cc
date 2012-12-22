// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/capturer/capture_data.h"

namespace remoting {

CaptureData::CaptureData(uint8* data, int stride, const SkISize& size)
    : data_(data),
      stride_(stride),
      size_(size),
      capture_time_ms_(0),
      client_sequence_number_(0),
      dpi_(SkIPoint::Make(0, 0)) {
}

CaptureData::~CaptureData() {}

}  // namespace remoting
