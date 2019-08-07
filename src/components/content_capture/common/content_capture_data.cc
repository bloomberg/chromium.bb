// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/common/content_capture_data.h"

namespace content_capture {

ContentCaptureData::ContentCaptureData() {}

ContentCaptureData::ContentCaptureData(const ContentCaptureData& data)
    : id(data.id),
      value(data.value),
      bounds(data.bounds),
      children(data.children) {}

ContentCaptureData::~ContentCaptureData() {}

bool ContentCaptureData::operator==(const ContentCaptureData& other) const {
  return id == other.id && value == other.value && bounds == other.bounds &&
         children == other.children;
}

}  // namespace content_capture
