// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_TEXT_DETECTION_IMPL_WIN_H_
#define SERVICES_SHAPE_DETECTION_TEXT_DETECTION_IMPL_WIN_H_

#include <windows.media.ocr.h>
#include <wrl/client.h>

#include "base/macros.h"
#include "services/shape_detection/public/interfaces/textdetection.mojom.h"

class SkBitmap;

namespace shape_detection {

using ABI::Windows::Media::Ocr::IOcrEngine;

class TextDetectionImplWin : public mojom::TextDetection {
 public:
  explicit TextDetectionImplWin(Microsoft::WRL::ComPtr<IOcrEngine> ocr_engine);
  ~TextDetectionImplWin() override;

  // mojom::TextDetection implementation.
  void Detect(const SkBitmap& bitmap,
              mojom::TextDetection::DetectCallback callback) override;

 private:
  Microsoft::WRL::ComPtr<IOcrEngine> ocr_engine_;

  DISALLOW_COPY_AND_ASSIGN(TextDetectionImplWin);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_TEXT_DETECTION_IMPL_WIN_H_
