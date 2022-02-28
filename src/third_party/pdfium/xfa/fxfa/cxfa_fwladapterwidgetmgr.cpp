// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/cxfa_fwladapterwidgetmgr.h"

#include "core/fxcrt/fx_coordinates.h"
#include "xfa/fxfa/cxfa_ffdoc.h"
#include "xfa/fxfa/cxfa_fffield.h"

CXFA_FWLAdapterWidgetMgr::CXFA_FWLAdapterWidgetMgr() = default;

CXFA_FWLAdapterWidgetMgr::~CXFA_FWLAdapterWidgetMgr() = default;

void CXFA_FWLAdapterWidgetMgr::Trace(cppgc::Visitor* visitor) const {}

void CXFA_FWLAdapterWidgetMgr::RepaintWidget(CFWL_Widget* pWidget) {
  if (!pWidget)
    return;

  auto* pFFWidget = static_cast<CXFA_FFWidget*>(pWidget->GetAdapterIface());
  if (!pFFWidget)
    return;

  pFFWidget->InvalidateRect();
}

bool CXFA_FWLAdapterWidgetMgr::GetPopupPos(CFWL_Widget* pWidget,
                                           float fMinHeight,
                                           float fMaxHeight,
                                           const CFX_RectF& rtAnchor,
                                           CFX_RectF* pPopupRect) {
  auto* pFFWidget = static_cast<CXFA_FFWidget*>(pWidget->GetAdapterIface());
  CFX_RectF rtRotateAnchor =
      pFFWidget->GetRotateMatrix().TransformRect(rtAnchor);
  pFFWidget->GetDoc()->GetPopupPos(pFFWidget, fMinHeight, fMaxHeight,
                                   rtRotateAnchor, pPopupRect);
  return true;
}
