// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_xfa.h"

#include "fxjs/cfxjse_engine.h"
#include "fxjs/cfxjse_value.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_xfa.h"

CJX_Xfa::CJX_Xfa(CXFA_Xfa* node) : CJX_Model(node) {}

CJX_Xfa::~CJX_Xfa() = default;

void CJX_Xfa::thisValue(CFXJSE_Value* pValue,
                        bool bSetting,
                        XFA_Attribute eAttribute) {
  if (bSetting)
    return;

  CXFA_Object* pThis = GetDocument()->GetScriptContext()->GetThisObject();
  ASSERT(pThis);
  pValue->Assign(GetDocument()->GetScriptContext()->GetJSValueFromMap(pThis));
}

void CJX_Xfa::timeStamp(CFXJSE_Value* pValue,
                        bool bSetting,
                        XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CJX_Xfa::uuid(CFXJSE_Value* pValue,
                   bool bSetting,
                   XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}
