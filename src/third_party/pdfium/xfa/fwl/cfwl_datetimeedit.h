// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CFWL_DATETIMEEDIT_H_
#define XFA_FWL_CFWL_DATETIMEEDIT_H_

#include "xfa/fwl/cfwl_edit.h"
#include "xfa/fwl/cfwl_widget.h"

class CFWL_DateTimeEdit final : public CFWL_Edit {
 public:
  CONSTRUCT_VIA_MAKE_GARBAGE_COLLECTED;
  ~CFWL_DateTimeEdit() override;

  // CFWL_Edit:
  void OnProcessMessage(CFWL_Message* pMessage) override;

 private:
  CFWL_DateTimeEdit(CFWL_App* app,
                    const Properties& properties,
                    CFWL_Widget* pOuter);
};

#endif  // XFA_FWL_CFWL_DATETIMEEDIT_H_
