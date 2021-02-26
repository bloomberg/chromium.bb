// Copyright 2020 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_PWL_CPWL_CBBUTTON_H_
#define FPDFSDK_PWL_CPWL_CBBUTTON_H_

#include <memory>

#include "fpdfsdk/pwl/cpwl_wnd.h"
#include "fpdfsdk/pwl/ipwl_systemhandler.h"

class CPWL_CBButton final : public CPWL_Wnd {
 public:
  CPWL_CBButton(
      const CreateParams& cp,
      std::unique_ptr<IPWL_SystemHandler::PerWindowData> pAttachedData);
  ~CPWL_CBButton() override;

  // CPWL_Wnd:
  void DrawThisAppearance(CFX_RenderDevice* pDevice,
                          const CFX_Matrix& mtUser2Device) override;
  bool OnLButtonDown(uint32_t nFlag, const CFX_PointF& point) override;
  bool OnLButtonUp(uint32_t nFlag, const CFX_PointF& point) override;
};

#endif  // FPDFSDK_PWL_CPWL_CBBUTTON_H_
