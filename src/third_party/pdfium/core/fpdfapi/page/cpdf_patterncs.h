// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_PATTERNCS_H_
#define CORE_FPDFAPI_PAGE_CPDF_PATTERNCS_H_

#include <set>

#include "core/fpdfapi/page/cpdf_basedcs.h"
#include "core/fxcrt/retain_ptr.h"

class CPDF_Document;

class CPDF_PatternCS final : public CPDF_BasedCS {
 public:
  CONSTRUCT_VIA_MAKE_RETAIN;
  ~CPDF_PatternCS() override;

  // Called for the stock pattern, since it is not initialized via
  // CPDF_ColorSpace::Load().
  void InitializeStockPattern();

  // CPDF_ColorSpace:
  bool GetRGB(pdfium::span<const float> pBuf,
              float* R,
              float* G,
              float* B) const override;
  const CPDF_PatternCS* AsPatternCS() const override;
  uint32_t v_Load(CPDF_Document* pDoc,
                  const CPDF_Array* pArray,
                  std::set<const CPDF_Object*>* pVisited) override;

  bool GetPatternRGB(const PatternValue& value,
                     float* R,
                     float* G,
                     float* B) const;

 private:
  CPDF_PatternCS();
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_PATTERNCS_H_
