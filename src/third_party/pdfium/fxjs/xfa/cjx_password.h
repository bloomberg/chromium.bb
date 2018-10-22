// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_PASSWORD_H_
#define FXJS_XFA_CJX_PASSWORD_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_textnode.h"

class CXFA_Password;

class CJX_Password final : public CJX_TextNode {
 public:
  explicit CJX_Password(CXFA_Password* node);
  ~CJX_Password() override;

  JSE_PROP(use);
  JSE_PROP(usehref);
};

#endif  // FXJS_XFA_CJX_PASSWORD_H_
