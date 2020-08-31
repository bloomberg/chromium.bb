// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/cxfa_fffield.h"

#include <algorithm>
#include <utility>

#include "core/fxcrt/fx_memory.h"
#include "core/fxge/render_defines.h"
#include "third_party/base/ptr_util.h"
#include "xfa/fwl/cfwl_edit.h"
#include "xfa/fwl/cfwl_eventmouse.h"
#include "xfa/fwl/cfwl_messagekey.h"
#include "xfa/fwl/cfwl_messagekillfocus.h"
#include "xfa/fwl/cfwl_messagemouse.h"
#include "xfa/fwl/cfwl_messagemousewheel.h"
#include "xfa/fwl/cfwl_messagesetfocus.h"
#include "xfa/fwl/cfwl_picturebox.h"
#include "xfa/fwl/cfwl_widgetmgr.h"
#include "xfa/fwl/fwl_widgetdef.h"
#include "xfa/fxfa/cxfa_ffapp.h"
#include "xfa/fxfa/cxfa_ffdoc.h"
#include "xfa/fxfa/cxfa_ffdocview.h"
#include "xfa/fxfa/cxfa_ffpageview.h"
#include "xfa/fxfa/cxfa_ffwidget.h"
#include "xfa/fxfa/cxfa_fwltheme.h"
#include "xfa/fxfa/cxfa_textlayout.h"
#include "xfa/fxfa/parser/cxfa_border.h"
#include "xfa/fxfa/parser/cxfa_calculate.h"
#include "xfa/fxfa/parser/cxfa_caption.h"
#include "xfa/fxfa/parser/cxfa_margin.h"
#include "xfa/fxfa/parser/cxfa_node.h"
#include "xfa/fxfa/parser/cxfa_script.h"
#include "xfa/fxgraphics/cxfa_gecolor.h"
#include "xfa/fxgraphics/cxfa_gepath.h"

CXFA_FFField::CXFA_FFField(CXFA_Node* pNode) : CXFA_FFWidget(pNode) {}

CXFA_FFField::~CXFA_FFField() = default;

CXFA_FFDropDown* CXFA_FFField::AsDropDown() {
  return nullptr;
}

CXFA_FFField* CXFA_FFField::AsField() {
  return this;
}

CFX_RectF CXFA_FFField::GetBBox(FocusOption focus) {
  if (focus == kDoNotDrawFocus)
    return CXFA_FFWidget::GetBBox(kDoNotDrawFocus);

  switch (m_pNode->GetFFWidgetType()) {
    case XFA_FFWidgetType::kButton:
    case XFA_FFWidgetType::kCheckButton:
    case XFA_FFWidgetType::kImageEdit:
    case XFA_FFWidgetType::kSignature:
    case XFA_FFWidgetType::kChoiceList:
      return GetRotateMatrix().TransformRect(m_UIRect);
    default:
      return CFX_RectF();
  }
}

void CXFA_FFField::RenderWidget(CXFA_Graphics* pGS,
                                const CFX_Matrix& matrix,
                                HighlightOption highlight) {
  if (!HasVisibleStatus())
    return;

  CFX_Matrix mtRotate = GetRotateMatrix();
  mtRotate.Concat(matrix);

  CXFA_FFWidget::RenderWidget(pGS, mtRotate, highlight);
  DrawBorder(pGS, m_pNode->GetUIBorder(), m_UIRect, mtRotate);
  RenderCaption(pGS, &mtRotate);
  DrawHighlight(pGS, &mtRotate, highlight, kSquareShape);

  CFX_RectF rtWidget = GetNormalWidget()->GetWidgetRect();
  CFX_Matrix mt(1, 0, 0, 1, rtWidget.left, rtWidget.top);
  mt.Concat(mtRotate);
  GetApp()->GetFWLWidgetMgr()->OnDrawWidget(GetNormalWidget(), pGS, mt);
}

void CXFA_FFField::DrawHighlight(CXFA_Graphics* pGS,
                                 CFX_Matrix* pMatrix,
                                 HighlightOption highlight,
                                 ShapeOption shape) {
  if (highlight == kNoHighlight)
    return;

  if (m_UIRect.IsEmpty() || !GetDoc()->GetXFADoc()->IsInteractive() ||
      !m_pNode->IsOpenAccess()) {
    return;
  }
  CXFA_FFDoc* pDoc = GetDoc();
  pGS->SetFillColor(
      CXFA_GEColor(pDoc->GetDocEnvironment()->GetHighlightColor(pDoc)));
  CXFA_GEPath path;
  if (shape == kRoundShape)
    path.AddEllipse(m_UIRect);
  else
    path.AddRectangle(m_UIRect.left, m_UIRect.top, m_UIRect.width,
                      m_UIRect.height);

  pGS->FillPath(&path, FXFILL_WINDING, pMatrix);
}

void CXFA_FFField::DrawFocus(CXFA_Graphics* pGS, CFX_Matrix* pMatrix) {
  if (!GetLayoutItem()->TestStatusBits(XFA_WidgetStatus_Focused))
    return;

  pGS->SetStrokeColor(CXFA_GEColor(0xFF000000));

  static constexpr float kDashPattern[2] = {1, 1};
  pGS->SetLineDash(0.0f, kDashPattern, FX_ArraySize(kDashPattern));
  pGS->SetLineWidth(0);

  CXFA_GEPath path;
  path.AddRectangle(m_UIRect.left, m_UIRect.top, m_UIRect.width,
                    m_UIRect.height);
  pGS->StrokePath(&path, pMatrix);
}

void CXFA_FFField::SetFWLThemeProvider() {
  if (GetNormalWidget())
    GetNormalWidget()->SetThemeProvider(GetApp()->GetFWLTheme(GetDoc()));
}

CFWL_Widget* CXFA_FFField::GetNormalWidget() {
  return m_pNormalWidget.get();
}

const CFWL_Widget* CXFA_FFField::GetNormalWidget() const {
  return m_pNormalWidget.get();
}

void CXFA_FFField::SetNormalWidget(std::unique_ptr<CFWL_Widget> widget) {
  m_pNormalWidget = std::move(widget);
}

bool CXFA_FFField::IsLoaded() {
  return GetNormalWidget() && CXFA_FFWidget::IsLoaded();
}

bool CXFA_FFField::LoadWidget() {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retain_layout(m_pLayoutItem.Get());

  SetFWLThemeProvider();
  m_pNode->LoadCaption(GetDoc());
  PerformLayout();
  return true;
}

void CXFA_FFField::SetEditScrollOffset() {
  XFA_FFWidgetType eType = m_pNode->GetFFWidgetType();
  if (eType != XFA_FFWidgetType::kTextEdit &&
      eType != XFA_FFWidgetType::kNumericEdit &&
      eType != XFA_FFWidgetType::kPasswordEdit) {
    return;
  }

  float fScrollOffset = 0;
  CXFA_ContentLayoutItem* pItem = GetLayoutItem()->GetPrev();
  CXFA_FFField* pPrev = pItem ? ToField(pItem->GetFFWidget()) : nullptr;
  if (pPrev)
    fScrollOffset = -(m_pNode->GetUIMargin().top);

  while (pPrev) {
    fScrollOffset += pPrev->m_UIRect.height;
    pItem = pPrev->GetLayoutItem()->GetPrev();
    pPrev = pItem ? ToField(pItem->GetFFWidget()) : nullptr;
  }
  static_cast<CFWL_Edit*>(GetNormalWidget())->SetScrollOffset(fScrollOffset);
}

bool CXFA_FFField::PerformLayout() {
  CXFA_FFWidget::PerformLayout();
  CapPlacement();
  LayoutCaption();
  SetFWLRect();
  SetEditScrollOffset();
  if (GetNormalWidget())
    GetNormalWidget()->Update();
  return true;
}

void CXFA_FFField::CapPlacement() {
  CFX_RectF rtWidget = GetRectWithoutRotate();
  CXFA_Margin* margin = m_pNode->GetMarginIfExists();
  if (margin) {
    CXFA_ContentLayoutItem* pItem = GetLayoutItem();
    float fLeftInset = margin->GetLeftInset();
    float fRightInset = margin->GetRightInset();
    float fTopInset = margin->GetTopInset();
    float fBottomInset = margin->GetBottomInset();
    if (!pItem->GetPrev() && !pItem->GetNext()) {
      rtWidget.Deflate(fLeftInset, fTopInset, fRightInset, fBottomInset);
    } else {
      if (!pItem->GetPrev())
        rtWidget.Deflate(fLeftInset, fTopInset, fRightInset, 0);
      else if (!pItem->GetNext())
        rtWidget.Deflate(fLeftInset, 0, fRightInset, fBottomInset);
      else
        rtWidget.Deflate(fLeftInset, 0, fRightInset, 0);
    }
  }

  XFA_AttributeValue iCapPlacement = XFA_AttributeValue::Unknown;
  float fCapReserve = 0;
  CXFA_Caption* caption = m_pNode->GetCaptionIfExists();
  if (caption && !caption->IsHidden()) {
    iCapPlacement = caption->GetPlacementType();
    if ((iCapPlacement == XFA_AttributeValue::Top &&
         GetLayoutItem()->GetPrev()) ||
        (iCapPlacement == XFA_AttributeValue::Bottom &&
         GetLayoutItem()->GetNext())) {
      m_CaptionRect = CFX_RectF();
    } else {
      fCapReserve = caption->GetReserve();
      if (iCapPlacement == XFA_AttributeValue::Top ||
          iCapPlacement == XFA_AttributeValue::Bottom) {
        fCapReserve = std::min(fCapReserve, rtWidget.height);
      } else {
        fCapReserve = std::min(fCapReserve, rtWidget.width);
      }
      CXFA_ContentLayoutItem* pItem = GetLayoutItem();
      if (!pItem->GetPrev() && !pItem->GetNext()) {
        m_CaptionRect = rtWidget;
      } else {
        pItem = pItem->GetFirst();
        m_CaptionRect = pItem->GetRect(false);
        pItem = pItem->GetNext();
        while (pItem) {
          m_CaptionRect.height += pItem->GetRect(false).Height();
          pItem = pItem->GetNext();
        }
        XFA_RectWithoutMargin(&m_CaptionRect, margin);
      }

      CXFA_TextLayout* pCapTextLayout = m_pNode->GetCaptionTextLayout();
      if (fCapReserve <= 0 && pCapTextLayout) {
        CFX_SizeF minSize;
        CFX_SizeF maxSize;
        CFX_SizeF size = pCapTextLayout->CalcSize(minSize, maxSize);
        if (iCapPlacement == XFA_AttributeValue::Top ||
            iCapPlacement == XFA_AttributeValue::Bottom) {
          fCapReserve = size.height;
        } else {
          fCapReserve = size.width;
        }
      }
    }
  }

  m_UIRect = rtWidget;
  CXFA_Margin* capMargin = caption ? caption->GetMarginIfExists() : nullptr;
  switch (iCapPlacement) {
    case XFA_AttributeValue::Left: {
      m_CaptionRect.width = fCapReserve;
      CapLeftRightPlacement(capMargin, rtWidget, iCapPlacement);
      m_UIRect.width -= fCapReserve;
      m_UIRect.left += fCapReserve;
      break;
    }
    case XFA_AttributeValue::Top: {
      m_CaptionRect.height = fCapReserve;
      CapTopBottomPlacement(capMargin, rtWidget, iCapPlacement);
      m_UIRect.top += fCapReserve;
      m_UIRect.height -= fCapReserve;
      break;
    }
    case XFA_AttributeValue::Right: {
      m_CaptionRect.left = m_CaptionRect.right() - fCapReserve;
      m_CaptionRect.width = fCapReserve;
      CapLeftRightPlacement(capMargin, rtWidget, iCapPlacement);
      m_UIRect.width -= fCapReserve;
      break;
    }
    case XFA_AttributeValue::Bottom: {
      m_CaptionRect.top = m_CaptionRect.bottom() - fCapReserve;
      m_CaptionRect.height = fCapReserve;
      CapTopBottomPlacement(capMargin, rtWidget, iCapPlacement);
      m_UIRect.height -= fCapReserve;
      break;
    }
    case XFA_AttributeValue::Inline:
      break;
    default:
      break;
  }

  CXFA_Border* borderUI = m_pNode->GetUIBorder();
  if (borderUI) {
    CXFA_Margin* borderMargin = borderUI->GetMarginIfExists();
    XFA_RectWithoutMargin(&m_UIRect, borderMargin);
  }
  m_UIRect.Normalize();
}

void CXFA_FFField::CapTopBottomPlacement(const CXFA_Margin* margin,
                                         const CFX_RectF& rtWidget,
                                         XFA_AttributeValue iCapPlacement) {
  CFX_RectF rtUIMargin = m_pNode->GetUIMargin();
  m_CaptionRect.left += rtUIMargin.left;
  if (margin) {
    XFA_RectWithoutMargin(&m_CaptionRect, margin);
    if (m_CaptionRect.height < 0)
      m_CaptionRect.top += m_CaptionRect.height;
  }

  float fWidth = rtUIMargin.left + rtUIMargin.width;
  float fHeight = m_CaptionRect.height + rtUIMargin.top + rtUIMargin.height;
  if (fWidth > rtWidget.width)
    m_UIRect.width += fWidth - rtWidget.width;

  if (fHeight == XFA_DEFAULTUI_HEIGHT && m_UIRect.height < XFA_MINUI_HEIGHT) {
    m_UIRect.height = XFA_MINUI_HEIGHT;
    m_CaptionRect.top += rtUIMargin.top + rtUIMargin.height;
  } else if (fHeight > rtWidget.height) {
    m_UIRect.height += fHeight - rtWidget.height;
    if (iCapPlacement == XFA_AttributeValue::Bottom)
      m_CaptionRect.top += fHeight - rtWidget.height;
  }
}

void CXFA_FFField::CapLeftRightPlacement(const CXFA_Margin* margin,
                                         const CFX_RectF& rtWidget,
                                         XFA_AttributeValue iCapPlacement) {
  CFX_RectF rtUIMargin = m_pNode->GetUIMargin();
  m_CaptionRect.top += rtUIMargin.top;
  m_CaptionRect.height -= rtUIMargin.top;
  if (margin) {
    XFA_RectWithoutMargin(&m_CaptionRect, margin);
    if (m_CaptionRect.height < 0)
      m_CaptionRect.top += m_CaptionRect.height;
  }

  float fWidth = m_CaptionRect.width + rtUIMargin.left + rtUIMargin.width;
  float fHeight = rtUIMargin.top + rtUIMargin.height;
  if (fWidth > rtWidget.width) {
    m_UIRect.width += fWidth - rtWidget.width;
    if (iCapPlacement == XFA_AttributeValue::Right)
      m_CaptionRect.left += fWidth - rtWidget.width;
  }

  if (fHeight == XFA_DEFAULTUI_HEIGHT && m_UIRect.height < XFA_MINUI_HEIGHT) {
    m_UIRect.height = XFA_MINUI_HEIGHT;
    m_CaptionRect.top += rtUIMargin.top + rtUIMargin.height;
  } else if (fHeight > rtWidget.height) {
    m_UIRect.height += fHeight - rtWidget.height;
  }
}

void CXFA_FFField::UpdateFWL() {
  if (GetNormalWidget())
    GetNormalWidget()->Update();
}

uint32_t CXFA_FFField::UpdateUIProperty() {
  CXFA_Node* pUiNode = m_pNode->GetUIChildNode();
  if (pUiNode && pUiNode->GetElementType() == XFA_Element::DefaultUi)
    return FWL_STYLEEXT_EDT_ReadOnly;
  return 0;
}

void CXFA_FFField::SetFWLRect() {
  if (!GetNormalWidget())
    return;

  CFX_RectF rtUi = m_UIRect;
  rtUi.width = std::max(rtUi.width, 1.0f);
  if (!GetDoc()->GetXFADoc()->IsInteractive()) {
    float fFontSize = m_pNode->GetFontSize();
    rtUi.height = std::max(rtUi.height, fFontSize);
  }
  GetNormalWidget()->SetWidgetRect(rtUi);
}

bool CXFA_FFField::OnMouseEnter() {
  if (!GetNormalWidget())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageMouse>(
      GetNormalWidget(), FWL_MouseCommand::Enter));

  return true;
}

bool CXFA_FFField::OnMouseExit() {
  if (!GetNormalWidget())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageMouse>(
      GetNormalWidget(), FWL_MouseCommand::Leave));

  return true;
}

CFX_PointF CXFA_FFField::FWLToClient(const CFX_PointF& point) {
  return GetNormalWidget()
             ? point - GetNormalWidget()->GetWidgetRect().TopLeft()
             : point;
}

bool CXFA_FFField::AcceptsFocusOnButtonDown(uint32_t dwFlags,
                                            const CFX_PointF& point,
                                            FWL_MouseCommand command) {
  if (!GetNormalWidget())
    return false;
  if (!m_pNode->IsOpenAccess() || !GetDoc()->GetXFADoc()->IsInteractive())
    return false;
  if (!PtInActiveRect(point))
    return false;

  return true;
}

bool CXFA_FFField::OnLButtonDown(uint32_t dwFlags, const CFX_PointF& point) {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SetButtonDown(true);
  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageMouse>(
      GetNormalWidget(), FWL_MouseCommand::LeftButtonDown, dwFlags,
      FWLToClient(point)));

  return true;
}

bool CXFA_FFField::OnLButtonUp(uint32_t dwFlags, const CFX_PointF& point) {
  if (!GetNormalWidget())
    return false;
  if (!IsButtonDown())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SetButtonDown(false);
  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageMouse>(
      GetNormalWidget(), FWL_MouseCommand::LeftButtonUp, dwFlags,
      FWLToClient(point)));

  return true;
}

bool CXFA_FFField::OnLButtonDblClk(uint32_t dwFlags, const CFX_PointF& point) {
  if (!GetNormalWidget())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageMouse>(
      GetNormalWidget(), FWL_MouseCommand::LeftButtonDblClk, dwFlags,
      FWLToClient(point)));

  return true;
}

bool CXFA_FFField::OnMouseMove(uint32_t dwFlags, const CFX_PointF& point) {
  if (!GetNormalWidget())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageMouse>(
      GetNormalWidget(), FWL_MouseCommand::Move, dwFlags, FWLToClient(point)));

  return true;
}

bool CXFA_FFField::OnMouseWheel(uint32_t dwFlags,
                                const CFX_PointF& point,
                                const CFX_Vector& delta) {
  if (!GetNormalWidget())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageMouseWheel>(
      GetNormalWidget(), FWLToClient(point), delta));

  return true;
}

bool CXFA_FFField::OnRButtonDown(uint32_t dwFlags, const CFX_PointF& point) {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SetButtonDown(true);
  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageMouse>(
      GetNormalWidget(), FWL_MouseCommand::RightButtonDown, dwFlags,
      FWLToClient(point)));

  return true;
}

bool CXFA_FFField::OnRButtonUp(uint32_t dwFlags, const CFX_PointF& point) {
  if (!GetNormalWidget())
    return false;
  if (!IsButtonDown())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SetButtonDown(false);
  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageMouse>(
      GetNormalWidget(), FWL_MouseCommand::RightButtonUp, dwFlags,
      FWLToClient(point)));

  return true;
}

bool CXFA_FFField::OnRButtonDblClk(uint32_t dwFlags, const CFX_PointF& point) {
  if (!GetNormalWidget())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageMouse>(
      GetNormalWidget(), FWL_MouseCommand::RightButtonDblClk, dwFlags,
      FWLToClient(point)));

  return true;
}

bool CXFA_FFField::OnSetFocus(CXFA_FFWidget* pOldWidget) {
  if (!CXFA_FFWidget::OnSetFocus(pOldWidget))
    return false;

  if (!GetNormalWidget())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SendMessageToFWLWidget(
      pdfium::MakeUnique<CFWL_MessageSetFocus>(nullptr, GetNormalWidget()));
  GetLayoutItem()->SetStatusBits(XFA_WidgetStatus_Focused);
  InvalidateRect();

  return true;
}

bool CXFA_FFField::OnKillFocus(CXFA_FFWidget* pNewWidget) {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  ObservedPtr<CXFA_FFWidget> pNewWatched(pNewWidget);
  if (GetNormalWidget()) {
    SendMessageToFWLWidget(
        pdfium::MakeUnique<CFWL_MessageKillFocus>(nullptr, GetNormalWidget()));
    GetLayoutItem()->ClearStatusBits(XFA_WidgetStatus_Focused);
    InvalidateRect();
  }
  return pNewWatched && CXFA_FFWidget::OnKillFocus(pNewWatched.Get());
}

bool CXFA_FFField::OnKeyDown(uint32_t dwKeyCode, uint32_t dwFlags) {
  if (!GetNormalWidget() || !GetDoc()->GetXFADoc()->IsInteractive())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageKey>(
      GetNormalWidget(), FWL_KeyCommand::KeyDown, dwFlags, dwKeyCode));

  return true;
}

bool CXFA_FFField::OnKeyUp(uint32_t dwKeyCode, uint32_t dwFlags) {
  if (!GetNormalWidget() || !GetDoc()->GetXFADoc()->IsInteractive())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageKey>(
      GetNormalWidget(), FWL_KeyCommand::KeyUp, dwFlags, dwKeyCode));

  return true;
}

bool CXFA_FFField::OnChar(uint32_t dwChar, uint32_t dwFlags) {
  if (!GetDoc()->GetXFADoc()->IsInteractive())
    return false;
  if (dwChar == XFA_FWL_VKEY_Tab)
    return true;
  if (!GetNormalWidget())
    return false;
  if (!m_pNode->IsOpenAccess())
    return false;

  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  SendMessageToFWLWidget(pdfium::MakeUnique<CFWL_MessageKey>(
      GetNormalWidget(), FWL_KeyCommand::Char, dwFlags, dwChar));

  return true;
}

FWL_WidgetHit CXFA_FFField::HitTest(const CFX_PointF& point) {
  auto* pNorm = GetNormalWidget();
  if (pNorm && pNorm->HitTest(FWLToClient(point)) != FWL_WidgetHit::Unknown)
    return FWL_WidgetHit::Client;
  if (!GetRectWithoutRotate().Contains(point))
    return FWL_WidgetHit::Unknown;
  if (m_CaptionRect.Contains(point))
    return FWL_WidgetHit::Titlebar;
  return FWL_WidgetHit::Border;
}

bool CXFA_FFField::PtInActiveRect(const CFX_PointF& point) {
  return GetNormalWidget() &&
         GetNormalWidget()->GetWidgetRect().Contains(point);
}

void CXFA_FFField::LayoutCaption() {
  CXFA_TextLayout* pCapTextLayout = m_pNode->GetCaptionTextLayout();
  if (!pCapTextLayout)
    return;

  float fHeight = pCapTextLayout->Layout(m_CaptionRect.Size());
  m_CaptionRect.height = std::max(m_CaptionRect.height, fHeight);
}

void CXFA_FFField::RenderCaption(CXFA_Graphics* pGS, CFX_Matrix* pMatrix) {
  CXFA_TextLayout* pCapTextLayout = m_pNode->GetCaptionTextLayout();
  if (!pCapTextLayout)
    return;

  CXFA_Caption* caption = m_pNode->GetCaptionIfExists();
  if (!caption || !caption->IsVisible())
    return;

  if (!pCapTextLayout->IsLoaded())
    pCapTextLayout->Layout(m_CaptionRect.Size());

  CFX_RectF rtClip = m_CaptionRect;
  rtClip.Intersect(GetRectWithoutRotate());
  CFX_RenderDevice* pRenderDevice = pGS->GetRenderDevice();
  CFX_Matrix mt(1, 0, 0, 1, m_CaptionRect.left, m_CaptionRect.top);
  if (pMatrix) {
    rtClip = pMatrix->TransformRect(rtClip);
    mt.Concat(*pMatrix);
  }
  pCapTextLayout->DrawString(pRenderDevice, mt, rtClip, 0);
}

bool CXFA_FFField::ProcessCommittedData() {
  if (!m_pNode->IsOpenAccess())
    return false;
  if (!IsDataChanged())
    return false;

  m_pDocView->SetChangeMark();
  m_pDocView->AddValidateNode(m_pNode.Get());

  if (CalculateOverride() != 1)
    return false;
  return CommitData();
}

int32_t CXFA_FFField::CalculateOverride() {
  CXFA_Node* exclNode = m_pNode->GetExclGroupIfExists();
  if (!exclNode || !exclNode->IsWidgetReady())
    return CalculateNode(m_pNode.Get());
  if (CalculateNode(exclNode) == 0)
    return 0;

  CXFA_Node* pNode = exclNode->GetExclGroupFirstMember();
  if (!pNode)
    return 1;

  while (pNode) {
    if (!pNode->IsWidgetReady())
      return 1;
    if (CalculateNode(pNode) == 0)
      return 0;

    pNode = pNode->GetExclGroupNextMember(pNode);
  }
  return 1;
}

int32_t CXFA_FFField::CalculateNode(CXFA_Node* pNode) {
  CXFA_Calculate* calc = pNode->GetCalculateIfExists();
  if (!calc)
    return 1;

  XFA_VERSION version = GetDoc()->GetXFADoc()->GetCurVersionMode();
  switch (calc->GetOverride()) {
    case XFA_AttributeValue::Error: {
      if (version <= XFA_VERSION_204)
        return 1;

      IXFA_AppProvider* pAppProvider = GetAppProvider();
      if (pAppProvider) {
        pAppProvider->MsgBox(
            WideString::FromASCII("You are not allowed to modify this field."),
            WideString::FromASCII("Calculate Override"),
            static_cast<uint32_t>(AlertIcon::kWarning),
            static_cast<uint32_t>(AlertButton::kOK));
      }
      return 0;
    }
    case XFA_AttributeValue::Warning: {
      if (version <= XFA_VERSION_204) {
        CXFA_Script* script = calc->GetScriptIfExists();
        if (!script || script->GetExpression().IsEmpty())
          return 1;
      }

      if (pNode->IsUserInteractive())
        return 1;

      IXFA_AppProvider* pAppProvider = GetAppProvider();
      if (!pAppProvider)
        return 0;

      WideString wsMessage = calc->GetMessageText();
      if (!wsMessage.IsEmpty())
        wsMessage += L"\r\n";
      wsMessage +=
          WideString::FromASCII("Are you sure you want to modify this field?");

      if (pAppProvider->MsgBox(wsMessage,
                               WideString::FromASCII("Calculate Override"),
                               static_cast<uint32_t>(AlertIcon::kWarning),
                               static_cast<uint32_t>(AlertButton::kYesNo)) ==
          static_cast<uint32_t>(AlertReturn::kYes)) {
        pNode->SetFlag(XFA_NodeFlag_UserInteractive);
        return 1;
      }
      return 0;
    }
    case XFA_AttributeValue::Ignore:
      return 0;
    case XFA_AttributeValue::Disabled:
      pNode->SetFlag(XFA_NodeFlag_UserInteractive);
      return 1;
    default:
      return 1;
  }
}

bool CXFA_FFField::CommitData() {
  return false;
}

bool CXFA_FFField::IsDataChanged() {
  return false;
}

void CXFA_FFField::SendMessageToFWLWidget(
    std::unique_ptr<CFWL_Message> pMessage) {
  GetApp()->GetFWLWidgetMgr()->OnProcessMessageToForm(std::move(pMessage));
}

void CXFA_FFField::OnProcessMessage(CFWL_Message* pMessage) {}

void CXFA_FFField::OnProcessEvent(CFWL_Event* pEvent) {
  // Prevents destruction of the CXFA_ContentLayoutItem that owns |this|.
  RetainPtr<CXFA_ContentLayoutItem> retainer(m_pLayoutItem.Get());

  switch (pEvent->GetType()) {
    case CFWL_Event::Type::Mouse: {
      CFWL_EventMouse* event = static_cast<CFWL_EventMouse*>(pEvent);
      if (event->m_dwCmd == FWL_MouseCommand::Enter) {
        CXFA_EventParam eParam;
        eParam.m_eType = XFA_EVENT_MouseEnter;
        eParam.m_pTarget = m_pNode.Get();
        m_pNode->ProcessEvent(GetDocView(), XFA_AttributeValue::MouseEnter,
                              &eParam);
      } else if (event->m_dwCmd == FWL_MouseCommand::Leave) {
        CXFA_EventParam eParam;
        eParam.m_eType = XFA_EVENT_MouseExit;
        eParam.m_pTarget = m_pNode.Get();
        m_pNode->ProcessEvent(GetDocView(), XFA_AttributeValue::MouseExit,
                              &eParam);
      } else if (event->m_dwCmd == FWL_MouseCommand::LeftButtonDown) {
        CXFA_EventParam eParam;
        eParam.m_eType = XFA_EVENT_MouseDown;
        eParam.m_pTarget = m_pNode.Get();
        m_pNode->ProcessEvent(GetDocView(), XFA_AttributeValue::MouseDown,
                              &eParam);
      } else if (event->m_dwCmd == FWL_MouseCommand::LeftButtonUp) {
        CXFA_EventParam eParam;
        eParam.m_eType = XFA_EVENT_MouseUp;
        eParam.m_pTarget = m_pNode.Get();
        m_pNode->ProcessEvent(GetDocView(), XFA_AttributeValue::MouseUp,
                              &eParam);
      }
      break;
    }
    case CFWL_Event::Type::Click: {
      CXFA_EventParam eParam;
      eParam.m_eType = XFA_EVENT_Click;
      eParam.m_pTarget = m_pNode.Get();
      m_pNode->ProcessEvent(GetDocView(), XFA_AttributeValue::Click, &eParam);
      break;
    }
    default:
      break;
  }
}

void CXFA_FFField::OnDrawWidget(CXFA_Graphics* pGraphics,
                                const CFX_Matrix& matrix) {}
