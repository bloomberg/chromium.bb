// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_pagemodule.h"

#include "core/fpdfapi/page/cpdf_colorspace.h"
#include "core/fpdfapi/page/cpdf_devicecs.h"
#include "core/fpdfapi/page/cpdf_patterncs.h"

CPDF_PageModule::CPDF_PageModule()
    : m_StockGrayCS(pdfium::MakeRetain<CPDF_DeviceCS>(PDFCS_DEVICEGRAY)),
      m_StockRGBCS(pdfium::MakeRetain<CPDF_DeviceCS>(PDFCS_DEVICERGB)),
      m_StockCMYKCS(pdfium::MakeRetain<CPDF_DeviceCS>(PDFCS_DEVICECMYK)),
      m_StockPatternCS(pdfium::MakeRetain<CPDF_PatternCS>(nullptr)) {
  m_StockPatternCS->InitializeStockPattern();
}

CPDF_PageModule::~CPDF_PageModule() = default;

CPDF_FontGlobals* CPDF_PageModule::GetFontGlobals() {
  return &m_FontGlobals;
}

RetainPtr<CPDF_ColorSpace> CPDF_PageModule::GetStockCS(int family) {
  if (family == PDFCS_DEVICEGRAY)
    return m_StockGrayCS;
  if (family == PDFCS_DEVICERGB)
    return m_StockRGBCS;
  if (family == PDFCS_DEVICECMYK)
    return m_StockCMYKCS;
  if (family == PDFCS_PATTERN)
    return m_StockPatternCS;
  return nullptr;
}

void CPDF_PageModule::ClearStockFont(CPDF_Document* pDoc) {
  m_FontGlobals.Clear(pDoc);
}
