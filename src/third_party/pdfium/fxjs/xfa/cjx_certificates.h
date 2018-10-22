// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_CERTIFICATES_H_
#define FXJS_XFA_CJX_CERTIFICATES_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_node.h"

class CXFA_Certificates;

class CJX_Certificates final : public CJX_Node {
 public:
  explicit CJX_Certificates(CXFA_Certificates* node);
  ~CJX_Certificates() override;

  JSE_PROP(credentialServerPolicy);
  JSE_PROP(url);
  JSE_PROP(urlPolicy);
  JSE_PROP(use);
  JSE_PROP(usehref);
};

#endif  // FXJS_XFA_CJX_CERTIFICATES_H_
