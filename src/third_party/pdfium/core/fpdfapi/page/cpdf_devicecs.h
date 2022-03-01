// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_DEVICECS_H_
#define CORE_FPDFAPI_PAGE_CPDF_DEVICECS_H_

#include <set>

#include "core/fpdfapi/page/cpdf_colorspace.h"
#include "core/fxcrt/retain_ptr.h"

class CPDF_DeviceCS final : public CPDF_ColorSpace {
 public:
  CONSTRUCT_VIA_MAKE_RETAIN;
  ~CPDF_DeviceCS() override;

  // CPDF_ColorSpace:
  bool GetRGB(pdfium::span<const float> pBuf,
              float* R,
              float* G,
              float* B) const override;
  void TranslateImageLine(pdfium::span<uint8_t> dest_span,
                          pdfium::span<const uint8_t> src_span,
                          int pixels,
                          int image_width,
                          int image_height,
                          bool bTransMask) const override;
  uint32_t v_Load(CPDF_Document* pDoc,
                  const CPDF_Array* pArray,
                  std::set<const CPDF_Object*>* pVisited) override;

 private:
  explicit CPDF_DeviceCS(Family family);
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_DEVICECS_H_
