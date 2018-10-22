// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_digestmethods.h"

#include "xfa/fxfa/parser/cxfa_digestmethods.h"

CJX_DigestMethods::CJX_DigestMethods(CXFA_DigestMethods* node)
    : CJX_Node(node) {}

CJX_DigestMethods::~CJX_DigestMethods() = default;

void CJX_DigestMethods::use(CFXJSE_Value* pValue,
                            bool bSetting,
                            XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CJX_DigestMethods::type(CFXJSE_Value* pValue,
                             bool bSetting,
                             XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CJX_DigestMethods::usehref(CFXJSE_Value* pValue,
                                bool bSetting,
                                XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}
