// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_EXECUTE_H_
#define FXJS_XFA_CJX_EXECUTE_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_node.h"

class CXFA_Execute;

class CJX_Execute final : public CJX_Node {
 public:
  explicit CJX_Execute(CXFA_Execute* node);
  ~CJX_Execute() override;

  JSE_PROP(connection);
  JSE_PROP(executeType);
  JSE_PROP(runAt);
  JSE_PROP(use);
  JSE_PROP(usehref);
};

#endif  // FXJS_XFA_CJX_EXECUTE_H_
