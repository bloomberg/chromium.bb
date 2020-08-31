// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/theme/cfwl_edittp.h"

#include "core/fxge/render_defines.h"
#include "xfa/fwl/cfwl_edit.h"
#include "xfa/fwl/cfwl_themebackground.h"
#include "xfa/fwl/cfwl_widget.h"
#include "xfa/fxgraphics/cxfa_gecolor.h"
#include "xfa/fxgraphics/cxfa_gepath.h"

CFWL_EditTP::CFWL_EditTP() = default;

CFWL_EditTP::~CFWL_EditTP() = default;

void CFWL_EditTP::DrawBackground(const CFWL_ThemeBackground& pParams) {
  if (CFWL_Part::CombTextLine == pParams.m_iPart) {
    CFWL_Widget::AdapterIface* pWidget =
        pParams.m_pWidget->GetOutmost()->GetAdapterIface();
    FX_ARGB cr = 0xFF000000;
    float fWidth = 1.0f;
    pWidget->GetBorderColorAndThickness(&cr, &fWidth);
    pParams.m_pGraphics->SetStrokeColor(CXFA_GEColor(cr));
    pParams.m_pGraphics->SetLineWidth(fWidth);
    pParams.m_pGraphics->StrokePath(pParams.m_pPath.Get(), &pParams.m_matrix);
    return;
  }

  switch (pParams.m_iPart) {
    case CFWL_Part::Border: {
      DrawBorder(pParams.m_pGraphics.Get(), pParams.m_PartRect,
                 pParams.m_matrix);
      break;
    }
    case CFWL_Part::Background: {
      if (pParams.m_pPath) {
        CXFA_Graphics* pGraphics = pParams.m_pGraphics.Get();
        pGraphics->SaveGraphState();
        pGraphics->SetFillColor(CXFA_GEColor(FWLTHEME_COLOR_BKSelected));
        pGraphics->FillPath(pParams.m_pPath.Get(), FXFILL_WINDING,
                            &pParams.m_matrix);
        pGraphics->RestoreGraphState();
      } else {
        CXFA_GEPath path;
        path.AddRectangle(pParams.m_PartRect.left, pParams.m_PartRect.top,
                          pParams.m_PartRect.width, pParams.m_PartRect.height);
        CXFA_GEColor cr(FWLTHEME_COLOR_Background);
        if (!pParams.m_bStaticBackground) {
          if (pParams.m_dwStates & CFWL_PartState_Disabled)
            cr = CXFA_GEColor(FWLTHEME_COLOR_EDGERB1);
          else if (pParams.m_dwStates & CFWL_PartState_ReadOnly)
            cr = CXFA_GEColor(ArgbEncode(255, 236, 233, 216));
          else
            cr = CXFA_GEColor(0xFFFFFFFF);
        }
        pParams.m_pGraphics->SaveGraphState();
        pParams.m_pGraphics->SetFillColor(cr);
        pParams.m_pGraphics->FillPath(&path, FXFILL_WINDING, &pParams.m_matrix);
        pParams.m_pGraphics->RestoreGraphState();
      }
      break;
    }
    case CFWL_Part::CombTextLine: {
      pParams.m_pGraphics->SetStrokeColor(CXFA_GEColor(0xFF000000));
      pParams.m_pGraphics->SetLineWidth(1.0f);
      pParams.m_pGraphics->StrokePath(pParams.m_pPath.Get(), &pParams.m_matrix);
      break;
    }
    default:
      break;
  }
}
