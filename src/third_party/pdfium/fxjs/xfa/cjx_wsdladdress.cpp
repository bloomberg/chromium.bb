// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_wsdladdress.h"

#include "xfa/fxfa/parser/cxfa_wsdladdress.h"

CJX_WsdlAddress::CJX_WsdlAddress(CXFA_WsdlAddress* node) : CJX_TextNode(node) {}

CJX_WsdlAddress::~CJX_WsdlAddress() = default;

void CJX_WsdlAddress::use(CFXJSE_Value* pValue,
                          bool bSetting,
                          XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CJX_WsdlAddress::usehref(CFXJSE_Value* pValue,
                              bool bSetting,
                              XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}
