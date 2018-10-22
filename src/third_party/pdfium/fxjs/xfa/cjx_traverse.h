// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_TRAVERSE_H_
#define FXJS_XFA_CJX_TRAVERSE_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_node.h"

class CXFA_Traverse;

class CJX_Traverse final : public CJX_Node {
 public:
  explicit CJX_Traverse(CXFA_Traverse* node);
  ~CJX_Traverse() override;

  JSE_PROP(operation);
  JSE_PROP(ref);
  JSE_PROP(use);
  JSE_PROP(usehref);
};

#endif  // FXJS_XFA_CJX_TRAVERSE_H_
