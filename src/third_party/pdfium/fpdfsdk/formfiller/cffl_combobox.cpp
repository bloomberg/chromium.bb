// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_combobox.h"

#include <utility>

#include "constants/form_flags.h"
#include "core/fpdfdoc/cba_fontmap.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/formfiller/cffl_interactiveformfiller.h"
#include "fpdfsdk/pwl/cpwl_combo_box.h"
#include "third_party/base/ptr_util.h"

CFFL_ComboBox::CFFL_ComboBox(CPDFSDK_FormFillEnvironment* pApp,
                             CPDFSDK_Widget* pWidget)
    : CFFL_TextObject(pApp, pWidget) {
}

CFFL_ComboBox::~CFFL_ComboBox() {
  for (const auto& it : m_Maps)
    it.second->InvalidateFocusHandler(this);

  // See comment in cffl_formfiller.h.
  // The font map should be stored somewhere more appropriate so it will live
  // until the PWL_Edit is done with it. pdfium:566
  DestroyWindows();
}

CPWL_Wnd::CreateParams CFFL_ComboBox::GetCreateParam() {
  CPWL_Wnd::CreateParams cp = CFFL_TextObject::GetCreateParam();
  if (m_pWidget->GetFieldFlags() & pdfium::form_flags::kChoiceEdit)
    cp.dwFlags |= PCBS_ALLOWCUSTOMTEXT;

  cp.pFontMap = MaybeCreateFontMap();
  cp.pFocusHandler = this;
  return cp;
}

std::unique_ptr<CPWL_Wnd> CFFL_ComboBox::NewPWLWindow(
    const CPWL_Wnd::CreateParams& cp,
    std::unique_ptr<IPWL_SystemHandler::PerWindowData> pAttachedData) {
  auto pWnd = pdfium::MakeUnique<CPWL_ComboBox>(cp, std::move(pAttachedData));
  pWnd->AttachFFLData(this);
  pWnd->Realize();

  CFFL_InteractiveFormFiller* pFormFiller =
      m_pFormFillEnv->GetInteractiveFormFiller();
  pWnd->SetFillerNotify(pFormFiller);

  int32_t nCurSel = m_pWidget->GetSelectedIndex(0);
  WideString swText;
  if (nCurSel < 0)
    swText = m_pWidget->GetValue();
  else
    swText = m_pWidget->GetOptionLabel(nCurSel);

  for (int32_t i = 0, sz = m_pWidget->CountOptions(); i < sz; i++)
    pWnd->AddString(m_pWidget->GetOptionLabel(i));

  pWnd->SetSelect(nCurSel);
  pWnd->SetText(swText);
  return std::move(pWnd);
}

bool CFFL_ComboBox::OnChar(CPDFSDK_Annot* pAnnot,
                           uint32_t nChar,
                           uint32_t nFlags) {
  return CFFL_TextObject::OnChar(pAnnot, nChar, nFlags);
}

bool CFFL_ComboBox::IsDataChanged(CPDFSDK_PageView* pPageView) {
  auto* pWnd = GetComboBox(pPageView, false);
  if (!pWnd)
    return false;

  int32_t nCurSel = pWnd->GetSelect();
  if (!(m_pWidget->GetFieldFlags() & pdfium::form_flags::kChoiceEdit))
    return nCurSel != m_pWidget->GetSelectedIndex(0);

  if (nCurSel >= 0)
    return nCurSel != m_pWidget->GetSelectedIndex(0);

  return pWnd->GetText() != m_pWidget->GetValue();
}

void CFFL_ComboBox::SaveData(CPDFSDK_PageView* pPageView) {
  CPWL_ComboBox* pWnd = GetComboBox(pPageView, false);
  if (!pWnd)
    return;

  WideString swText = pWnd->GetText();
  int32_t nCurSel = pWnd->GetSelect();
  bool bSetValue = false;
  if (m_pWidget->GetFieldFlags() & pdfium::form_flags::kChoiceEdit)
    bSetValue = (nCurSel < 0) || (swText != m_pWidget->GetOptionLabel(nCurSel));

  if (bSetValue) {
    m_pWidget->SetValue(swText, NotificationOption::kDoNotNotify);
  } else {
    m_pWidget->GetSelectedIndex(0);
    m_pWidget->SetOptionSelection(nCurSel, true,
                                  NotificationOption::kDoNotNotify);
  }
  ObservedPtr<CPDFSDK_Widget> observed_widget(m_pWidget.Get());
  ObservedPtr<CFFL_ComboBox> observed_this(this);
  m_pWidget->ResetFieldAppearance();
  if (!observed_widget)
    return;

  m_pWidget->UpdateField();
  if (!observed_widget || !observed_this)
    return;

  SetChangeMark();
}

void CFFL_ComboBox::GetActionData(CPDFSDK_PageView* pPageView,
                                  CPDF_AAction::AActionType type,
                                  CPDFSDK_FieldAction& fa) {
  switch (type) {
    case CPDF_AAction::kKeyStroke:
      if (CPWL_ComboBox* pComboBox = GetComboBox(pPageView, false)) {
        if (CPWL_Edit* pEdit = pComboBox->GetEdit()) {
          fa.bFieldFull = pEdit->IsTextFull();
          std::tie(fa.nSelStart, fa.nSelEnd) = pEdit->GetSelection();
          fa.sValue = pEdit->GetText();
          fa.sChangeEx = GetSelectExportText();

          if (fa.bFieldFull) {
            fa.sChange.clear();
            fa.sChangeEx.clear();
          }
        }
      }
      break;
    case CPDF_AAction::kValidate:
      if (CPWL_ComboBox* pComboBox = GetComboBox(pPageView, false)) {
        if (CPWL_Edit* pEdit = pComboBox->GetEdit()) {
          fa.sValue = pEdit->GetText();
        }
      }
      break;
    case CPDF_AAction::kLoseFocus:
    case CPDF_AAction::kGetFocus:
      fa.sValue = m_pWidget->GetValue();
      break;
    default:
      break;
  }
}

void CFFL_ComboBox::SetActionData(CPDFSDK_PageView* pPageView,
                                  CPDF_AAction::AActionType type,
                                  const CPDFSDK_FieldAction& fa) {
  switch (type) {
    case CPDF_AAction::kKeyStroke:
      if (CPWL_ComboBox* pComboBox = GetComboBox(pPageView, false)) {
        if (CPWL_Edit* pEdit = pComboBox->GetEdit()) {
          pEdit->SetSelection(fa.nSelStart, fa.nSelEnd);
          pEdit->ReplaceSelection(fa.sChange);
        }
      }
      break;
    default:
      break;
  }
}

void CFFL_ComboBox::SaveState(CPDFSDK_PageView* pPageView) {
  CPWL_ComboBox* pComboBox = GetComboBox(pPageView, false);
  if (!pComboBox)
    return;

  m_State.nIndex = pComboBox->GetSelect();

  CPWL_Edit* pEdit = pComboBox->GetEdit();
  if (!pEdit)
    return;

  std::tie(m_State.nStart, m_State.nEnd) = pEdit->GetSelection();
  m_State.sValue = pEdit->GetText();
}

void CFFL_ComboBox::RestoreState(CPDFSDK_PageView* pPageView) {
  CPWL_ComboBox* pComboBox = GetComboBox(pPageView, true);
  if (!pComboBox)
    return;

  if (m_State.nIndex >= 0) {
    pComboBox->SetSelect(m_State.nIndex);
    return;
  }

  CPWL_Edit* pEdit = pComboBox->GetEdit();
  if (!pEdit)
    return;

  pEdit->SetText(m_State.sValue);
  pEdit->SetSelection(m_State.nStart, m_State.nEnd);
}

bool CFFL_ComboBox::SetIndexSelected(int index, bool selected) {
  if (!IsValid() || !selected)
    return false;

  if (index < 0 || index >= m_pWidget->CountOptions())
    return false;

  CPWL_ComboBox* pWnd = GetComboBox(GetCurPageView(), false);
  if (!pWnd)
    return false;

  pWnd->SetSelect(index);
  return true;
}

bool CFFL_ComboBox::IsIndexSelected(int index) {
  if (!IsValid())
    return false;

  if (index < 0 || index >= m_pWidget->CountOptions())
    return false;

  CPWL_ComboBox* pWnd = GetComboBox(GetCurPageView(), false);
  return pWnd && index == pWnd->GetSelect();
}

#ifdef PDF_ENABLE_XFA
bool CFFL_ComboBox::IsFieldFull(CPDFSDK_PageView* pPageView) {
  CPWL_ComboBox* pComboBox = GetComboBox(pPageView, false);
  if (!pComboBox)
    return false;

  CPWL_Edit* pEdit = pComboBox->GetEdit();
  return pEdit && pEdit->IsTextFull();
}
#endif  // PDF_ENABLE_XFA

void CFFL_ComboBox::OnSetFocus(CPWL_Edit* pEdit) {
  pEdit->SetCharSet(FX_CHARSET_ChineseSimplified);
  pEdit->SetReadyToInput();

  WideString wsText = pEdit->GetText();
  int nCharacters = wsText.GetLength();
  ByteString bsUTFText = wsText.ToUTF16LE();
  auto* pBuffer = reinterpret_cast<const unsigned short*>(bsUTFText.c_str());
  m_pFormFillEnv->OnSetFieldInputFocus(pBuffer, nCharacters, true);
}

WideString CFFL_ComboBox::GetSelectExportText() {
  WideString swRet;

  CPWL_ComboBox* pComboBox = GetComboBox(GetCurPageView(), false);
  int nExport = pComboBox ? pComboBox->GetSelect() : -1;

  if (nExport >= 0) {
    if (CPDF_FormField* pFormField = m_pWidget->GetFormField()) {
      swRet = pFormField->GetOptionValue(nExport);
      if (swRet.IsEmpty())
        swRet = pFormField->GetOptionLabel(nExport);
    }
  }

  return swRet;
}

CPWL_ComboBox* CFFL_ComboBox::GetComboBox(CPDFSDK_PageView* pPageView,
                                          bool bNew) {
  return static_cast<CPWL_ComboBox*>(GetPWLWindow(pPageView, bNew));
}
