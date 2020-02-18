// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_interactiveformfiller.h"

#include "constants/form_flags.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fxcrt/autorestorer.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_interactiveform.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/formfiller/cffl_checkbox.h"
#include "fpdfsdk/formfiller/cffl_combobox.h"
#include "fpdfsdk/formfiller/cffl_formfiller.h"
#include "fpdfsdk/formfiller/cffl_listbox.h"
#include "fpdfsdk/formfiller/cffl_pushbutton.h"
#include "fpdfsdk/formfiller/cffl_radiobutton.h"
#include "fpdfsdk/formfiller/cffl_textfield.h"
#include "public/fpdf_fwlevent.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

CFFL_InteractiveFormFiller::CFFL_InteractiveFormFiller(
    CPDFSDK_FormFillEnvironment* pFormFillEnv)
    : m_pFormFillEnv(pFormFillEnv) {}

CFFL_InteractiveFormFiller::~CFFL_InteractiveFormFiller() = default;

bool CFFL_InteractiveFormFiller::Annot_HitTest(CPDFSDK_PageView* pPageView,
                                               CPDFSDK_Annot* pAnnot,
                                               const CFX_PointF& point) {
  return pAnnot->GetRect().Contains(point);
}

FX_RECT CFFL_InteractiveFormFiller::GetViewBBox(CPDFSDK_PageView* pPageView,
                                                CPDFSDK_Annot* pAnnot) {
  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot))
    return pFormFiller->GetViewBBox(pPageView);

  ASSERT(pPageView);

  CPDF_Annot* pPDFAnnot = pAnnot->GetPDFAnnot();
  CFX_FloatRect rcWin = pPDFAnnot->GetRect();
  if (!rcWin.IsEmpty()) {
    rcWin.Inflate(1, 1);
    rcWin.Normalize();
  }
  return rcWin.GetOuterRect();
}

void CFFL_InteractiveFormFiller::OnDraw(CPDFSDK_PageView* pPageView,
                                        CPDFSDK_Annot* pAnnot,
                                        CFX_RenderDevice* pDevice,
                                        const CFX_Matrix& mtUser2Device) {
  ASSERT(pPageView);
  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot);
  if (!IsVisible(pWidget))
    return;

  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot);
  if (pFormFiller && pFormFiller->IsValid()) {
    pFormFiller->OnDraw(pPageView, pAnnot, pDevice, mtUser2Device);
    pAnnot->GetPDFPage();
    if (m_pFormFillEnv->GetFocusAnnot() != pAnnot)
      return;

    CFX_FloatRect rcFocus = pFormFiller->GetFocusBox(pPageView);
    if (rcFocus.IsEmpty())
      return;

    CFX_PathData path;
    path.AppendPoint(CFX_PointF(rcFocus.left, rcFocus.top), FXPT_TYPE::MoveTo,
                     false);
    path.AppendPoint(CFX_PointF(rcFocus.left, rcFocus.bottom),
                     FXPT_TYPE::LineTo, false);
    path.AppendPoint(CFX_PointF(rcFocus.right, rcFocus.bottom),
                     FXPT_TYPE::LineTo, false);
    path.AppendPoint(CFX_PointF(rcFocus.right, rcFocus.top), FXPT_TYPE::LineTo,
                     false);
    path.AppendPoint(CFX_PointF(rcFocus.left, rcFocus.top), FXPT_TYPE::LineTo,
                     false);

    CFX_GraphStateData gsd;
    gsd.m_DashArray = {1.0f};
    gsd.m_DashPhase = 0;
    gsd.m_LineWidth = 1.0f;
    pDevice->DrawPath(&path, &mtUser2Device, &gsd, 0, ArgbEncode(255, 0, 0, 0),
                      FXFILL_ALTERNATE);
    return;
  }

  if (pFormFiller) {
    pFormFiller->OnDrawDeactive(pPageView, pAnnot, pDevice, mtUser2Device);
  } else {
    pWidget->DrawAppearance(pDevice, mtUser2Device, CPDF_Annot::Normal,
                            nullptr);
  }

  if (!IsReadOnly(pWidget) && IsFillingAllowed(pWidget))
    pWidget->DrawShadow(pDevice, pPageView);
}

void CFFL_InteractiveFormFiller::OnDelete(CPDFSDK_Annot* pAnnot) {
  UnRegisterFormFiller(pAnnot);
}

void CFFL_InteractiveFormFiller::OnMouseEnter(
    CPDFSDK_PageView* pPageView,
    ObservedPtr<CPDFSDK_Annot>* pAnnot,
    uint32_t nFlag) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  if (!m_bNotifying) {
    CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
    if (pWidget->GetAAction(CPDF_AAction::kCursorEnter).GetDict()) {
      m_bNotifying = true;

      uint32_t nValueAge = pWidget->GetValueAge();
      pWidget->ClearAppModified();
      ASSERT(pPageView);

      CPDFSDK_FieldAction fa;
      fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlag);
      fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlag);
      pWidget->OnAAction(CPDF_AAction::kCursorEnter, &fa, pPageView);
      m_bNotifying = false;
      if (!pAnnot->HasObservable())
        return;

      if (pWidget->IsAppModified()) {
        if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget)) {
          pFormFiller->ResetPWLWindow(pPageView,
                                      pWidget->GetValueAge() == nValueAge);
        }
      }
    }
  }
  if (CFFL_FormFiller* pFormFiller = GetOrCreateFormFiller(pAnnot->Get()))
    pFormFiller->OnMouseEnter(pPageView);
}

void CFFL_InteractiveFormFiller::OnMouseExit(CPDFSDK_PageView* pPageView,
                                             ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                             uint32_t nFlag) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  if (!m_bNotifying) {
    CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
    if (pWidget->GetAAction(CPDF_AAction::kCursorExit).GetDict()) {
      m_bNotifying = true;

      uint32_t nValueAge = pWidget->GetValueAge();
      pWidget->ClearAppModified();
      ASSERT(pPageView);

      CPDFSDK_FieldAction fa;
      fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlag);
      fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlag);
      pWidget->OnAAction(CPDF_AAction::kCursorExit, &fa, pPageView);
      m_bNotifying = false;
      if (!pAnnot->HasObservable())
        return;

      if (pWidget->IsAppModified()) {
        if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget)) {
          pFormFiller->ResetPWLWindow(pPageView,
                                      nValueAge == pWidget->GetValueAge());
        }
      }
    }
  }
  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get()))
    pFormFiller->OnMouseExit(pPageView);
}

bool CFFL_InteractiveFormFiller::OnLButtonDown(
    CPDFSDK_PageView* pPageView,
    ObservedPtr<CPDFSDK_Annot>* pAnnot,
    uint32_t nFlags,
    const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  if (!m_bNotifying) {
    CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
    if (Annot_HitTest(pPageView, pAnnot->Get(), point) &&
        pWidget->GetAAction(CPDF_AAction::kButtonDown).GetDict()) {
      m_bNotifying = true;

      uint32_t nValueAge = pWidget->GetValueAge();
      pWidget->ClearAppModified();
      ASSERT(pPageView);

      CPDFSDK_FieldAction fa;
      fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlags);
      fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlags);
      pWidget->OnAAction(CPDF_AAction::kButtonDown, &fa, pPageView);
      m_bNotifying = false;
      if (!pAnnot->HasObservable())
        return true;

      if (!IsValidAnnot(pPageView, pAnnot->Get()))
        return true;

      if (pWidget->IsAppModified()) {
        if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget)) {
          pFormFiller->ResetPWLWindow(pPageView,
                                      nValueAge == pWidget->GetValueAge());
        }
      }
    }
  }
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get());
  return pFormFiller &&
         pFormFiller->OnLButtonDown(pPageView, pAnnot->Get(), nFlags, point);
}

bool CFFL_InteractiveFormFiller::OnLButtonUp(CPDFSDK_PageView* pPageView,
                                             ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                             uint32_t nFlags,
                                             const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());

  bool bSetFocus;
  switch (pWidget->GetFieldType()) {
    case FormFieldType::kPushButton:
    case FormFieldType::kCheckBox:
    case FormFieldType::kRadioButton: {
      FX_RECT bbox = GetViewBBox(pPageView, pAnnot->Get());
      bSetFocus =
          bbox.Contains(static_cast<int>(point.x), static_cast<int>(point.y));
      break;
    }
    default:
      bSetFocus = true;
      break;
  }
  if (bSetFocus)
    m_pFormFillEnv->SetFocusAnnot(pAnnot);

  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get());
  bool bRet = pFormFiller &&
              pFormFiller->OnLButtonUp(pPageView, pAnnot->Get(), nFlags, point);
  if (m_pFormFillEnv->GetFocusAnnot() != pAnnot->Get())
    return bRet;
  if (OnButtonUp(pAnnot, pPageView, nFlags) || !pAnnot)
    return true;
#ifdef PDF_ENABLE_XFA
  if (OnClick(pAnnot, pPageView, nFlags) || !pAnnot)
    return true;
#endif  // PDF_ENABLE_XFA
  return bRet;
}

bool CFFL_InteractiveFormFiller::OnButtonUp(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                            CPDFSDK_PageView* pPageView,
                                            uint32_t nFlag) {
  if (m_bNotifying)
    return false;

  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
  if (!pWidget->GetAAction(CPDF_AAction::kButtonUp).GetDict())
    return false;

  m_bNotifying = true;

  uint32_t nAge = pWidget->GetAppearanceAge();
  uint32_t nValueAge = pWidget->GetValueAge();
  ASSERT(pPageView);

  CPDFSDK_FieldAction fa;
  fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlag);
  fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlag);
  pWidget->OnAAction(CPDF_AAction::kButtonUp, &fa, pPageView);
  m_bNotifying = false;
  if (!pAnnot->HasObservable() || !IsValidAnnot(pPageView, pWidget))
    return true;
  if (nAge == pWidget->GetAppearanceAge())
    return false;

  CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget);
  if (pFormFiller)
    pFormFiller->ResetPWLWindow(pPageView, nValueAge == pWidget->GetValueAge());
  return true;
}

bool CFFL_InteractiveFormFiller::SetIndexSelected(
    ObservedPtr<CPDFSDK_Annot>* pAnnot,
    int index,
    bool selected) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);

  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get());
  return pFormFiller && pFormFiller->SetIndexSelected(index, selected);
}

bool CFFL_InteractiveFormFiller::IsIndexSelected(
    ObservedPtr<CPDFSDK_Annot>* pAnnot,
    int index) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);

  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get());
  return pFormFiller && pFormFiller->IsIndexSelected(index);
}

bool CFFL_InteractiveFormFiller::OnLButtonDblClk(
    CPDFSDK_PageView* pPageView,
    ObservedPtr<CPDFSDK_Annot>* pAnnot,
    uint32_t nFlags,
    const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get());
  return pFormFiller && pFormFiller->OnLButtonDblClk(pPageView, nFlags, point);
}

bool CFFL_InteractiveFormFiller::OnMouseMove(CPDFSDK_PageView* pPageView,
                                             ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                             uint32_t nFlags,
                                             const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetOrCreateFormFiller(pAnnot->Get());
  return pFormFiller && pFormFiller->OnMouseMove(pPageView, nFlags, point);
}

bool CFFL_InteractiveFormFiller::OnMouseWheel(
    CPDFSDK_PageView* pPageView,
    ObservedPtr<CPDFSDK_Annot>* pAnnot,
    uint32_t nFlags,
    short zDelta,
    const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get());
  return pFormFiller &&
         pFormFiller->OnMouseWheel(pPageView, nFlags, zDelta, point);
}

bool CFFL_InteractiveFormFiller::OnRButtonDown(
    CPDFSDK_PageView* pPageView,
    ObservedPtr<CPDFSDK_Annot>* pAnnot,
    uint32_t nFlags,
    const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get());
  return pFormFiller && pFormFiller->OnRButtonDown(pPageView, nFlags, point);
}

bool CFFL_InteractiveFormFiller::OnRButtonUp(CPDFSDK_PageView* pPageView,
                                             ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                             uint32_t nFlags,
                                             const CFX_PointF& point) {
  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get());
  return pFormFiller && pFormFiller->OnRButtonUp(pPageView, nFlags, point);
}

bool CFFL_InteractiveFormFiller::OnKeyDown(CPDFSDK_Annot* pAnnot,
                                           uint32_t nKeyCode,
                                           uint32_t nFlags) {
  ASSERT(pAnnot->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);

  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot);
  return pFormFiller && pFormFiller->OnKeyDown(nKeyCode, nFlags);
}

bool CFFL_InteractiveFormFiller::OnChar(CPDFSDK_Annot* pAnnot,
                                        uint32_t nChar,
                                        uint32_t nFlags) {
  ASSERT(pAnnot->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  if (nChar == FWL_VKEY_Tab)
    return true;

  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot);
  return pFormFiller && pFormFiller->OnChar(pAnnot, nChar, nFlags);
}

bool CFFL_InteractiveFormFiller::OnSetFocus(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                            uint32_t nFlag) {
  if (!pAnnot->HasObservable())
    return false;

  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  if (!m_bNotifying) {
    CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
    if (pWidget->GetAAction(CPDF_AAction::kGetFocus).GetDict()) {
      m_bNotifying = true;

      uint32_t nValueAge = pWidget->GetValueAge();
      pWidget->ClearAppModified();

      CFFL_FormFiller* pFormFiller = GetOrCreateFormFiller(pWidget);
      if (!pFormFiller)
        return false;

      CPDFSDK_PageView* pPageView = (*pAnnot)->GetPageView();
      ASSERT(pPageView);

      CPDFSDK_FieldAction fa;
      fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlag);
      fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlag);
      pFormFiller->GetActionData(pPageView, CPDF_AAction::kGetFocus, fa);
      pWidget->OnAAction(CPDF_AAction::kGetFocus, &fa, pPageView);
      m_bNotifying = false;
      if (!pAnnot->HasObservable())
        return false;

      if (pWidget->IsAppModified()) {
        if (CFFL_FormFiller* pFiller = GetFormFiller(pWidget)) {
          pFiller->ResetPWLWindow(pPageView,
                                  nValueAge == pWidget->GetValueAge());
        }
      }
    }
  }

  if (CFFL_FormFiller* pFormFiller = GetOrCreateFormFiller(pAnnot->Get()))
    pFormFiller->SetFocusForAnnot(pAnnot->Get(), nFlag);

  return true;
}

bool CFFL_InteractiveFormFiller::OnKillFocus(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                             uint32_t nFlag) {
  if (!pAnnot->HasObservable())
    return false;

  ASSERT((*pAnnot)->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot->Get());
  if (!pFormFiller)
    return true;

  pFormFiller->KillFocusForAnnot(nFlag);
  if (!pAnnot->HasObservable())
    return false;

  if (m_bNotifying)
    return true;

  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
  if (!pWidget->GetAAction(CPDF_AAction::kLoseFocus).GetDict())
    return true;

  m_bNotifying = true;
  pWidget->ClearAppModified();

  CPDFSDK_PageView* pPageView = pWidget->GetPageView();
  ASSERT(pPageView);

  CPDFSDK_FieldAction fa;
  fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlag);
  fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlag);
  pFormFiller->GetActionData(pPageView, CPDF_AAction::kLoseFocus, fa);
  pWidget->OnAAction(CPDF_AAction::kLoseFocus, &fa, pPageView);
  m_bNotifying = false;
  return pAnnot->HasObservable();
}

bool CFFL_InteractiveFormFiller::IsVisible(CPDFSDK_Widget* pWidget) {
  return pWidget->IsVisible();
}

bool CFFL_InteractiveFormFiller::IsReadOnly(CPDFSDK_Widget* pWidget) {
  int nFieldFlags = pWidget->GetFieldFlags();
  return !!(nFieldFlags & pdfium::form_flags::kReadOnly);
}

bool CFFL_InteractiveFormFiller::IsFillingAllowed(CPDFSDK_Widget* pWidget) {
  if (pWidget->GetFieldType() == FormFieldType::kPushButton)
    return false;

  CPDF_Page* pPage = pWidget->GetPDFPage();
  uint32_t dwPermissions = pPage->GetDocument()->GetUserPermissions();
  return (dwPermissions & FPDFPERM_FILL_FORM) ||
         (dwPermissions & FPDFPERM_ANNOT_FORM) ||
         (dwPermissions & FPDFPERM_MODIFY);
}

CFFL_FormFiller* CFFL_InteractiveFormFiller::GetFormFiller(
    CPDFSDK_Annot* pAnnot) {
  auto it = m_Map.find(pAnnot);
  return it != m_Map.end() ? it->second.get() : nullptr;
}

CFFL_FormFiller* CFFL_InteractiveFormFiller::GetOrCreateFormFiller(
    CPDFSDK_Annot* pAnnot) {
  CFFL_FormFiller* result = GetFormFiller(pAnnot);
  if (result)
    return result;

  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot);
  std::unique_ptr<CFFL_FormFiller> pFormFiller;
  switch (pWidget->GetFieldType()) {
    case FormFieldType::kPushButton:
      pFormFiller =
          pdfium::MakeUnique<CFFL_PushButton>(m_pFormFillEnv.Get(), pWidget);
      break;
    case FormFieldType::kCheckBox:
      pFormFiller =
          pdfium::MakeUnique<CFFL_CheckBox>(m_pFormFillEnv.Get(), pWidget);
      break;
    case FormFieldType::kRadioButton:
      pFormFiller =
          pdfium::MakeUnique<CFFL_RadioButton>(m_pFormFillEnv.Get(), pWidget);
      break;
    case FormFieldType::kTextField:
      pFormFiller =
          pdfium::MakeUnique<CFFL_TextField>(m_pFormFillEnv.Get(), pWidget);
      break;
    case FormFieldType::kListBox:
      pFormFiller =
          pdfium::MakeUnique<CFFL_ListBox>(m_pFormFillEnv.Get(), pWidget);
      break;
    case FormFieldType::kComboBox:
      pFormFiller =
          pdfium::MakeUnique<CFFL_ComboBox>(m_pFormFillEnv.Get(), pWidget);
      break;
    case FormFieldType::kUnknown:
    default:
      return nullptr;
  }

  result = pFormFiller.get();
  m_Map[pAnnot] = std::move(pFormFiller);
  return result;
}

WideString CFFL_InteractiveFormFiller::GetText(CPDFSDK_Annot* pAnnot) {
  ASSERT(pAnnot->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot);
  return pFormFiller ? pFormFiller->GetText() : WideString();
}

WideString CFFL_InteractiveFormFiller::GetSelectedText(CPDFSDK_Annot* pAnnot) {
  ASSERT(pAnnot->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot);
  return pFormFiller ? pFormFiller->GetSelectedText() : WideString();
}

void CFFL_InteractiveFormFiller::ReplaceSelection(CPDFSDK_Annot* pAnnot,
                                                  const WideString& text) {
  ASSERT(pAnnot->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot);
  if (!pFormFiller)
    return;

  pFormFiller->ReplaceSelection(text);
}

bool CFFL_InteractiveFormFiller::CanUndo(CPDFSDK_Annot* pAnnot) {
  ASSERT(pAnnot->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot);
  return pFormFiller && pFormFiller->CanUndo();
}

bool CFFL_InteractiveFormFiller::CanRedo(CPDFSDK_Annot* pAnnot) {
  ASSERT(pAnnot->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot);
  return pFormFiller && pFormFiller->CanRedo();
}

bool CFFL_InteractiveFormFiller::Undo(CPDFSDK_Annot* pAnnot) {
  ASSERT(pAnnot->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot);
  return pFormFiller && pFormFiller->Undo();
}

bool CFFL_InteractiveFormFiller::Redo(CPDFSDK_Annot* pAnnot) {
  ASSERT(pAnnot->GetPDFAnnot()->GetSubtype() == CPDF_Annot::Subtype::WIDGET);
  CFFL_FormFiller* pFormFiller = GetFormFiller(pAnnot);
  return pFormFiller && pFormFiller->Redo();
}

void CFFL_InteractiveFormFiller::UnRegisterFormFiller(CPDFSDK_Annot* pAnnot) {
  auto it = m_Map.find(pAnnot);
  if (it == m_Map.end())
    return;

  m_Map.erase(it);
}

void CFFL_InteractiveFormFiller::QueryWherePopup(
    const IPWL_SystemHandler::PerWindowData* pAttached,
    float fPopupMin,
    float fPopupMax,
    bool* bBottom,
    float* fPopupRet) {
  auto* pData = static_cast<const CFFL_PrivateData*>(pAttached);
  CPDFSDK_Widget* pWidget = pData->pWidget.Get();
  CPDF_Page* pPage = pWidget->GetPDFPage();

  CFX_FloatRect rcPageView(0, pPage->GetPageHeight(), pPage->GetPageWidth(), 0);
  rcPageView.Normalize();

  CFX_FloatRect rcAnnot = pWidget->GetRect();
  float fTop = 0.0f;
  float fBottom = 0.0f;
  switch (pWidget->GetRotate() / 90) {
    default:
    case 0:
      fTop = rcPageView.top - rcAnnot.top;
      fBottom = rcAnnot.bottom - rcPageView.bottom;
      break;
    case 1:
      fTop = rcAnnot.left - rcPageView.left;
      fBottom = rcPageView.right - rcAnnot.right;
      break;
    case 2:
      fTop = rcAnnot.bottom - rcPageView.bottom;
      fBottom = rcPageView.top - rcAnnot.top;
      break;
    case 3:
      fTop = rcPageView.right - rcAnnot.right;
      fBottom = rcAnnot.left - rcPageView.left;
      break;
  }

  constexpr float kMaxListBoxHeight = 140;
  const float fMaxListBoxHeight =
      pdfium::clamp(kMaxListBoxHeight, fPopupMin, fPopupMax);

  if (fBottom > fMaxListBoxHeight) {
    *fPopupRet = fMaxListBoxHeight;
    *bBottom = true;
    return;
  }

  if (fTop > fMaxListBoxHeight) {
    *fPopupRet = fMaxListBoxHeight;
    *bBottom = false;
    return;
  }

  if (fTop > fBottom) {
    *fPopupRet = fTop;
    *bBottom = false;
  } else {
    *fPopupRet = fBottom;
    *bBottom = true;
  }
}

bool CFFL_InteractiveFormFiller::OnKeyStrokeCommit(
    ObservedPtr<CPDFSDK_Annot>* pAnnot,
    CPDFSDK_PageView* pPageView,
    uint32_t nFlag) {
  if (m_bNotifying)
    return true;

  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
  if (!pWidget->GetAAction(CPDF_AAction::kKeyStroke).GetDict())
    return true;

  ASSERT(pPageView);
  m_bNotifying = true;
  pWidget->ClearAppModified();

  CPDFSDK_FieldAction fa;
  fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlag);
  fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlag);
  fa.bWillCommit = true;
  fa.bKeyDown = true;
  fa.bRC = true;

  CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget);
  pFormFiller->GetActionData(pPageView, CPDF_AAction::kKeyStroke, fa);
  pFormFiller->SaveState(pPageView);
  pWidget->OnAAction(CPDF_AAction::kKeyStroke, &fa, pPageView);
  if (!pAnnot->HasObservable())
    return true;

  m_bNotifying = false;
  return fa.bRC;
}

bool CFFL_InteractiveFormFiller::OnValidate(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                            CPDFSDK_PageView* pPageView,
                                            uint32_t nFlag) {
  if (m_bNotifying)
    return true;

  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
  if (!pWidget->GetAAction(CPDF_AAction::kValidate).GetDict())
    return true;

  ASSERT(pPageView);
  m_bNotifying = true;
  pWidget->ClearAppModified();

  CPDFSDK_FieldAction fa;
  fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlag);
  fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlag);
  fa.bKeyDown = true;
  fa.bRC = true;

  CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget);
  pFormFiller->GetActionData(pPageView, CPDF_AAction::kValidate, fa);
  pFormFiller->SaveState(pPageView);
  pWidget->OnAAction(CPDF_AAction::kValidate, &fa, pPageView);
  if (!pAnnot->HasObservable())
    return true;

  m_bNotifying = false;
  return fa.bRC;
}

void CFFL_InteractiveFormFiller::OnCalculate(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                             CPDFSDK_PageView* pPageView,
                                             uint32_t nFlag) {
  if (m_bNotifying)
    return;

  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
  if (pWidget) {
    CPDFSDK_InteractiveForm* pForm =
        pPageView->GetFormFillEnv()->GetInteractiveForm();
    pForm->OnCalculate(pWidget->GetFormField());
  }
  m_bNotifying = false;
}

void CFFL_InteractiveFormFiller::OnFormat(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                          CPDFSDK_PageView* pPageView,
                                          uint32_t nFlag) {
  if (m_bNotifying)
    return;

  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
  ASSERT(pWidget);
  CPDFSDK_InteractiveForm* pForm =
      pPageView->GetFormFillEnv()->GetInteractiveForm();

  Optional<WideString> sValue = pForm->OnFormat(pWidget->GetFormField());
  if (!pAnnot->HasObservable())
    return;

  if (sValue.has_value()) {
    pForm->ResetFieldAppearance(pWidget->GetFormField(), sValue);
    pForm->UpdateField(pWidget->GetFormField());
  }

  m_bNotifying = false;
}

#ifdef PDF_ENABLE_XFA
bool CFFL_InteractiveFormFiller::OnClick(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                         CPDFSDK_PageView* pPageView,
                                         uint32_t nFlag) {
  if (m_bNotifying)
    return false;

  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
  if (!pWidget->HasXFAAAction(PDFSDK_XFA_Click))
    return false;

  m_bNotifying = true;
  uint32_t nAge = pWidget->GetAppearanceAge();
  uint32_t nValueAge = pWidget->GetValueAge();

  CPDFSDK_FieldAction fa;
  fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlag);
  fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlag);

  pWidget->OnXFAAAction(PDFSDK_XFA_Click, &fa, pPageView);
  m_bNotifying = false;
  if (!pAnnot->HasObservable() || !IsValidAnnot(pPageView, pWidget))
    return true;
  if (nAge == pWidget->GetAppearanceAge())
    return false;

  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget))
    pFormFiller->ResetPWLWindow(pPageView, nValueAge == pWidget->GetValueAge());
  return false;
}

bool CFFL_InteractiveFormFiller::OnFull(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                        CPDFSDK_PageView* pPageView,
                                        uint32_t nFlag) {
  if (m_bNotifying)
    return false;

  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
  if (!pWidget->HasXFAAAction(PDFSDK_XFA_Full))
    return false;

  m_bNotifying = true;
  uint32_t nAge = pWidget->GetAppearanceAge();
  uint32_t nValueAge = pWidget->GetValueAge();

  CPDFSDK_FieldAction fa;
  fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlag);
  fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlag);

  pWidget->OnXFAAAction(PDFSDK_XFA_Full, &fa, pPageView);
  m_bNotifying = false;
  if (!pAnnot->HasObservable() || !IsValidAnnot(pPageView, pWidget))
    return true;
  if (nAge == pWidget->GetAppearanceAge())
    return false;

  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget))
    pFormFiller->ResetPWLWindow(pPageView, nValueAge == pWidget->GetValueAge());

  return true;
}

bool CFFL_InteractiveFormFiller::OnPreOpen(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                           CPDFSDK_PageView* pPageView,
                                           uint32_t nFlag) {
  if (m_bNotifying)
    return false;

  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
  if (!pWidget->HasXFAAAction(PDFSDK_XFA_PreOpen))
    return false;

  m_bNotifying = true;
  uint32_t nAge = pWidget->GetAppearanceAge();
  uint32_t nValueAge = pWidget->GetValueAge();

  CPDFSDK_FieldAction fa;
  fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlag);
  fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlag);

  pWidget->OnXFAAAction(PDFSDK_XFA_PreOpen, &fa, pPageView);
  m_bNotifying = false;
  if (!pAnnot->HasObservable() || !IsValidAnnot(pPageView, pWidget))
    return true;
  if (nAge == pWidget->GetAppearanceAge())
    return false;

  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget))
    pFormFiller->ResetPWLWindow(pPageView, nValueAge == pWidget->GetValueAge());

  return true;
}

bool CFFL_InteractiveFormFiller::OnPostOpen(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                            CPDFSDK_PageView* pPageView,
                                            uint32_t nFlag) {
  if (m_bNotifying)
    return false;

  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot->Get());
  if (!pWidget->HasXFAAAction(PDFSDK_XFA_PostOpen))
    return false;

  m_bNotifying = true;
  uint32_t nAge = pWidget->GetAppearanceAge();
  uint32_t nValueAge = pWidget->GetValueAge();

  CPDFSDK_FieldAction fa;
  fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlag);
  fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlag);

  pWidget->OnXFAAAction(PDFSDK_XFA_PostOpen, &fa, pPageView);
  m_bNotifying = false;
  if (!pAnnot->HasObservable() || !IsValidAnnot(pPageView, pWidget))
    return true;
  if (nAge == pWidget->GetAppearanceAge())
    return false;

  if (CFFL_FormFiller* pFormFiller = GetFormFiller(pWidget))
    pFormFiller->ResetPWLWindow(pPageView, nValueAge == pWidget->GetValueAge());

  return true;
}
#endif  // PDF_ENABLE_XFA

bool CFFL_InteractiveFormFiller::IsValidAnnot(CPDFSDK_PageView* pPageView,
                                              CPDFSDK_Annot* pAnnot) {
  return pPageView && pPageView->IsValidAnnot(pAnnot->GetPDFAnnot());
}

std::pair<bool, bool> CFFL_InteractiveFormFiller::OnBeforeKeyStroke(
    const IPWL_SystemHandler::PerWindowData* pAttached,
    WideString& strChange,
    const WideString& strChangeEx,
    int nSelStart,
    int nSelEnd,
    bool bKeyDown,
    uint32_t nFlag) {
  // Copy the private data since the window owning it may not survive.
  CFFL_PrivateData privateData =
      *static_cast<const CFFL_PrivateData*>(pAttached);
  ASSERT(privateData.pWidget);

  CFFL_FormFiller* pFormFiller = GetFormFiller(privateData.GetWidget());

#ifdef PDF_ENABLE_XFA
  if (pFormFiller->IsFieldFull(privateData.pPageView)) {
    ObservedPtr<CPDFSDK_Annot> pObserved(privateData.GetWidget());
    if (OnFull(&pObserved, privateData.pPageView, nFlag) || !pObserved)
      return {true, true};
  }
#endif  // PDF_ENABLE_XFA

  if (m_bNotifying ||
      !privateData.pWidget->GetAAction(CPDF_AAction::kKeyStroke).GetDict()) {
    return {true, false};
  }

  AutoRestorer<bool> restorer(&m_bNotifying);
  m_bNotifying = true;

  uint32_t nAge = privateData.pWidget->GetAppearanceAge();
  uint32_t nValueAge = privateData.pWidget->GetValueAge();
  CPDFSDK_FormFillEnvironment* pFormFillEnv =
      privateData.pPageView->GetFormFillEnv();

  CPDFSDK_FieldAction fa;
  fa.bModifier = CPWL_Wnd::IsCTRLKeyDown(nFlag);
  fa.bShift = CPWL_Wnd::IsSHIFTKeyDown(nFlag);
  fa.sChange = strChange;
  fa.sChangeEx = strChangeEx;
  fa.bKeyDown = bKeyDown;
  fa.bWillCommit = false;
  fa.bRC = true;
  fa.nSelStart = nSelStart;
  fa.nSelEnd = nSelEnd;
  pFormFiller->GetActionData(privateData.pPageView, CPDF_AAction::kKeyStroke,
                             fa);
  pFormFiller->SaveState(privateData.pPageView);

  ObservedPtr<CPDFSDK_Annot> pObserved(privateData.GetWidget());
  bool action_status = privateData.pWidget->OnAAction(
      CPDF_AAction::kKeyStroke, &fa, privateData.pPageView);

  if (!pObserved ||
      !IsValidAnnot(privateData.pPageView, privateData.GetWidget())) {
    return {true, true};
  }
  if (!action_status)
    return {true, false};

  bool bExit = false;
  if (nAge != privateData.pWidget->GetAppearanceAge()) {
    CPWL_Wnd* pWnd = pFormFiller->ResetPWLWindow(
        privateData.pPageView, nValueAge == privateData.pWidget->GetValueAge());
    if (!pWnd)
      return {true, true};
    privateData =
        *static_cast<const CFFL_PrivateData*>(pWnd->GetAttachedData());
    bExit = true;
  }
  if (fa.bRC) {
    pFormFiller->SetActionData(privateData.pPageView, CPDF_AAction::kKeyStroke,
                               fa);
  } else {
    pFormFiller->RestoreState(privateData.pPageView);
  }
  if (pFormFillEnv->GetFocusAnnot() == privateData.GetWidget())
    return {false, bExit};

  pFormFiller->CommitData(privateData.pPageView, nFlag);
  return {false, true};
}

bool CFFL_InteractiveFormFiller::OnPopupPreOpen(
    const IPWL_SystemHandler::PerWindowData* pAttached,
    uint32_t nFlag) {
#ifdef PDF_ENABLE_XFA
  auto* pData = static_cast<const CFFL_PrivateData*>(pAttached);
  ASSERT(pData->pWidget);

  ObservedPtr<CPDFSDK_Annot> pObserved(pData->GetWidget());
  return OnPreOpen(&pObserved, pData->pPageView, nFlag) || !pObserved;
#else
  return false;
#endif
}

bool CFFL_InteractiveFormFiller::OnPopupPostOpen(
    const IPWL_SystemHandler::PerWindowData* pAttached,
    uint32_t nFlag) {
#ifdef PDF_ENABLE_XFA
  auto* pData = static_cast<const CFFL_PrivateData*>(pAttached);
  ASSERT(pData->pWidget);

  ObservedPtr<CPDFSDK_Annot> pObserved(pData->GetWidget());
  return OnPostOpen(&pObserved, pData->pPageView, nFlag) || !pObserved;
#else
  return false;
#endif
}

CFFL_PrivateData::CFFL_PrivateData() = default;

CFFL_PrivateData::CFFL_PrivateData(const CFFL_PrivateData& that) = default;

CFFL_PrivateData::~CFFL_PrivateData() = default;

std::unique_ptr<IPWL_SystemHandler::PerWindowData> CFFL_PrivateData::Clone()
    const {
  return pdfium::MakeUnique<CFFL_PrivateData>(*this);
}
