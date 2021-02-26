// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/theme/cfwl_listboxtp.h"

#include "build/build_config.h"
#include "xfa/fgas/graphics/cfgas_gecolor.h"
#include "xfa/fgas/graphics/cfgas_gepath.h"
#include "xfa/fwl/cfwl_listbox.h"
#include "xfa/fwl/cfwl_themebackground.h"
#include "xfa/fwl/cfwl_widget.h"

CFWL_ListBoxTP::CFWL_ListBoxTP() = default;

CFWL_ListBoxTP::~CFWL_ListBoxTP() = default;

void CFWL_ListBoxTP::DrawBackground(const CFWL_ThemeBackground& pParams) {
  switch (pParams.m_iPart) {
    case CFWL_Part::Border: {
      DrawBorder(pParams.m_pGraphics.Get(), pParams.m_PartRect,
                 pParams.m_matrix);
      break;
    }
    case CFWL_Part::Background: {
      FillSolidRect(pParams.m_pGraphics.Get(), ArgbEncode(255, 255, 255, 255),
                    pParams.m_PartRect, pParams.m_matrix);
      if (pParams.m_pRtData) {
        FillSolidRect(pParams.m_pGraphics.Get(), FWLTHEME_COLOR_Background,
                      *pParams.m_pRtData, pParams.m_matrix);
      }
      break;
    }
    case CFWL_Part::ListItem: {
      DrawListBoxItem(pParams.m_pGraphics.Get(), pParams.m_dwStates,
                      pParams.m_PartRect, pParams.m_pRtData, pParams.m_matrix);
      break;
    }
    case CFWL_Part::Check: {
      uint32_t color = 0xFF000000;
      if (pParams.m_dwStates == CFWL_PartState_Checked) {
        color = 0xFFFF0000;
      } else if (pParams.m_dwStates == CFWL_PartState_Normal) {
        color = 0xFF0000FF;
      }
      FillSolidRect(pParams.m_pGraphics.Get(), color, pParams.m_PartRect,
                    pParams.m_matrix);
      break;
    }
    default:
      break;
  }
}

void CFWL_ListBoxTP::DrawListBoxItem(CFGAS_GEGraphics* pGraphics,
                                     uint32_t dwStates,
                                     const CFX_RectF& rtItem,
                                     const CFX_RectF* pData,
                                     const CFX_Matrix& matrix) {
  if (dwStates & CFWL_PartState_Selected) {
    pGraphics->SaveGraphState();
    pGraphics->SetFillColor(CFGAS_GEColor(FWLTHEME_COLOR_BKSelected));
    CFGAS_GEPath path;
#if defined(OS_APPLE)
    path.AddRectangle(rtItem.left, rtItem.top, rtItem.width - 1,
                      rtItem.height - 1);
#else
    path.AddRectangle(rtItem.left, rtItem.top, rtItem.width, rtItem.height);
#endif
    pGraphics->FillPath(&path, CFX_FillRenderOptions::FillType::kWinding,
                        &matrix);
    pGraphics->RestoreGraphState();
  }
  if ((dwStates & CFWL_PartState_Focused) && pData)
    DrawFocus(pGraphics, *pData, matrix);
}
