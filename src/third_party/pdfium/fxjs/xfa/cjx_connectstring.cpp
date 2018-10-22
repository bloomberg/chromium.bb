// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_connectstring.h"

#include "xfa/fxfa/parser/cxfa_connectstring.h"

CJX_ConnectString::CJX_ConnectString(CXFA_ConnectString* node)
    : CJX_TextNode(node) {}

CJX_ConnectString::~CJX_ConnectString() = default;

void CJX_ConnectString::use(CFXJSE_Value* pValue,
                            bool bSetting,
                            XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CJX_ConnectString::usehref(CFXJSE_Value* pValue,
                                bool bSetting,
                                XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}
