// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_SUBJECTDNS_H_
#define FXJS_XFA_CJX_SUBJECTDNS_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_node.h"

class CXFA_SubjectDNs;

class CJX_SubjectDNs final : public CJX_Node {
 public:
  explicit CJX_SubjectDNs(CXFA_SubjectDNs* node);
  ~CJX_SubjectDNs() override;

  JSE_PROP(type);
};

#endif  // FXJS_XFA_CJX_SUBJECTDNS_H_
