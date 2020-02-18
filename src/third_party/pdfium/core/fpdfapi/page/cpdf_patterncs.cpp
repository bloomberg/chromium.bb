// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_patterncs.h"

#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"

CPDF_PatternCS::CPDF_PatternCS(CPDF_Document* pDoc)
    : CPDF_ColorSpace(pDoc, PDFCS_PATTERN) {}

CPDF_PatternCS::~CPDF_PatternCS() = default;

void CPDF_PatternCS::InitializeStockPattern() {
  SetComponentsForStockCS(1);
}

uint32_t CPDF_PatternCS::v_Load(CPDF_Document* pDoc,
                                const CPDF_Array* pArray,
                                std::set<const CPDF_Object*>* pVisited) {
  const CPDF_Object* pBaseCS = pArray->GetDirectObjectAt(1);
  if (pBaseCS == m_pArray)
    return 0;

  auto* pDocPageData = CPDF_DocPageData::FromDocument(pDoc);
  m_pBaseCS = pDocPageData->GetColorSpaceGuarded(pBaseCS, nullptr, pVisited);
  if (!m_pBaseCS)
    return 1;

  if (m_pBaseCS->GetFamily() == PDFCS_PATTERN)
    return 0;

  if (m_pBaseCS->CountComponents() > kMaxPatternColorComps)
    return 0;

  return m_pBaseCS->CountComponents() + 1;
}

bool CPDF_PatternCS::GetRGB(const float* pBuf,
                            float* R,
                            float* G,
                            float* B) const {
  NOTREACHED();
  return false;
}

CPDF_PatternCS* CPDF_PatternCS::AsPatternCS() {
  return this;
}

const CPDF_PatternCS* CPDF_PatternCS::AsPatternCS() const {
  return this;
}

bool CPDF_PatternCS::GetPatternRGB(const PatternValue& value,
                                   float* R,
                                   float* G,
                                   float* B) const {
  if (m_pBaseCS && m_pBaseCS->GetRGB(value.GetComps().data(), R, G, B))
    return true;

  *R = 0.75f;
  *G = 0.75f;
  *B = 0.75f;
  return false;
}
