// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CFWL_COMBOEDIT_H_
#define XFA_FWL_CFWL_COMBOEDIT_H_

#include "xfa/fwl/cfwl_edit.h"
#include "xfa/fwl/cfwl_widget.h"

class CFWL_ComboEdit final : public CFWL_Edit {
 public:
  CONSTRUCT_VIA_MAKE_GARBAGE_COLLECTED;
  ~CFWL_ComboEdit() override;

  // CFWL_Edit:
  void OnProcessMessage(CFWL_Message* pMessage) override;

  void ClearSelected();
  void SetSelected();

 private:
  CFWL_ComboEdit(CFWL_App* app,
                 const Properties& properties,
                 CFWL_Widget* pOuter);
};

#endif  // XFA_FWL_CFWL_COMBOEDIT_H_
