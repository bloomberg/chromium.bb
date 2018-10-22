// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_COLOR_H_
#define FXJS_XFA_CJX_COLOR_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_node.h"

class CXFA_Color;

class CJX_Color final : public CJX_Node {
 public:
  explicit CJX_Color(CXFA_Color* node);
  ~CJX_Color() override;

  JSE_PROP(cSpace);
  JSE_PROP(use);
  JSE_PROP(usehref);
  JSE_PROP(value);
};

#endif  // FXJS_XFA_CJX_COLOR_H_
