// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/cxfa_ffcheckbutton.h"

#include <utility>
#include "third_party/base/ptr_util.h"
#include "xfa/fwl/cfwl_checkbox.h"
#include "xfa/fwl/cfwl_messagemouse.h"
#include "xfa/fwl/cfwl_notedriver.h"
#include "xfa/fwl/cfwl_widgetmgr.h"
#include "xfa/fxfa/cxfa_ffapp.h"
#include "xfa/fxfa/cxfa_ffdoc.h"
#include "xfa/fxfa/cxfa_ffdocview.h"
#include "xfa/fxfa/cxfa_ffexclgroup.h"
#include "xfa/fxfa/cxfa_fffield.h"
#include "xfa/fxfa/cxfa_ffpageview.h"
#include "xfa/fxfa/cxfa_ffwidget.h"
#include "xfa/fxfa/parser/cxfa_border.h"
#include "xfa/fxfa/parser/cxfa_caption.h"
#include "xfa/fxfa/parser/cxfa_checkbutton.h"
#include "xfa/fxfa/parser/cxfa_para.h"

CXFA_FFCheckButton::CXFA_FFCheckButton(CXFA_Node* pNode,
                                       CXFA_CheckButton* button)
    : CXFA_FFField(pNode), button_(button) {}

CXFA_FFCheckButton::~CXFA_FFCheckButton() = default;

bool CXFA_FFCheckButton::LoadWidget() {
  ASSERT(!IsLoaded());
  auto pNew = pdfium::MakeUnique<CFWL_CheckBox>(GetFWLApp());
  CFWL_CheckBox* pCheckBox = pNew.get();
  SetNormalWidget(std::move(pNew));
  pCheckBox->SetFFWidget(this);

  CFWL_NoteDriver* pNoteDriver = pCheckBox->GetOwnerApp()->GetNoteDriver();
  pNoteDriver->RegisterEventTarget(pCheckBox, pCheckBox);
  m_pOldDelegate = pCheckBox->GetDelegate();
  pCheckBox->SetDelegate(this);
  if (m_pNode->IsRadioButton())
    pCheckBox->ModifyStylesEx(FWL_STYLEEXT_CKB_RadioButton, 0xFFFFFFFF);

  {
    CFWL_Widget::ScopedUpdateLock update_lock(pCheckBox);
    UpdateWidgetProperty();
    SetFWLCheckState(m_pNode->GetCheckState());
  }

  return CXFA_FFField::LoadWidget();
}

void CXFA_FFCheckButton::UpdateWidgetProperty() {
  auto* pCheckBox = static_cast<CFWL_CheckBox*>(GetNormalWidget());
  if (!pCheckBox)
    return;

  pCheckBox->SetBoxSize(m_pNode->GetCheckButtonSize());
  uint32_t dwStyleEx = FWL_STYLEEXT_CKB_SignShapeCross;
  switch (button_->GetMark()) {
    case XFA_AttributeValue::Check:
      dwStyleEx = FWL_STYLEEXT_CKB_SignShapeCheck;
      break;
    case XFA_AttributeValue::Circle:
      dwStyleEx = FWL_STYLEEXT_CKB_SignShapeCircle;
      break;
    case XFA_AttributeValue::Cross:
      break;
    case XFA_AttributeValue::Diamond:
      dwStyleEx = FWL_STYLEEXT_CKB_SignShapeDiamond;
      break;
    case XFA_AttributeValue::Square:
      dwStyleEx = FWL_STYLEEXT_CKB_SignShapeSquare;
      break;
    case XFA_AttributeValue::Star:
      dwStyleEx = FWL_STYLEEXT_CKB_SignShapeStar;
      break;
    default: {
      if (button_->IsRound())
        dwStyleEx = FWL_STYLEEXT_CKB_SignShapeCircle;
    } break;
  }
  if (button_->IsAllowNeutral())
    dwStyleEx |= FWL_STYLEEXT_CKB_3State;

  pCheckBox->ModifyStylesEx(
      dwStyleEx, FWL_STYLEEXT_CKB_SignShapeMask | FWL_STYLEEXT_CKB_3State);
}

bool CXFA_FFCheckButton::PerformLayout() {
  CXFA_FFWidget::PerformLayout();

  float fCheckSize = m_pNode->GetCheckButtonSize();
  CXFA_Margin* margin = m_pNode->GetMarginIfExists();
  CFX_RectF rtWidget = GetRectWithoutRotate();
  XFA_RectWithoutMargin(&rtWidget, margin);

  XFA_AttributeValue iCapPlacement = XFA_AttributeValue::Unknown;
  float fCapReserve = 0;
  CXFA_Caption* caption = m_pNode->GetCaptionIfExists();
  if (caption && caption->IsVisible()) {
    m_rtCaption = rtWidget;
    iCapPlacement = caption->GetPlacementType();
    fCapReserve = caption->GetReserve();
    if (fCapReserve <= 0) {
      if (iCapPlacement == XFA_AttributeValue::Top ||
          iCapPlacement == XFA_AttributeValue::Bottom) {
        fCapReserve = rtWidget.height - fCheckSize;
      } else {
        fCapReserve = rtWidget.width - fCheckSize;
      }
    }
  }

  XFA_AttributeValue iHorzAlign = XFA_AttributeValue::Left;
  XFA_AttributeValue iVertAlign = XFA_AttributeValue::Top;
  CXFA_Para* para = m_pNode->GetParaIfExists();
  if (para) {
    iHorzAlign = para->GetHorizontalAlign();
    iVertAlign = para->GetVerticalAlign();
  }

  m_rtUI = rtWidget;
  CXFA_Margin* captionMargin = caption ? caption->GetMarginIfExists() : nullptr;
  switch (iCapPlacement) {
    case XFA_AttributeValue::Left: {
      m_rtCaption.width = fCapReserve;
      CapLeftRightPlacement(captionMargin);
      m_rtUI.width -= fCapReserve;
      m_rtUI.left += fCapReserve;
      break;
    }
    case XFA_AttributeValue::Top: {
      m_rtCaption.height = fCapReserve;
      XFA_RectWithoutMargin(&m_rtCaption, captionMargin);
      m_rtUI.height -= fCapReserve;
      m_rtUI.top += fCapReserve;
      break;
    }
    case XFA_AttributeValue::Right: {
      m_rtCaption.left = m_rtCaption.right() - fCapReserve;
      m_rtCaption.width = fCapReserve;
      CapLeftRightPlacement(captionMargin);
      m_rtUI.width -= fCapReserve;
      break;
    }
    case XFA_AttributeValue::Bottom: {
      m_rtCaption.top = m_rtCaption.bottom() - fCapReserve;
      m_rtCaption.height = fCapReserve;
      XFA_RectWithoutMargin(&m_rtCaption, captionMargin);
      m_rtUI.height -= fCapReserve;
      break;
    }
    case XFA_AttributeValue::Inline:
      break;
    default:
      iHorzAlign = XFA_AttributeValue::Right;
      break;
  }

  if (iHorzAlign == XFA_AttributeValue::Center)
    m_rtUI.left += (m_rtUI.width - fCheckSize) / 2;
  else if (iHorzAlign == XFA_AttributeValue::Right)
    m_rtUI.left = m_rtUI.right() - fCheckSize;

  if (iVertAlign == XFA_AttributeValue::Middle)
    m_rtUI.top += (m_rtUI.height - fCheckSize) / 2;
  else if (iVertAlign == XFA_AttributeValue::Bottom)
    m_rtUI.top = m_rtUI.bottom() - fCheckSize;

  m_rtUI.width = fCheckSize;
  m_rtUI.height = fCheckSize;
  AddUIMargin(iCapPlacement);
  m_rtCheckBox = m_rtUI;
  CXFA_Border* borderUI = m_pNode->GetUIBorder();
  if (borderUI) {
    CXFA_Margin* borderMargin = borderUI->GetMarginIfExists();
    XFA_RectWithoutMargin(&m_rtUI, borderMargin);
  }

  m_rtUI.Normalize();
  LayoutCaption();
  SetFWLRect();
  if (GetNormalWidget())
    GetNormalWidget()->Update();

  return true;
}

void CXFA_FFCheckButton::CapLeftRightPlacement(
    const CXFA_Margin* captionMargin) {
  XFA_RectWithoutMargin(&m_rtCaption, captionMargin);
  if (m_rtCaption.height < 0)
    m_rtCaption.top += m_rtCaption.height;
  if (m_rtCaption.width < 0) {
    m_rtCaption.left += m_rtCaption.width;
    m_rtCaption.width = -m_rtCaption.width;
  }
}

void CXFA_FFCheckButton::AddUIMargin(XFA_AttributeValue iCapPlacement) {
  CFX_RectF rtUIMargin = m_pNode->GetUIMargin();
  m_rtUI.top -= rtUIMargin.top / 2 - rtUIMargin.height / 2;

  float fLeftAddRight = rtUIMargin.left + rtUIMargin.width;
  float fTopAddBottom = rtUIMargin.top + rtUIMargin.height;
  if (m_rtUI.width < fLeftAddRight) {
    if (iCapPlacement == XFA_AttributeValue::Right ||
        iCapPlacement == XFA_AttributeValue::Left) {
      m_rtUI.left -= fLeftAddRight - m_rtUI.width;
    } else {
      m_rtUI.left -= 2 * (fLeftAddRight - m_rtUI.width);
    }
    m_rtUI.width += 2 * (fLeftAddRight - m_rtUI.width);
  }
  if (m_rtUI.height < fTopAddBottom) {
    if (iCapPlacement == XFA_AttributeValue::Right)
      m_rtUI.left -= fTopAddBottom - m_rtUI.height;

    m_rtUI.top -= fTopAddBottom - m_rtUI.height;
    m_rtUI.height += 2 * (fTopAddBottom - m_rtUI.height);
  }
}

void CXFA_FFCheckButton::RenderWidget(CXFA_Graphics* pGS,
                                      const CFX_Matrix& matrix,
                                      HighlightOption highlight) {
  if (!HasVisibleStatus())
    return;

  CFX_Matrix mtRotate = GetRotateMatrix();
  mtRotate.Concat(matrix);

  CXFA_FFWidget::RenderWidget(pGS, mtRotate, highlight);
  DrawBorderWithFlag(pGS, m_pNode->GetUIBorder(), m_rtUI, mtRotate,
                     button_->IsRound());
  RenderCaption(pGS, &mtRotate);
  DrawHighlight(pGS, &mtRotate, highlight,
                button_->IsRound() ? kRoundShape : kSquareShape);
  CFX_Matrix mt(1, 0, 0, 1, m_rtCheckBox.left, m_rtCheckBox.top);
  mt.Concat(mtRotate);
  GetApp()->GetFWLWidgetMgr()->OnDrawWidget(GetNormalWidget(), pGS, mt);
}

bool CXFA_FFCheckButton::OnLButtonUp(uint32_t dwFlags,
                                     const CFX_PointF& point) {
  if (!GetNormalWidget() || !IsButtonDown())
    return false;

  SetButtonDown(false);
  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageMouse>(
      GetNormalWidget(), FWL_MouseCommand::LeftButtonUp, dwFlags,
      FWLToClient(point)));

  return true;
}

XFA_CHECKSTATE CXFA_FFCheckButton::FWLState2XFAState() {
  uint32_t dwState = GetNormalWidget()->GetStates();
  if (dwState & FWL_STATE_CKB_Checked)
    return XFA_CHECKSTATE_On;
  if (dwState & FWL_STATE_CKB_Neutral)
    return XFA_CHECKSTATE_Neutral;
  return XFA_CHECKSTATE_Off;
}

bool CXFA_FFCheckButton::CommitData() {
  XFA_CHECKSTATE eCheckState = FWLState2XFAState();
  m_pNode->SetCheckState(eCheckState, true);
  return true;
}

bool CXFA_FFCheckButton::IsDataChanged() {
  XFA_CHECKSTATE eCheckState = FWLState2XFAState();
  return m_pNode->GetCheckState() != eCheckState;
}

void CXFA_FFCheckButton::SetFWLCheckState(XFA_CHECKSTATE eCheckState) {
  if (eCheckState == XFA_CHECKSTATE_Neutral)
    GetNormalWidget()->SetStates(FWL_STATE_CKB_Neutral);
  else if (eCheckState == XFA_CHECKSTATE_On)
    GetNormalWidget()->SetStates(FWL_STATE_CKB_Checked);
  else
    GetNormalWidget()->RemoveStates(FWL_STATE_CKB_Checked);
}

bool CXFA_FFCheckButton::UpdateFWLData() {
  if (!GetNormalWidget())
    return false;

  XFA_CHECKSTATE eState = m_pNode->GetCheckState();
  SetFWLCheckState(eState);
  GetNormalWidget()->Update();
  return true;
}

void CXFA_FFCheckButton::OnProcessMessage(CFWL_Message* pMessage) {
  m_pOldDelegate->OnProcessMessage(pMessage);
}

void CXFA_FFCheckButton::OnProcessEvent(CFWL_Event* pEvent) {
  CXFA_FFField::OnProcessEvent(pEvent);
  switch (pEvent->GetType()) {
    case CFWL_Event::Type::CheckStateChanged: {
      CXFA_EventParam eParam;
      eParam.m_eType = XFA_EVENT_Change;
      eParam.m_wsPrevText = m_pNode->GetValue(XFA_VALUEPICTURE_Raw);

      CXFA_Node* exclNode = m_pNode->GetExclGroupIfExists();
      if (ProcessCommittedData()) {
        eParam.m_pTarget = exclNode;
        if (exclNode) {
          m_pDocView->AddValidateNode(exclNode);
          m_pDocView->AddCalculateNode(exclNode);
          exclNode->ProcessEvent(GetDocView(), XFA_AttributeValue::Change,
                                 &eParam);
        }
        eParam.m_pTarget = m_pNode.Get();
        m_pNode->ProcessEvent(GetDocView(), XFA_AttributeValue::Change,
                              &eParam);
      } else {
        SetFWLCheckState(m_pNode->GetCheckState());
      }
      if (exclNode) {
        eParam.m_pTarget = exclNode;
        exclNode->ProcessEvent(GetDocView(), XFA_AttributeValue::Click,
                               &eParam);
      }
      eParam.m_pTarget = m_pNode.Get();
      m_pNode->ProcessEvent(GetDocView(), XFA_AttributeValue::Click, &eParam);
      break;
    }
    default:
      break;
  }
  m_pOldDelegate->OnProcessEvent(pEvent);
}

void CXFA_FFCheckButton::OnDrawWidget(CXFA_Graphics* pGraphics,
                                      const CFX_Matrix& matrix) {
  m_pOldDelegate->OnDrawWidget(pGraphics, matrix);
}

FormFieldType CXFA_FFCheckButton::GetFormFieldType() {
  return FormFieldType::kXFA_CheckBox;
}
