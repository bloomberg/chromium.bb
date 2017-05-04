// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_TEXT_DETECTION_IMPL_H_
#define SERVICES_SHAPE_DETECTION_TEXT_DETECTION_IMPL_H_

#include "services/shape_detection/public/interfaces/textdetection.mojom.h"

namespace service_manager {
struct BindSourceInfo;
}

namespace shape_detection {

class TextDetectionImpl {
 public:
  static void Create(const service_manager::BindSourceInfo& source_info,
                     mojom::TextDetectionRequest request);

 private:
  ~TextDetectionImpl() = default;

  DISALLOW_COPY_AND_ASSIGN(TextDetectionImpl);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_TEXT_DETECTION_IMPL_H_
