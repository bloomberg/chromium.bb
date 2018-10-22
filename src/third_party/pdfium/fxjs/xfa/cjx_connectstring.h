// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_CONNECTSTRING_H_
#define FXJS_XFA_CJX_CONNECTSTRING_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_textnode.h"

class CXFA_ConnectString;

class CJX_ConnectString final : public CJX_TextNode {
 public:
  explicit CJX_ConnectString(CXFA_ConnectString* node);
  ~CJX_ConnectString() override;

  JSE_PROP(use);
  JSE_PROP(usehref);
};

#endif  // FXJS_XFA_CJX_CONNECTSTRING_H_
