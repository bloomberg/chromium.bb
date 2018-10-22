// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_sourceset.h"

#include "xfa/fxfa/parser/cxfa_sourceset.h"

CJX_SourceSet::CJX_SourceSet(CXFA_SourceSet* node) : CJX_Model(node) {}

CJX_SourceSet::~CJX_SourceSet() = default;

void CJX_SourceSet::use(CFXJSE_Value* pValue,
                        bool bSetting,
                        XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}

void CJX_SourceSet::usehref(CFXJSE_Value* pValue,
                            bool bSetting,
                            XFA_Attribute eAttribute) {
  Script_Attribute_String(pValue, bSetting, eAttribute);
}
