// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_TEXTEDIT_H_
#define FXJS_XFA_CJX_TEXTEDIT_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_node.h"

class CXFA_TextEdit;

class CJX_TextEdit final : public CJX_Node {
 public:
  explicit CJX_TextEdit(CXFA_TextEdit* node);
  ~CJX_TextEdit() override;

  JSE_PROP(allowRichText);
  JSE_PROP(hScrollPolicy);
  JSE_PROP(multiLine);
  JSE_PROP(use);
  JSE_PROP(usehref);
  JSE_PROP(vScrollPolicy);
};

#endif  // FXJS_XFA_CJX_TEXTEDIT_H_
