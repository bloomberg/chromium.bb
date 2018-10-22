// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_datetimeedit.h"

#include "xfa/fxfa/parser/cxfa_datetimeedit.h"

CJX_DateTimeEdit::CJX_DateTimeEdit(CXFA_DateTimeEdit* node) : CJX_Node(node) {}

CJX_DateTimeEdit::~CJX_DateTimeEdit() = default;

void CJX_DateTimeEdit::use(CFXJSE_Value* pValue,
                           bool bSetting,
                           XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CJX_DateTimeEdit::usehref(CFXJSE_Value* pValue,
                               bool bSetting,
                               XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CJX_DateTimeEdit::hScrollPolicy(CFXJSE_Value* pValue,
                                     bool bSetting,
                                     XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}
