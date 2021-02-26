// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_iconfit.h"

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fxcrt/fx_string.h"

namespace {

constexpr float kDefaultPosition = 0.5f;

}  // namespace

CPDF_IconFit::CPDF_IconFit(const CPDF_Dictionary* pDict) : m_pDict(pDict) {}

CPDF_IconFit::CPDF_IconFit(const CPDF_IconFit& that) = default;

CPDF_IconFit::~CPDF_IconFit() = default;

CPDF_IconFit::ScaleMethod CPDF_IconFit::GetScaleMethod() const {
  if (!m_pDict)
    return ScaleMethod::kAlways;

  ByteString csSW = m_pDict->GetStringFor("SW", "A");
  if (csSW == "B")
    return ScaleMethod::kBigger;
  if (csSW == "S")
    return ScaleMethod::kSmaller;
  if (csSW == "N")
    return ScaleMethod::kNever;
  return ScaleMethod::kAlways;
}

bool CPDF_IconFit::IsProportionalScale() const {
  return !m_pDict || m_pDict->GetStringFor("S", "P") != "A";
}

CFX_PointF CPDF_IconFit::GetIconBottomLeftPosition() const {
  float fLeft = kDefaultPosition;
  float fBottom = kDefaultPosition;
  if (!m_pDict)
    return {fLeft, fBottom};

  const CPDF_Array* pA = m_pDict->GetArrayFor("A");
  if (!pA)
    return {fLeft, fBottom};

  size_t dwCount = pA->size();
  if (dwCount > 0)
    fLeft = pA->GetNumberAt(0);
  if (dwCount > 1)
    fBottom = pA->GetNumberAt(1);
  return {fLeft, fBottom};
}

bool CPDF_IconFit::GetFittingBounds() const {
  return m_pDict && m_pDict->GetBooleanFor("FB", false);
}

CFX_PointF CPDF_IconFit::GetIconPosition() const {
  if (!m_pDict)
    return CFX_PointF();

  const CPDF_Array* pA = m_pDict->GetArrayFor("A");
  if (!pA)
    return CFX_PointF();

  size_t dwCount = pA->size();
  return {dwCount > 0 ? pA->GetNumberAt(0) : 0.0f,
          dwCount > 1 ? pA->GetNumberAt(1) : 0.0f};
}
