// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/cfwl_scrollbar.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"
#include "xfa/fwl/cfwl_app.h"
#include "xfa/fwl/cfwl_messagemouse.h"
#include "xfa/fwl/cfwl_messagemousewheel.h"
#include "xfa/fwl/cfwl_notedriver.h"
#include "xfa/fwl/cfwl_themebackground.h"
#include "xfa/fwl/cfwl_themepart.h"
#include "xfa/fwl/ifwl_themeprovider.h"

#define FWL_SCROLLBAR_Elapse 500

namespace {

const float kMinThumbSize = 5.0f;

}  // namespace

CFWL_ScrollBar::CFWL_ScrollBar(
    const CFWL_App* app,
    std::unique_ptr<CFWL_WidgetProperties> properties,
    CFWL_Widget* pOuter)
    : CFWL_Widget(app, std::move(properties), pOuter) {}

CFWL_ScrollBar::~CFWL_ScrollBar() = default;

FWL_Type CFWL_ScrollBar::GetClassID() const {
  return FWL_Type::ScrollBar;
}

void CFWL_ScrollBar::Update() {
  if (IsLocked())
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  Layout();
}

void CFWL_ScrollBar::DrawWidget(CXFA_Graphics* pGraphics,
                                const CFX_Matrix& matrix) {
  if (!pGraphics)
    return;
  if (!m_pProperties->m_pThemeProvider)
    return;

  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider.Get();
  if (HasBorder())
    DrawBorder(pGraphics, CFWL_Part::Border, pTheme, matrix);
  DrawTrack(pGraphics, pTheme, true, &matrix);
  DrawTrack(pGraphics, pTheme, false, &matrix);
  DrawArrowBtn(pGraphics, pTheme, true, &matrix);
  DrawArrowBtn(pGraphics, pTheme, false, &matrix);
  DrawThumb(pGraphics, pTheme, &matrix);
}

void CFWL_ScrollBar::SetTrackPos(float fTrackPos) {
  m_fTrackPos = fTrackPos;
  m_ThumbRect = CalcThumbButtonRect(m_ThumbRect);
  m_MinTrackRect = CalcMinTrackRect(m_MinTrackRect);
  m_MaxTrackRect = CalcMaxTrackRect(m_MaxTrackRect);
}

bool CFWL_ScrollBar::DoScroll(CFWL_EventScroll::Code dwCode, float fPos) {
  if (dwCode == CFWL_EventScroll::Code::None)
    return false;
  return OnScroll(dwCode, fPos);
}

void CFWL_ScrollBar::DrawTrack(CXFA_Graphics* pGraphics,
                               IFWL_ThemeProvider* pTheme,
                               bool bLower,
                               const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = bLower ? CFWL_Part::LowerTrack : CFWL_Part::UpperTrack;
  param.m_dwStates = (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)
                         ? CFWL_PartState_Disabled
                         : (bLower ? m_iMinTrackState : m_iMaxTrackState);
  param.m_pGraphics = pGraphics;
  param.m_matrix.Concat(*pMatrix);
  param.m_PartRect = bLower ? m_MinTrackRect : m_MaxTrackRect;
  pTheme->DrawBackground(param);
}

void CFWL_ScrollBar::DrawArrowBtn(CXFA_Graphics* pGraphics,
                                  IFWL_ThemeProvider* pTheme,
                                  bool bMinBtn,
                                  const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = bMinBtn ? CFWL_Part::ForeArrow : CFWL_Part::BackArrow;
  param.m_dwStates = (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)
                         ? CFWL_PartState_Disabled
                         : (bMinBtn ? m_iMinButtonState : m_iMaxButtonState);
  param.m_pGraphics = pGraphics;
  param.m_matrix.Concat(*pMatrix);
  param.m_PartRect = bMinBtn ? m_MinBtnRect : m_MaxBtnRect;
  if (param.m_PartRect.height > 0 && param.m_PartRect.width > 0)
    pTheme->DrawBackground(param);
}

void CFWL_ScrollBar::DrawThumb(CXFA_Graphics* pGraphics,
                               IFWL_ThemeProvider* pTheme,
                               const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = CFWL_Part::Thumb;
  param.m_dwStates = (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)
                         ? CFWL_PartState_Disabled
                         : m_iThumbButtonState;
  param.m_pGraphics = pGraphics;
  param.m_matrix.Concat(*pMatrix);
  param.m_PartRect = m_ThumbRect;
  pTheme->DrawBackground(param);
}

void CFWL_ScrollBar::Layout() {
  m_ClientRect = GetClientRect();

  CalcButtonLen();
  m_MinBtnRect = CalcMinButtonRect();
  m_MaxBtnRect = CalcMaxButtonRect();
  m_ThumbRect = CalcThumbButtonRect(m_ThumbRect);
  m_MinTrackRect = CalcMinTrackRect(m_MinTrackRect);
  m_MaxTrackRect = CalcMaxTrackRect(m_MaxTrackRect);
}

void CFWL_ScrollBar::CalcButtonLen() {
  m_fButtonLen = IsVertical() ? m_ClientRect.width : m_ClientRect.height;
  float fLength = IsVertical() ? m_ClientRect.height : m_ClientRect.width;
  if (fLength < m_fButtonLen * 2) {
    m_fButtonLen = fLength / 2;
    m_bMinSize = true;
  } else {
    m_bMinSize = false;
  }
}

CFX_RectF CFWL_ScrollBar::CalcMinButtonRect() {
  if (IsVertical())
    return CFX_RectF(m_ClientRect.TopLeft(), m_ClientRect.width, m_fButtonLen);
  return CFX_RectF(m_ClientRect.TopLeft(), m_fButtonLen, m_ClientRect.height);
}

CFX_RectF CFWL_ScrollBar::CalcMaxButtonRect() {
  if (IsVertical()) {
    return CFX_RectF(m_ClientRect.left, m_ClientRect.bottom() - m_fButtonLen,
                     m_ClientRect.width, m_fButtonLen);
  }
  return CFX_RectF(m_ClientRect.right() - m_fButtonLen, m_ClientRect.top,
                   m_fButtonLen, m_ClientRect.height);
}

CFX_RectF CFWL_ScrollBar::CalcThumbButtonRect(const CFX_RectF& rtThumb) {
  CFX_RectF rect;
  if (!IsEnabled())
    return rect;

  if (m_bMinSize) {
    rect.left = rtThumb.left;
    rect.top = rtThumb.top;
    return rect;
  }

  float fRange = m_fRangeMax - m_fRangeMin;
  if (fRange < 0) {
    if (IsVertical()) {
      return CFX_RectF(m_ClientRect.left, m_MaxBtnRect.bottom(),
                       m_ClientRect.width, 0);
    }
    return CFX_RectF(m_MaxBtnRect.right(), m_ClientRect.top, 0,
                     m_ClientRect.height);
  }

  CFX_RectF rtClient = m_ClientRect;
  float fLength = IsVertical() ? rtClient.height : rtClient.width;
  float fSize = m_fButtonLen;
  fLength -= fSize * 2.0f;
  if (fLength < fSize)
    fLength = 0.0f;

  float fThumbSize = fLength * fLength / (fRange + fLength);
  fThumbSize = std::max(fThumbSize, kMinThumbSize);

  float fDiff = std::max(fLength - fThumbSize, 0.0f);
  float fTrackPos = pdfium::clamp(m_fTrackPos, m_fRangeMin, m_fRangeMax);
  if (!fRange)
    return rect;

  float iPos = fSize + fDiff * (fTrackPos - m_fRangeMin) / fRange;
  rect.left = rtClient.left;
  rect.top = rtClient.top;
  if (IsVertical()) {
    rect.top += iPos;
    rect.width = rtClient.width;
    rect.height = fThumbSize;
  } else {
    rect.left += iPos;
    rect.width = fThumbSize;
    rect.height = rtClient.height;
  }
  return rect;
}

CFX_RectF CFWL_ScrollBar::CalcMinTrackRect(const CFX_RectF& rtMinRect) {
  CFX_RectF rect;
  if (m_bMinSize) {
    rect.left = rtMinRect.left;
    rect.top = rtMinRect.top;
    return rect;
  }

  rect.left = m_ClientRect.left;
  rect.top = m_ClientRect.top;
  if (IsVertical()) {
    rect.width = m_ClientRect.width;
    rect.height = (m_ThumbRect.top + m_ThumbRect.bottom()) / 2;
  } else {
    rect.width = (m_ThumbRect.left + m_ThumbRect.right()) / 2;
    rect.height = m_ClientRect.height;
  }
  return rect;
}

CFX_RectF CFWL_ScrollBar::CalcMaxTrackRect(const CFX_RectF& rtMaxRect) {
  if (m_bMinSize)
    return CFX_RectF(rtMaxRect.TopLeft(), 0, 0);

  if (IsVertical()) {
    float iy = (m_ThumbRect.top + m_ThumbRect.bottom()) / 2;
    return CFX_RectF(m_ClientRect.left, iy, m_ClientRect.width,
                     m_ClientRect.bottom() - iy);
  }

  float ix = (m_ThumbRect.left + m_ThumbRect.right()) / 2;
  return CFX_RectF(ix, m_ClientRect.top, m_ClientRect.height - ix,
                   m_ClientRect.height);
}

float CFWL_ScrollBar::GetTrackPointPos(const CFX_PointF& point) {
  CFX_PointF diff = point - m_cpTrackPoint;
  float fRange = m_fRangeMax - m_fRangeMin;
  float fPos;

  if (IsVertical()) {
    fPos = fRange * diff.y /
           (m_MaxBtnRect.top - m_MinBtnRect.bottom() - m_ThumbRect.height);
  } else {
    fPos = fRange * diff.x /
           (m_MaxBtnRect.left - m_MinBtnRect.right() - m_ThumbRect.width);
  }

  fPos += m_fLastTrackPos;
  return pdfium::clamp(fPos, m_fRangeMin, m_fRangeMax);
}

bool CFWL_ScrollBar::SendEvent() {
  if (m_iMinButtonState == CFWL_PartState_Pressed) {
    DoScroll(CFWL_EventScroll::Code::StepBackward, m_fTrackPos);
    return false;
  }
  if (m_iMaxButtonState == CFWL_PartState_Pressed) {
    DoScroll(CFWL_EventScroll::Code::StepForward, m_fTrackPos);
    return false;
  }
  if (m_iMinTrackState == CFWL_PartState_Pressed) {
    DoScroll(CFWL_EventScroll::Code::PageBackward, m_fTrackPos);
    return m_ThumbRect.Contains(m_cpTrackPoint);
  }
  if (m_iMaxTrackState == CFWL_PartState_Pressed) {
    DoScroll(CFWL_EventScroll::Code::PageForward, m_fTrackPos);
    return m_ThumbRect.Contains(m_cpTrackPoint);
  }
  if (m_iMouseWheel) {
    CFWL_EventScroll::Code dwCode = m_iMouseWheel < 0
                                        ? CFWL_EventScroll::Code::StepForward
                                        : CFWL_EventScroll::Code::StepBackward;
    DoScroll(dwCode, m_fTrackPos);
  }
  return true;
}

bool CFWL_ScrollBar::OnScroll(CFWL_EventScroll::Code dwCode, float fPos) {
  CFWL_EventScroll ev(this);
  ev.m_iScrollCode = dwCode;
  ev.m_fPos = fPos;
  DispatchEvent(&ev);
  return true;
}

void CFWL_ScrollBar::OnProcessMessage(CFWL_Message* pMessage) {
  if (!pMessage)
    return;

  CFWL_Message::Type type = pMessage->GetType();
  if (type == CFWL_Message::Type::Mouse) {
    CFWL_MessageMouse* pMsg = static_cast<CFWL_MessageMouse*>(pMessage);
    switch (pMsg->m_dwCmd) {
      case FWL_MouseCommand::LeftButtonDown:
        OnLButtonDown(pMsg->m_pos);
        break;
      case FWL_MouseCommand::LeftButtonUp:
        OnLButtonUp(pMsg->m_pos);
        break;
      case FWL_MouseCommand::Move:
        OnMouseMove(pMsg->m_pos);
        break;
      case FWL_MouseCommand::Leave:
        OnMouseLeave();
        break;
      default:
        break;
    }
  } else if (type == CFWL_Message::Type::MouseWheel) {
    auto* pMsg = static_cast<CFWL_MessageMouseWheel*>(pMessage);
    OnMouseWheel(pMsg->delta());
  }
}

void CFWL_ScrollBar::OnDrawWidget(CXFA_Graphics* pGraphics,
                                  const CFX_Matrix& matrix) {
  DrawWidget(pGraphics, matrix);
}

void CFWL_ScrollBar::OnLButtonDown(const CFX_PointF& point) {
  if (!IsEnabled())
    return;

  m_bMouseDown = true;
  SetGrab(true);

  m_cpTrackPoint = point;
  m_fLastTrackPos = m_fTrackPos;
  if (m_MinBtnRect.Contains(point))
    DoMouseDown(0, m_MinBtnRect, m_iMinButtonState, point);
  else if (m_ThumbRect.Contains(point))
    DoMouseDown(1, m_ThumbRect, m_iThumbButtonState, point);
  else if (m_MaxBtnRect.Contains(point))
    DoMouseDown(2, m_MaxBtnRect, m_iMaxButtonState, point);
  else if (m_MinTrackRect.Contains(point))
    DoMouseDown(3, m_MinTrackRect, m_iMinTrackState, point);
  else
    DoMouseDown(4, m_MaxTrackRect, m_iMaxTrackState, point);

  if (!SendEvent()) {
    m_pTimer = pdfium::MakeUnique<CFX_Timer>(
        GetOwnerApp()->GetAdapterNative()->GetTimerHandler(), this,
        FWL_SCROLLBAR_Elapse);
  }
}

void CFWL_ScrollBar::OnLButtonUp(const CFX_PointF& point) {
  m_pTimer.reset();
  m_bMouseDown = false;
  DoMouseUp(0, m_MinBtnRect, m_iMinButtonState, point);
  DoMouseUp(1, m_ThumbRect, m_iThumbButtonState, point);
  DoMouseUp(2, m_MaxBtnRect, m_iMaxButtonState, point);
  DoMouseUp(3, m_MinTrackRect, m_iMinTrackState, point);
  DoMouseUp(4, m_MaxTrackRect, m_iMaxTrackState, point);
  SetGrab(false);
}

void CFWL_ScrollBar::OnMouseMove(const CFX_PointF& point) {
  DoMouseMove(0, m_MinBtnRect, m_iMinButtonState, point);
  DoMouseMove(1, m_ThumbRect, m_iThumbButtonState, point);
  DoMouseMove(2, m_MaxBtnRect, m_iMaxButtonState, point);
  DoMouseMove(3, m_MinTrackRect, m_iMinTrackState, point);
  DoMouseMove(4, m_MaxTrackRect, m_iMaxTrackState, point);
}

void CFWL_ScrollBar::OnMouseLeave() {
  DoMouseLeave(0, m_MinBtnRect, m_iMinButtonState);
  DoMouseLeave(1, m_ThumbRect, m_iThumbButtonState);
  DoMouseLeave(2, m_MaxBtnRect, m_iMaxButtonState);
  DoMouseLeave(3, m_MinTrackRect, m_iMinTrackState);
  DoMouseLeave(4, m_MaxTrackRect, m_iMaxTrackState);
}

void CFWL_ScrollBar::OnMouseWheel(const CFX_Vector& delta) {
  m_iMouseWheel = delta.y;
  SendEvent();
  m_iMouseWheel = 0;
}

void CFWL_ScrollBar::DoMouseDown(int32_t iItem,
                                 const CFX_RectF& rtItem,
                                 int32_t& iState,
                                 const CFX_PointF& point) {
  if (!rtItem.Contains(point))
    return;
  if (iState == CFWL_PartState_Pressed)
    return;

  iState = CFWL_PartState_Pressed;
  RepaintRect(rtItem);
}

void CFWL_ScrollBar::DoMouseUp(int32_t iItem,
                               const CFX_RectF& rtItem,
                               int32_t& iState,
                               const CFX_PointF& point) {
  int32_t iNewState =
      rtItem.Contains(point) ? CFWL_PartState_Hovered : CFWL_PartState_Normal;
  if (iState == iNewState)
    return;

  iState = iNewState;
  RepaintRect(rtItem);
  OnScroll(CFWL_EventScroll::Code::EndScroll, m_fTrackPos);
}

void CFWL_ScrollBar::DoMouseMove(int32_t iItem,
                                 const CFX_RectF& rtItem,
                                 int32_t& iState,
                                 const CFX_PointF& point) {
  if (!m_bMouseDown) {
    int32_t iNewState =
        rtItem.Contains(point) ? CFWL_PartState_Hovered : CFWL_PartState_Normal;
    if (iState == iNewState)
      return;

    iState = iNewState;
    RepaintRect(rtItem);
  } else if ((2 == iItem) && (m_iThumbButtonState == CFWL_PartState_Pressed)) {
    m_fTrackPos = GetTrackPointPos(point);
    OnScroll(CFWL_EventScroll::Code::TrackPos, m_fTrackPos);
  }
}

void CFWL_ScrollBar::DoMouseLeave(int32_t iItem,
                                  const CFX_RectF& rtItem,
                                  int32_t& iState) {
  if (iState == CFWL_PartState_Normal)
    return;

  iState = CFWL_PartState_Normal;
  RepaintRect(rtItem);
}

void CFWL_ScrollBar::DoMouseHover(int32_t iItem,
                                  const CFX_RectF& rtItem,
                                  int32_t& iState) {
  if (iState == CFWL_PartState_Hovered)
    return;

  iState = CFWL_PartState_Hovered;
  RepaintRect(rtItem);
}

void CFWL_ScrollBar::OnTimerFired() {
  m_pTimer.reset();
  if (!SendEvent()) {
    m_pTimer = pdfium::MakeUnique<CFX_Timer>(
        GetOwnerApp()->GetAdapterNative()->GetTimerHandler(), this, 0);
  }
}
