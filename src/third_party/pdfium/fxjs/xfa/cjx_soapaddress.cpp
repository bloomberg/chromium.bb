// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_soapaddress.h"

#include "xfa/fxfa/parser/cxfa_soapaddress.h"

CJX_SoapAddress::CJX_SoapAddress(CXFA_SoapAddress* node) : CJX_TextNode(node) {}

CJX_SoapAddress::~CJX_SoapAddress() = default;

void CJX_SoapAddress::use(CFXJSE_Value* pValue,
                          bool bSetting,
                          XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CJX_SoapAddress::usehref(CFXJSE_Value* pValue,
                              bool bSetting,
                              XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}
