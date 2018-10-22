// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_encoding.h"

#include "xfa/fxfa/parser/cxfa_encoding.h"

CJX_Encoding::CJX_Encoding(CXFA_Encoding* node) : CJX_Node(node) {}

CJX_Encoding::~CJX_Encoding() = default;

void CJX_Encoding::use(CFXJSE_Value* pValue,
                       bool bSetting,
                       XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CJX_Encoding::usehref(CFXJSE_Value* pValue,
                           bool bSetting,
                           XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}
