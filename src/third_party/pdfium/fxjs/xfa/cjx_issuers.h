// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_ISSUERS_H_
#define FXJS_XFA_CJX_ISSUERS_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_node.h"

class CXFA_Issuers;

class CJX_Issuers final : public CJX_Node {
 public:
  explicit CJX_Issuers(CXFA_Issuers* node);
  ~CJX_Issuers() override;

  JSE_PROP(type);
  JSE_PROP(use);
  JSE_PROP(usehref);
};

#endif  // FXJS_XFA_CJX_ISSUERS_H_
