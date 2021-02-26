// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/cxfa_fwltheme.h"

#include "core/fxcrt/fx_codepage.h"
#include "third_party/base/stl_util.h"
#include "xfa/fde/cfde_textout.h"
#include "xfa/fgas/font/cfgas_fontmgr.h"
#include "xfa/fgas/font/cfgas_gefont.h"
#include "xfa/fgas/font/cfgas_gemodule.h"
#include "xfa/fgas/graphics/cfgas_gecolor.h"
#include "xfa/fwl/cfwl_barcode.h"
#include "xfa/fwl/cfwl_caret.h"
#include "xfa/fwl/cfwl_checkbox.h"
#include "xfa/fwl/cfwl_combobox.h"
#include "xfa/fwl/cfwl_datetimepicker.h"
#include "xfa/fwl/cfwl_edit.h"
#include "xfa/fwl/cfwl_listbox.h"
#include "xfa/fwl/cfwl_monthcalendar.h"
#include "xfa/fwl/cfwl_picturebox.h"
#include "xfa/fwl/cfwl_pushbutton.h"
#include "xfa/fwl/cfwl_scrollbar.h"
#include "xfa/fwl/cfwl_themebackground.h"
#include "xfa/fwl/cfwl_themetext.h"
#include "xfa/fwl/theme/cfwl_fontmanager.h"
#include "xfa/fwl/theme/cfwl_widgettp.h"
#include "xfa/fxfa/cxfa_ffapp.h"
#include "xfa/fxfa/cxfa_ffwidget.h"
#include "xfa/fxfa/cxfa_fontmgr.h"
#include "xfa/fxfa/parser/cxfa_para.h"

namespace {

constexpr const wchar_t* kFWLThemeCalFonts[] = {
    L"Arial",
    L"Courier New",
    L"DejaVu Sans",
};

const float kLineHeight = 12.0f;

CXFA_FFWidget* GetOutmostFFWidget(CFWL_Widget* pWidget) {
  CFWL_Widget* pOuter = pWidget ? pWidget->GetOutmost() : nullptr;
  return pOuter ? static_cast<CXFA_FFWidget*>(pOuter->GetAdapterIface())
                : nullptr;
}

}  // namespace

CXFA_FWLTheme::CXFA_FWLTheme(cppgc::Heap* pHeap, CXFA_FFApp* pApp)
    : IFWL_ThemeProvider(pHeap),
      m_pTextOut(std::make_unique<CFDE_TextOut>()),
      m_pApp(pApp) {}

CXFA_FWLTheme::~CXFA_FWLTheme() = default;

void CXFA_FWLTheme::PreFinalize() {
  m_pTextOut.reset();
  CFWL_FontManager::DestroyInstance();
}

void CXFA_FWLTheme::Trace(cppgc::Visitor* visitor) const {
  IFWL_ThemeProvider::Trace(visitor);
  visitor->Trace(m_pApp);
}

bool CXFA_FWLTheme::LoadCalendarFont(CXFA_FFDoc* doc) {
  if (m_pCalendarFont)
    return true;

  for (const wchar_t* font : kFWLThemeCalFonts) {
    m_pCalendarFont = m_pApp->GetXFAFontMgr()->GetFont(doc, font, 0);
    if (m_pCalendarFont)
      return true;
  }

  m_pCalendarFont = CFGAS_GEModule::Get()->GetFontMgr()->GetFontByCodePage(
      FX_CODEPAGE_MSWin_WesternEuropean, 0, nullptr);
  return !!m_pCalendarFont;
}

void CXFA_FWLTheme::DrawBackground(const CFWL_ThemeBackground& pParams) {
  GetTheme(pParams.m_pWidget)->DrawBackground(pParams);
}

void CXFA_FWLTheme::DrawText(const CFWL_ThemeText& pParams) {
  if (pParams.m_wsText.IsEmpty())
    return;

  if (pParams.m_pWidget->GetClassID() == FWL_Type::MonthCalendar) {
    CXFA_FFWidget* pWidget = GetOutmostFFWidget(pParams.m_pWidget);
    if (!pWidget)
      return;

    m_pTextOut->SetStyles(pParams.m_dwTTOStyles);
    m_pTextOut->SetAlignment(pParams.m_iTTOAlign);
    m_pTextOut->SetFont(m_pCalendarFont);
    m_pTextOut->SetFontSize(FWLTHEME_CAPACITY_FontSize);
    m_pTextOut->SetTextColor(FWLTHEME_CAPACITY_TextColor);
    if ((pParams.m_iPart == CFWL_Part::DatesIn) &&
        !(pParams.m_dwStates & FWL_ITEMSTATE_MCD_Flag) &&
        (pParams.m_dwStates &
         (CFWL_PartState_Hovered | CFWL_PartState_Selected))) {
      m_pTextOut->SetTextColor(0xFF888888);
    }
    if (pParams.m_iPart == CFWL_Part::Caption)
      m_pTextOut->SetTextColor(ArgbEncode(0xff, 0, 153, 255));

    CFGAS_GEGraphics* pGraphics = pParams.m_pGraphics;
    CFX_RenderDevice* pRenderDevice = pGraphics->GetRenderDevice();
    CFX_Matrix mtPart = pParams.m_matrix;
    const CFX_Matrix* pMatrix = pGraphics->GetMatrix();
    if (pMatrix)
      mtPart.Concat(*pMatrix);

    m_pTextOut->SetMatrix(mtPart);
    m_pTextOut->DrawLogicText(pRenderDevice, pParams.m_wsText.AsStringView(),
                              pParams.m_PartRect);
    return;
  }
  CXFA_FFWidget* pWidget = GetOutmostFFWidget(pParams.m_pWidget);
  if (!pWidget)
    return;

  CXFA_Node* pNode = pWidget->GetNode();
  CFGAS_GEGraphics* pGraphics = pParams.m_pGraphics;
  CFX_RenderDevice* pRenderDevice = pGraphics->GetRenderDevice();
  m_pTextOut->SetStyles(pParams.m_dwTTOStyles);
  m_pTextOut->SetAlignment(pParams.m_iTTOAlign);
  m_pTextOut->SetFont(pNode->GetFGASFont(pWidget->GetDoc()));
  m_pTextOut->SetFontSize(pNode->GetFontSize());
  m_pTextOut->SetTextColor(pNode->GetTextColor());
  CFX_Matrix mtPart = pParams.m_matrix;
  const CFX_Matrix* pMatrix = pGraphics->GetMatrix();
  if (pMatrix)
    mtPart.Concat(*pMatrix);

  m_pTextOut->SetMatrix(mtPart);
  m_pTextOut->DrawLogicText(pRenderDevice, pParams.m_wsText.AsStringView(),
                            pParams.m_PartRect);
}

CFX_RectF CXFA_FWLTheme::GetUIMargin(const CFWL_ThemePart& pThemePart) const {
  CXFA_FFWidget* pWidget = GetOutmostFFWidget(pThemePart.m_pWidget);
  if (!pWidget)
    return CFX_RectF();

  CXFA_ContentLayoutItem* pItem = pWidget->GetLayoutItem();
  CXFA_Node* pNode = pWidget->GetNode();
  CFX_RectF rect = pNode->GetUIMargin();
  CXFA_Para* para = pNode->GetParaIfExists();
  if (para) {
    rect.left += para->GetMarginLeft();
    if (pNode->IsMultiLine())
      rect.width += para->GetMarginRight();
  }
  if (!pItem->GetPrev()) {
    if (pItem->GetNext())
      rect.height = 0;
  } else if (!pItem->GetNext()) {
    rect.top = 0;
  } else {
    rect.top = 0;
    rect.height = 0;
  }
  return rect;
}

float CXFA_FWLTheme::GetCXBorderSize() const {
  return 1.0f;
}

float CXFA_FWLTheme::GetCYBorderSize() const {
  return 1.0f;
}

float CXFA_FWLTheme::GetFontSize(const CFWL_ThemePart& pThemePart) const {
  if (CXFA_FFWidget* pWidget = GetOutmostFFWidget(pThemePart.m_pWidget))
    return pWidget->GetNode()->GetFontSize();
  return FWLTHEME_CAPACITY_FontSize;
}

RetainPtr<CFGAS_GEFont> CXFA_FWLTheme::GetFont(
    const CFWL_ThemePart& pThemePart) const {
  if (CXFA_FFWidget* pWidget = GetOutmostFFWidget(pThemePart.m_pWidget))
    return pWidget->GetNode()->GetFGASFont(pWidget->GetDoc());
  return GetTheme(pThemePart.m_pWidget)->GetFont();
}

float CXFA_FWLTheme::GetLineHeight(const CFWL_ThemePart& pThemePart) const {
  if (CXFA_FFWidget* pWidget = GetOutmostFFWidget(pThemePart.m_pWidget))
    return pWidget->GetNode()->GetLineHeight();
  return kLineHeight;
}

float CXFA_FWLTheme::GetScrollBarWidth() const {
  return 9.0f;
}

FX_COLORREF CXFA_FWLTheme::GetTextColor(
    const CFWL_ThemePart& pThemePart) const {
  if (CXFA_FFWidget* pWidget = GetOutmostFFWidget(pThemePart.m_pWidget))
    return pWidget->GetNode()->GetTextColor();
  return FWLTHEME_CAPACITY_TextColor;
}

CFX_SizeF CXFA_FWLTheme::GetSpaceAboveBelow(
    const CFWL_ThemePart& pThemePart) const {
  CFX_SizeF sizeAboveBelow;
  if (CXFA_FFWidget* pWidget = GetOutmostFFWidget(pThemePart.m_pWidget)) {
    CXFA_Para* para = pWidget->GetNode()->GetParaIfExists();
    if (para) {
      sizeAboveBelow.width = para->GetSpaceAbove();
      sizeAboveBelow.height = para->GetSpaceBelow();
    }
  }
  return sizeAboveBelow;
}

void CXFA_FWLTheme::CalcTextRect(const CFWL_ThemeText& pParams,
                                 CFX_RectF* pRect) {
  if (!m_pTextOut)
    return;

  CXFA_FFWidget* pWidget = GetOutmostFFWidget(pParams.m_pWidget);
  if (!pWidget)
    return;

  if (pParams.m_pWidget->GetClassID() == FWL_Type::MonthCalendar) {
    m_pTextOut->SetFont(m_pCalendarFont);
    m_pTextOut->SetFontSize(FWLTHEME_CAPACITY_FontSize);
    m_pTextOut->SetTextColor(FWLTHEME_CAPACITY_TextColor);
    m_pTextOut->SetAlignment(pParams.m_iTTOAlign);
    m_pTextOut->SetStyles(pParams.m_dwTTOStyles);
    m_pTextOut->CalcLogicSize(pParams.m_wsText.AsStringView(), pRect);
    return;
  }

  CXFA_Node* pNode = pWidget->GetNode();
  m_pTextOut->SetFont(pNode->GetFGASFont(pWidget->GetDoc()));
  m_pTextOut->SetFontSize(pNode->GetFontSize());
  m_pTextOut->SetTextColor(pNode->GetTextColor());
  m_pTextOut->SetAlignment(pParams.m_iTTOAlign);
  m_pTextOut->SetStyles(pParams.m_dwTTOStyles);
  m_pTextOut->CalcLogicSize(pParams.m_wsText.AsStringView(), pRect);
}
