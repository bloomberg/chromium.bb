// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_listbox.h"

#include <utility>

#include "constants/form_flags.h"
#include "core/fpdfdoc/cba_fontmap.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/formfiller/cffl_interactiveformfiller.h"
#include "fpdfsdk/pwl/cpwl_list_box.h"
#include "third_party/base/ptr_util.h"

CFFL_ListBox::CFFL_ListBox(CPDFSDK_FormFillEnvironment* pApp,
                           CPDFSDK_Widget* pWidget)
    : CFFL_TextObject(pApp, pWidget) {}

CFFL_ListBox::~CFFL_ListBox() = default;

CPWL_Wnd::CreateParams CFFL_ListBox::GetCreateParam() {
  CPWL_Wnd::CreateParams cp = CFFL_TextObject::GetCreateParam();
  uint32_t dwFieldFlag = m_pWidget->GetFieldFlags();
  if (dwFieldFlag & pdfium::form_flags::kChoiceMultiSelect)
    cp.dwFlags |= PLBS_MULTIPLESEL;

  cp.dwFlags |= PWS_VSCROLL;

  if (cp.dwFlags & PWS_AUTOFONTSIZE) {
    constexpr float kDefaultListBoxFontSize = 12.0f;
    cp.fFontSize = kDefaultListBoxFontSize;
  }

  cp.pFontMap = MaybeCreateFontMap();
  return cp;
}

std::unique_ptr<CPWL_Wnd> CFFL_ListBox::NewPWLWindow(
    const CPWL_Wnd::CreateParams& cp,
    std::unique_ptr<IPWL_SystemHandler::PerWindowData> pAttachedData) {
  auto pWnd = pdfium::MakeUnique<CPWL_ListBox>(cp, std::move(pAttachedData));
  pWnd->AttachFFLData(this);
  pWnd->Realize();
  pWnd->SetFillerNotify(m_pFormFillEnv->GetInteractiveFormFiller());

  for (int32_t i = 0, sz = m_pWidget->CountOptions(); i < sz; i++)
    pWnd->AddString(m_pWidget->GetOptionLabel(i));

  if (pWnd->HasFlag(PLBS_MULTIPLESEL)) {
    m_OriginSelections.clear();

    bool bSetCaret = false;
    for (int32_t i = 0, sz = m_pWidget->CountOptions(); i < sz; i++) {
      if (m_pWidget->IsOptionSelected(i)) {
        if (!bSetCaret) {
          pWnd->SetCaret(i);
          bSetCaret = true;
        }
        pWnd->Select(i);
        m_OriginSelections.insert(i);
      }
    }
  } else {
    for (int i = 0, sz = m_pWidget->CountOptions(); i < sz; i++) {
      if (m_pWidget->IsOptionSelected(i)) {
        pWnd->Select(i);
        break;
      }
    }
  }

  pWnd->SetTopVisibleIndex(m_pWidget->GetTopVisibleIndex());
  return std::move(pWnd);
}

bool CFFL_ListBox::OnChar(CPDFSDK_Annot* pAnnot,
                          uint32_t nChar,
                          uint32_t nFlags) {
  return CFFL_TextObject::OnChar(pAnnot, nChar, nFlags);
}

bool CFFL_ListBox::IsDataChanged(CPDFSDK_PageView* pPageView) {
  CPWL_ListBox* pListBox = GetListBox(pPageView);
  if (!pListBox)
    return false;

  if (m_pWidget->GetFieldFlags() & pdfium::form_flags::kChoiceMultiSelect) {
    size_t nSelCount = 0;
    for (int32_t i = 0, sz = pListBox->GetCount(); i < sz; ++i) {
      if (pListBox->IsItemSelected(i)) {
        if (m_OriginSelections.count(i) == 0)
          return true;

        ++nSelCount;
      }
    }

    return nSelCount != m_OriginSelections.size();
  }
  return pListBox->GetCurSel() != m_pWidget->GetSelectedIndex(0);
}

void CFFL_ListBox::SaveData(CPDFSDK_PageView* pPageView) {
  CPWL_ListBox* pListBox = GetListBox(pPageView);
  if (!pListBox)
    return;

  int32_t nNewTopIndex = pListBox->GetTopVisibleIndex();
  m_pWidget->ClearSelection(NotificationOption::kDoNotNotify);
  if (m_pWidget->GetFieldFlags() & pdfium::form_flags::kChoiceMultiSelect) {
    for (int32_t i = 0, sz = pListBox->GetCount(); i < sz; i++) {
      if (pListBox->IsItemSelected(i)) {
        m_pWidget->SetOptionSelection(i, true,
                                      NotificationOption::kDoNotNotify);
      }
    }
  } else {
    m_pWidget->SetOptionSelection(pListBox->GetCurSel(), true,
                                  NotificationOption::kDoNotNotify);
  }
  ObservedPtr<CPDFSDK_Widget> observed_widget(m_pWidget.Get());
  ObservedPtr<CFFL_ListBox> observed_this(this);
  m_pWidget->SetTopVisibleIndex(nNewTopIndex);
  if (!observed_widget)
    return;

  m_pWidget->ResetFieldAppearance();
  if (!observed_widget)
    return;

  m_pWidget->UpdateField();
  if (!observed_widget || !observed_this)
    return;

  SetChangeMark();
}

void CFFL_ListBox::GetActionData(CPDFSDK_PageView* pPageView,
                                 CPDF_AAction::AActionType type,
                                 CPDFSDK_FieldAction& fa) {
  switch (type) {
    case CPDF_AAction::kValidate:
      if (m_pWidget->GetFieldFlags() & pdfium::form_flags::kChoiceMultiSelect) {
        fa.sValue.clear();
      } else {
        CPWL_ListBox* pListBox = GetListBox(pPageView);
        if (pListBox) {
          int32_t nCurSel = pListBox->GetCurSel();
          if (nCurSel >= 0)
            fa.sValue = m_pWidget->GetOptionLabel(nCurSel);
        }
      }
      break;
    case CPDF_AAction::kLoseFocus:
    case CPDF_AAction::kGetFocus:
      if (m_pWidget->GetFieldFlags() & pdfium::form_flags::kChoiceMultiSelect) {
        fa.sValue.clear();
      } else {
        int32_t nCurSel = m_pWidget->GetSelectedIndex(0);
        if (nCurSel >= 0)
          fa.sValue = m_pWidget->GetOptionLabel(nCurSel);
      }
      break;
    default:
      break;
  }
}

void CFFL_ListBox::SaveState(CPDFSDK_PageView* pPageView) {
  CPWL_ListBox* pListBox = GetListBox(pPageView);
  if (!pListBox)
    return;

  for (int32_t i = 0, sz = pListBox->GetCount(); i < sz; i++) {
    if (pListBox->IsItemSelected(i))
      m_State.push_back(i);
  }
}

void CFFL_ListBox::RestoreState(CPDFSDK_PageView* pPageView) {
  CPWL_ListBox* pListBox = GetListBox(pPageView);
  if (!pListBox)
    return;

  for (const auto& item : m_State)
    pListBox->Select(item);
}

bool CFFL_ListBox::SetIndexSelected(int index, bool selected) {
  if (!IsValid())
    return false;

  if (index < 0 || index >= m_pWidget->CountOptions())
    return false;

  CPWL_ListBox* pListBox = GetListBox(GetCurPageView());
  if (!pListBox)
    return false;

  if (selected) {
    pListBox->Select(index);
    pListBox->SetCaret(index);
  } else {
    pListBox->Deselect(index);
    pListBox->SetCaret(index);
  }

  return true;
}

bool CFFL_ListBox::IsIndexSelected(int index) {
  if (!IsValid())
    return false;

  if (index < 0 || index >= m_pWidget->CountOptions())
    return false;

  CPWL_ListBox* pListBox = GetListBox(GetCurPageView());
  return pListBox && pListBox->IsItemSelected(index);
}

CPWL_ListBox* CFFL_ListBox::GetListBox(CPDFSDK_PageView* pPageView) {
  return static_cast<CPWL_ListBox*>(GetPWLWindow(pPageView, /*bNew=*/false));
}
