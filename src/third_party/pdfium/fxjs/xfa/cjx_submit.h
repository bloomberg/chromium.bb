// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_SUBMIT_H_
#define FXJS_XFA_CJX_SUBMIT_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_node.h"

class CXFA_Submit;

class CJX_Submit final : public CJX_Node {
 public:
  explicit CJX_Submit(CXFA_Submit* node);
  ~CJX_Submit() override;

  JSE_PROP(embedPDF);
  JSE_PROP(format);
  JSE_PROP(target);
  JSE_PROP(textEncoding);
  JSE_PROP(use);
  JSE_PROP(usehref);
  JSE_PROP(xdpContent);
};

#endif  // FXJS_XFA_CJX_SUBMIT_H_
