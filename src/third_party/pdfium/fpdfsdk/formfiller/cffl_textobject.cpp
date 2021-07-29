// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_textobject.h"

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfdoc/cpdf_bafontmap.h"

CFFL_TextObject::CFFL_TextObject(CPDFSDK_FormFillEnvironment* pApp,
                                 CPDFSDK_Widget* pWidget)
    : CFFL_FormField(pApp, pWidget) {}

CFFL_TextObject::~CFFL_TextObject() {
  // Destroy view classes before this object's members are destroyed since
  // the view classes have pointers to m_pFontMap that would be left dangling.
  DestroyWindows();
}

CPWL_Wnd* CFFL_TextObject::ResetPWLWindow(const CPDFSDK_PageView* pPageView) {
  DestroyPWLWindow(pPageView);
  ObservedPtr<CPWL_Wnd> pRet(CreateOrUpdatePWLWindow(pPageView));
  m_pWidget->UpdateField();  // May invoke JS, invalidating |pRet|.
  return pRet.Get();
}

CPWL_Wnd* CFFL_TextObject::RestorePWLWindow(const CPDFSDK_PageView* pPageView) {
  SavePWLWindowState(pPageView);
  DestroyPWLWindow(pPageView);
  RecreatePWLWindowFromSavedState(pPageView);
  ObservedPtr<CPWL_Wnd> pRet(GetPWLWindow(pPageView));
  m_pWidget->UpdateField();  // May invoke JS, invalidating |pRet|.
  return pRet.Get();
}

CPDF_BAFontMap* CFFL_TextObject::GetOrCreateFontMap() {
  if (!m_pFontMap) {
    m_pFontMap = std::make_unique<CPDF_BAFontMap>(
        m_pWidget->GetPDFPage()->GetDocument(),
        m_pWidget->GetPDFAnnot()->GetAnnotDict(), "N");
  }
  return m_pFontMap.get();
}
