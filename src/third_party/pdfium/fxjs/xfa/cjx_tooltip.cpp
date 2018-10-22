// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_tooltip.h"

#include "xfa/fxfa/parser/cxfa_tooltip.h"

CJX_ToolTip::CJX_ToolTip(CXFA_ToolTip* node) : CJX_TextNode(node) {}

CJX_ToolTip::~CJX_ToolTip() = default;

void CJX_ToolTip::use(CFXJSE_Value* pValue,
                      bool bSetting,
                      XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CJX_ToolTip::usehref(CFXJSE_Value* pValue,
                          bool bSetting,
                          XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}
