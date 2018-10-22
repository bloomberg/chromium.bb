// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_XSDCONNECTION_H_
#define FXJS_XFA_CJX_XSDCONNECTION_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_node.h"

class CXFA_XsdConnection;

class CJX_XsdConnection final : public CJX_Node {
 public:
  explicit CJX_XsdConnection(CXFA_XsdConnection* node);
  ~CJX_XsdConnection() override;

  JSE_PROP(dataDescription);
};

#endif  // FXJS_XFA_CJX_XSDCONNECTION_H_
