// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_SHADINGOBJECT_H_
#define CORE_FPDFAPI_PAGE_CPDF_SHADINGOBJECT_H_

#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/retain_ptr.h"

class CPDF_ShadingPattern;

class CPDF_ShadingObject final : public CPDF_PageObject {
 public:
  CPDF_ShadingObject(int32_t content_stream,
                     CPDF_ShadingPattern* pattern,
                     const CFX_Matrix& matrix);
  ~CPDF_ShadingObject() override;

  // CPDF_PageObject:
  Type GetType() const override;
  void Transform(const CFX_Matrix& matrix) override;
  bool IsShading() const override;
  CPDF_ShadingObject* AsShading() override;
  const CPDF_ShadingObject* AsShading() const override;

  void CalcBoundingBox();

  const CPDF_ShadingPattern* pattern() const { return m_pShading.Get(); }
  const CFX_Matrix& matrix() const { return m_Matrix; }

 private:
  RetainPtr<CPDF_ShadingPattern> m_pShading;
  CFX_Matrix m_Matrix;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_SHADINGOBJECT_H_
