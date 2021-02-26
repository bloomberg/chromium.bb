// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_radiobutton.h"

#include <utility>

#include "core/fpdfdoc/cpdf_formcontrol.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/formfiller/cffl_formfiller.h"
#include "fpdfsdk/pwl/cpwl_special_button.h"
#include "public/fpdf_fwlevent.h"

CFFL_RadioButton::CFFL_RadioButton(CPDFSDK_FormFillEnvironment* pApp,
                                   CPDFSDK_Widget* pWidget)
    : CFFL_Button(pApp, pWidget) {}

CFFL_RadioButton::~CFFL_RadioButton() = default;

std::unique_ptr<CPWL_Wnd> CFFL_RadioButton::NewPWLWindow(
    const CPWL_Wnd::CreateParams& cp,
    std::unique_ptr<IPWL_SystemHandler::PerWindowData> pAttachedData) {
  auto pWnd = std::make_unique<CPWL_RadioButton>(cp, std::move(pAttachedData));
  pWnd->Realize();
  pWnd->SetCheck(m_pWidget->IsChecked());
  return std::move(pWnd);
}

bool CFFL_RadioButton::OnKeyDown(uint32_t nKeyCode, uint32_t nFlags) {
  switch (nKeyCode) {
    case FWL_VKEY_Return:
    case FWL_VKEY_Space:
      return true;
    default:
      return CFFL_FormFiller::OnKeyDown(nKeyCode, nFlags);
  }
}

bool CFFL_RadioButton::OnChar(CPDFSDK_Annot* pAnnot,
                              uint32_t nChar,
                              uint32_t nFlags) {
  switch (nChar) {
    case FWL_VKEY_Return:
    case FWL_VKEY_Space: {
      CPDFSDK_PageView* pPageView = pAnnot->GetPageView();
      ASSERT(pPageView);

      ObservedPtr<CPDFSDK_Annot> pObserved(m_pWidget.Get());
      if (m_pFormFillEnv->GetInteractiveFormFiller()->OnButtonUp(
              &pObserved, pPageView, nFlags) ||
          !pObserved) {
        return true;
      }

      CFFL_FormFiller::OnChar(pAnnot, nChar, nFlags);
      CPWL_RadioButton* pWnd = GetRadioButton(pPageView, true);
      if (pWnd && !pWnd->IsReadOnly())
        pWnd->SetCheck(true);
      return CommitData(pPageView, nFlags);
    }
    default:
      return CFFL_FormFiller::OnChar(pAnnot, nChar, nFlags);
  }
}

bool CFFL_RadioButton::OnLButtonUp(CPDFSDK_PageView* pPageView,
                                   CPDFSDK_Annot* pAnnot,
                                   uint32_t nFlags,
                                   const CFX_PointF& point) {
  CFFL_Button::OnLButtonUp(pPageView, pAnnot, nFlags, point);

  if (!IsValid())
    return true;

  CPWL_RadioButton* pWnd = GetRadioButton(pPageView, true);
  if (pWnd)
    pWnd->SetCheck(true);

  return CommitData(pPageView, nFlags);
}

bool CFFL_RadioButton::IsDataChanged(CPDFSDK_PageView* pPageView) {
  CPWL_RadioButton* pWnd = GetRadioButton(pPageView, false);
  return pWnd && pWnd->IsChecked() != m_pWidget->IsChecked();
}

void CFFL_RadioButton::SaveData(CPDFSDK_PageView* pPageView) {
  CPWL_RadioButton* pWnd = GetRadioButton(pPageView, false);
  if (!pWnd)
    return;

  bool bNewChecked = pWnd->IsChecked();
  if (bNewChecked) {
    CPDF_FormField* pField = m_pWidget->GetFormField();
    for (int32_t i = 0, sz = pField->CountControls(); i < sz; i++) {
      if (CPDF_FormControl* pCtrl = pField->GetControl(i)) {
        if (pCtrl->IsChecked())
          break;
      }
    }
  }
  ObservedPtr<CPDFSDK_Widget> observed_widget(m_pWidget.Get());
  ObservedPtr<CFFL_RadioButton> observed_this(this);
  m_pWidget->SetCheck(bNewChecked, NotificationOption::kDoNotNotify);
  if (!observed_widget)
    return;

  m_pWidget->UpdateField();
  if (!observed_widget || !observed_this)
    return;

  SetChangeMark();
}

CPWL_RadioButton* CFFL_RadioButton::GetRadioButton(CPDFSDK_PageView* pPageView,
                                                   bool bNew) {
  return static_cast<CPWL_RadioButton*>(GetPWLWindow(pPageView, bNew));
}
