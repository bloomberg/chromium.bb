// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_reason.h"

#include "xfa/fxfa/parser/cxfa_reason.h"

CJX_Reason::CJX_Reason(CXFA_Reason* node) : CJX_TextNode(node) {}

CJX_Reason::~CJX_Reason() = default;

void CJX_Reason::use(CFXJSE_Value* pValue,
                     bool bSetting,
                     XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CJX_Reason::usehref(CFXJSE_Value* pValue,
                         bool bSetting,
                         XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}
