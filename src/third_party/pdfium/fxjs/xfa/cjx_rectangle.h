// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_RECTANGLE_H_
#define FXJS_XFA_CJX_RECTANGLE_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_node.h"

class CXFA_Rectangle;

class CJX_Rectangle final : public CJX_Node {
 public:
  explicit CJX_Rectangle(CXFA_Rectangle* node);
  ~CJX_Rectangle() override;

  JSE_PROP(hand);
  JSE_PROP(use);
  JSE_PROP(usehref);
};

#endif  // FXJS_XFA_CJX_RECTANGLE_H_
