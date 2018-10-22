// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_LINEAR_H_
#define FXJS_XFA_CJX_LINEAR_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_node.h"

class CXFA_Linear;

class CJX_Linear final : public CJX_Node {
 public:
  explicit CJX_Linear(CXFA_Linear* node);
  ~CJX_Linear() override;

  JSE_PROP(type);
  JSE_PROP(use);
  JSE_PROP(usehref);
};

#endif  // FXJS_XFA_CJX_LINEAR_H_
