// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_script.h"

#include "fxjs/xfa/cfxjse_value.h"
#include "xfa/fxfa/parser/cxfa_script.h"

CJX_Script::CJX_Script(CXFA_Script* node) : CJX_Node(node) {}

CJX_Script::~CJX_Script() = default;

bool CJX_Script::DynamicTypeIs(TypeTag eType) const {
  return eType == static_type__ || ParentType__::DynamicTypeIs(eType);
}

void CJX_Script::stateless(CFXJSE_Value* pValue,
                           bool bSetting,
                           XFA_Attribute eAttribute) {
  if (bSetting) {
    ThrowInvalidPropertyException();
    return;
  }
  pValue->SetString("0");
}
