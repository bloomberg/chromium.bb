// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_REASONS_H_
#define FXJS_XFA_CJX_REASONS_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_node.h"

class CXFA_Reasons;

class CJX_Reasons final : public CJX_Node {
 public:
  explicit CJX_Reasons(CXFA_Reasons* node);
  ~CJX_Reasons() override;

  JSE_PROP(type);
  JSE_PROP(use);
  JSE_PROP(usehref);
};

#endif  // FXJS_XFA_CJX_REASONS_H_
