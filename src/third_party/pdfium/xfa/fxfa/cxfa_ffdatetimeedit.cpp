// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/cxfa_ffdatetimeedit.h"

#include <utility>

#include "third_party/base/ptr_util.h"
#include "xfa/fwl/cfwl_datetimepicker.h"
#include "xfa/fwl/cfwl_eventselectchanged.h"
#include "xfa/fwl/cfwl_notedriver.h"
#include "xfa/fwl/cfwl_widget.h"
#include "xfa/fxfa/cxfa_eventparam.h"
#include "xfa/fxfa/cxfa_ffdoc.h"
#include "xfa/fxfa/cxfa_ffdocview.h"
#include "xfa/fxfa/parser/cxfa_localevalue.h"
#include "xfa/fxfa/parser/cxfa_para.h"
#include "xfa/fxfa/parser/cxfa_value.h"
#include "xfa/fxfa/parser/xfa_utils.h"

CXFA_FFDateTimeEdit::CXFA_FFDateTimeEdit(CXFA_Node* pNode)
    : CXFA_FFTextEdit(pNode) {}

CXFA_FFDateTimeEdit::~CXFA_FFDateTimeEdit() = default;

CFWL_DateTimePicker* CXFA_FFDateTimeEdit::GetPickerWidget() {
  return static_cast<CFWL_DateTimePicker*>(GetNormalWidget());
}

CFX_RectF CXFA_FFDateTimeEdit::GetBBox(FocusOption focus) {
  if (focus == kDrawFocus)
    return CFX_RectF();
  return CXFA_FFWidget::GetBBox(kDoNotDrawFocus);
}

bool CXFA_FFDateTimeEdit::PtInActiveRect(const CFX_PointF& point) {
  CFWL_DateTimePicker* pPicker = GetPickerWidget();
  return pPicker && pPicker->GetBBox().Contains(point);
}

bool CXFA_FFDateTimeEdit::LoadWidget() {
  ASSERT(!IsLoaded());

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retain_layout(m_pLayoutItem.Get());

  auto pNewPicker = pdfium::MakeUnique<CFWL_DateTimePicker>(GetFWLApp());
  CFWL_DateTimePicker* pWidget = pNewPicker.get();
  SetNormalWidget(std::move(pNewPicker));
  pWidget->SetAdapterIface(this);

  CFWL_NoteDriver* pNoteDriver = pWidget->GetOwnerApp()->GetNoteDriver();
  pNoteDriver->RegisterEventTarget(pWidget, pWidget);
  m_pOldDelegate = pWidget->GetDelegate();
  pWidget->SetDelegate(this);

  {
    CFWL_Widget::ScopedUpdateLock update_lock(pWidget);
    WideString wsText = m_pNode->GetValue(XFA_VALUEPICTURE_Display);
    pWidget->SetEditText(wsText);

    CXFA_Value* value = m_pNode->GetFormValueIfExists();
    if (value) {
      switch (value->GetChildValueClassID()) {
        case XFA_Element::Date: {
          if (!wsText.IsEmpty()) {
            CXFA_LocaleValue lcValue = XFA_GetLocaleValue(m_pNode.Get());
            CFX_DateTime date = lcValue.GetDate();
            if (date.IsSet())
              pWidget->SetCurSel(date.GetYear(), date.GetMonth(),
                                 date.GetDay());
          }
        } break;
        default:
          break;
      }
    }
    UpdateWidgetProperty();
  }

  return CXFA_FFField::LoadWidget();
}

void CXFA_FFDateTimeEdit::UpdateWidgetProperty() {
  CFWL_DateTimePicker* pPicker = GetPickerWidget();
  if (!pPicker)
    return;

  uint32_t dwExtendedStyle = FWL_STYLEEXT_DTP_ShortDateFormat;
  dwExtendedStyle |= UpdateUIProperty();
  dwExtendedStyle |= GetAlignment();
  GetNormalWidget()->ModifyStylesEx(dwExtendedStyle, 0xFFFFFFFF);

  uint32_t dwEditStyles = 0;
  Optional<int32_t> numCells = m_pNode->GetNumberOfCells();
  if (numCells && *numCells > 0) {
    dwEditStyles |= FWL_STYLEEXT_EDT_CombText;
    pPicker->SetEditLimit(*numCells);
  }
  if (!m_pNode->IsOpenAccess() || !GetDoc()->GetXFADoc()->IsInteractive())
    dwEditStyles |= FWL_STYLEEXT_EDT_ReadOnly;
  if (!m_pNode->IsHorizontalScrollPolicyOff())
    dwEditStyles |= FWL_STYLEEXT_EDT_AutoHScroll;

  pPicker->ModifyEditStylesEx(dwEditStyles, 0xFFFFFFFF);
}

uint32_t CXFA_FFDateTimeEdit::GetAlignment() {
  CXFA_Para* para = m_pNode->GetParaIfExists();
  if (!para)
    return 0;

  uint32_t dwExtendedStyle = 0;
  switch (para->GetHorizontalAlign()) {
    case XFA_AttributeValue::Center:
      dwExtendedStyle |= FWL_STYLEEXT_DTP_EditHCenter;
      break;
    case XFA_AttributeValue::Justify:
      dwExtendedStyle |= FWL_STYLEEXT_DTP_EditJustified;
      break;
    case XFA_AttributeValue::JustifyAll:
    case XFA_AttributeValue::Radix:
      break;
    case XFA_AttributeValue::Right:
      dwExtendedStyle |= FWL_STYLEEXT_DTP_EditHFar;
      break;
    default:
      dwExtendedStyle |= FWL_STYLEEXT_DTP_EditHNear;
      break;
  }

  switch (para->GetVerticalAlign()) {
    case XFA_AttributeValue::Middle:
      dwExtendedStyle |= FWL_STYLEEXT_DTP_EditVCenter;
      break;
    case XFA_AttributeValue::Bottom:
      dwExtendedStyle |= FWL_STYLEEXT_DTP_EditVFar;
      break;
    default:
      dwExtendedStyle |= FWL_STYLEEXT_DTP_EditVNear;
      break;
  }
  return dwExtendedStyle;
}

bool CXFA_FFDateTimeEdit::CommitData() {
  CFWL_DateTimePicker* pPicker = GetPickerWidget();
  if (!m_pNode->SetValue(XFA_VALUEPICTURE_Edit, pPicker->GetEditText()))
    return false;

  GetDoc()->GetDocView()->UpdateUIDisplay(m_pNode.Get(), this);
  return true;
}

bool CXFA_FFDateTimeEdit::UpdateFWLData() {
  if (!GetNormalWidget())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());
  XFA_VALUEPICTURE eType = XFA_VALUEPICTURE_Display;
  if (IsFocused())
    eType = XFA_VALUEPICTURE_Edit;

  WideString wsText = m_pNode->GetValue(eType);
  CFWL_DateTimePicker* pPicker = GetPickerWidget();
  pPicker->SetEditText(wsText);
  if (IsFocused() && !wsText.IsEmpty()) {
    CXFA_LocaleValue lcValue = XFA_GetLocaleValue(m_pNode.Get());
    CFX_DateTime date = lcValue.GetDate();
    if (lcValue.IsValid()) {
      if (date.IsSet())
        pPicker->SetCurSel(date.GetYear(), date.GetMonth(), date.GetDay());
    }
  }
  GetNormalWidget()->Update();
  return true;
}

bool CXFA_FFDateTimeEdit::IsDataChanged() {
  if (GetLayoutItem()->TestStatusBits(XFA_WidgetStatus_TextEditValueChanged))
    return true;

  WideString wsText = GetPickerWidget()->GetEditText();
  return m_pNode->GetValue(XFA_VALUEPICTURE_Edit) != wsText;
}

void CXFA_FFDateTimeEdit::OnSelectChanged(CFWL_Widget* pWidget,
                                          int32_t iYear,
                                          int32_t iMonth,
                                          int32_t iDay) {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  WideString wsPicture = m_pNode->GetPictureContent(XFA_VALUEPICTURE_Edit);
  CXFA_LocaleValue date(XFA_VT_DATE, GetDoc()->GetXFADoc()->GetLocaleMgr());
  date.SetDate(CFX_DateTime(iYear, iMonth, iDay, 0, 0, 0, 0));

  WideString wsDate;
  date.FormatPatterns(wsDate, wsPicture, m_pNode->GetLocale(),
                      XFA_VALUEPICTURE_Edit);

  CFWL_DateTimePicker* pPicker = GetPickerWidget();
  pPicker->SetEditText(wsDate);
  pPicker->Update();
  GetDoc()->GetDocEnvironment()->SetFocusWidget(GetDoc(), nullptr);

  CXFA_EventParam eParam;
  eParam.m_eType = XFA_EVENT_Change;
  eParam.m_pTarget = m_pNode.Get();
  eParam.m_wsPrevText = m_pNode->GetValue(XFA_VALUEPICTURE_Raw);
  m_pNode->ProcessEvent(GetDocView(), XFA_AttributeValue::Change, &eParam);
}

void CXFA_FFDateTimeEdit::OnProcessEvent(CFWL_Event* pEvent) {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retain_layout(m_pLayoutItem.Get());

  if (pEvent->GetType() == CFWL_Event::Type::SelectChanged) {
    auto* event = static_cast<CFWL_EventSelectChanged*>(pEvent);
    OnSelectChanged(GetNormalWidget(), event->iYear, event->iMonth,
                    event->iDay);
    return;
  }
  CXFA_FFTextEdit::OnProcessEvent(pEvent);
}

bool CXFA_FFDateTimeEdit::CanUndo() {
  return GetPickerWidget()->CanUndo();
}

bool CXFA_FFDateTimeEdit::CanRedo() {
  return GetPickerWidget()->CanRedo();
}

bool CXFA_FFDateTimeEdit::CanCopy() {
  return GetPickerWidget()->HasSelection();
}

bool CXFA_FFDateTimeEdit::CanCut() {
  if (GetPickerWidget()->GetStylesEx() & FWL_STYLEEXT_EDT_ReadOnly)
    return false;
  return GetPickerWidget()->HasSelection();
}

bool CXFA_FFDateTimeEdit::CanPaste() {
  return !(GetPickerWidget()->GetStylesEx() & FWL_STYLEEXT_EDT_ReadOnly);
}

bool CXFA_FFDateTimeEdit::CanSelectAll() {
  return GetPickerWidget()->GetEditTextLength() > 0;
}

Optional<WideString> CXFA_FFDateTimeEdit::Copy() {
  return GetPickerWidget()->Copy();
}

bool CXFA_FFDateTimeEdit::Undo() {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retain_layout(m_pLayoutItem.Get());

  return GetPickerWidget()->Undo();
}

bool CXFA_FFDateTimeEdit::Redo() {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retain_layout(m_pLayoutItem.Get());

  return GetPickerWidget()->Redo();
}

Optional<WideString> CXFA_FFDateTimeEdit::Cut() {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retain_layout(m_pLayoutItem.Get());

  return GetPickerWidget()->Cut();
}

bool CXFA_FFDateTimeEdit::Paste(const WideString& wsPaste) {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retain_layout(m_pLayoutItem.Get());

  return GetPickerWidget()->Paste(wsPaste);
}

void CXFA_FFDateTimeEdit::SelectAll() {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retain_layout(m_pLayoutItem.Get());

  GetPickerWidget()->SelectAll();
}

void CXFA_FFDateTimeEdit::Delete() {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retain_layout(m_pLayoutItem.Get());

  GetPickerWidget()->ClearText();
}

void CXFA_FFDateTimeEdit::DeSelect() {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retain_layout(m_pLayoutItem.Get());

  GetPickerWidget()->ClearSelection();
}

WideString CXFA_FFDateTimeEdit::GetText() {
  return GetPickerWidget()->GetEditText();
}
