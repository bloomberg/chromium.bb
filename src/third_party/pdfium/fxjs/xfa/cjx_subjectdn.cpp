// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_subjectdn.h"

#include "xfa/fxfa/parser/cxfa_subjectdn.h"

CJX_SubjectDN::CJX_SubjectDN(CXFA_SubjectDN* node) : CJX_Node(node) {}

CJX_SubjectDN::~CJX_SubjectDN() = default;

void CJX_SubjectDN::delimiter(CFXJSE_Value* pValue,
                              bool bSetting,
                              XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}
