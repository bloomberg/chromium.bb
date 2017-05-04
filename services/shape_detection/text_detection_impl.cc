// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/text_detection_impl.h"

namespace shape_detection {

// static
void TextDetectionImpl::Create(
    const service_manager::BindSourceInfo& source_info,
    mojom::TextDetectionRequest request) {
  DLOG(ERROR) << "Platform not supported for Text Detection Service.";
}

}  // namespace shape_detection
