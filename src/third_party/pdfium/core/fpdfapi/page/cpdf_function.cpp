// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_function.h"

#include <vector>

#include "core/fpdfapi/page/cpdf_expintfunc.h"
#include "core/fpdfapi/page/cpdf_psfunc.h"
#include "core/fpdfapi/page/cpdf_sampledfunc.h"
#include "core/fpdfapi/page/cpdf_stitchfunc.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/fpdf_parser_utility.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxcrt/scoped_set_insertion.h"
#include "third_party/base/containers/contains.h"
#include "third_party/base/cxx17_backports.h"

namespace {

CPDF_Function::Type IntegerToFunctionType(int iType) {
  switch (iType) {
    case 0:
    case 2:
    case 3:
    case 4:
      return static_cast<CPDF_Function::Type>(iType);
    default:
      return CPDF_Function::Type::kTypeInvalid;
  }
}

}  // namespace

// static
std::unique_ptr<CPDF_Function> CPDF_Function::Load(
    const CPDF_Object* pFuncObj) {
  std::set<const CPDF_Object*> visited;
  return Load(pFuncObj, &visited);
}

// static
std::unique_ptr<CPDF_Function> CPDF_Function::Load(
    const CPDF_Object* pFuncObj,
    std::set<const CPDF_Object*>* pVisited) {
  if (!pFuncObj)
    return nullptr;

  if (pdfium::Contains(*pVisited, pFuncObj))
    return nullptr;
  ScopedSetInsertion<const CPDF_Object*> insertion(pVisited, pFuncObj);

  int iType = -1;
  if (const CPDF_Stream* pStream = pFuncObj->AsStream())
    iType = pStream->GetDict()->GetIntegerFor("FunctionType");
  else if (const CPDF_Dictionary* pDict = pFuncObj->AsDictionary())
    iType = pDict->GetIntegerFor("FunctionType");

  std::unique_ptr<CPDF_Function> pFunc;
  Type type = IntegerToFunctionType(iType);
  if (type == Type::kType0Sampled)
    pFunc = std::make_unique<CPDF_SampledFunc>();
  else if (type == Type::kType2ExponentialInterpolation)
    pFunc = std::make_unique<CPDF_ExpIntFunc>();
  else if (type == Type::kType3Stitching)
    pFunc = std::make_unique<CPDF_StitchFunc>();
  else if (type == Type::kType4PostScript)
    pFunc = std::make_unique<CPDF_PSFunc>();

  if (!pFunc || !pFunc->Init(pFuncObj, pVisited))
    return nullptr;

  return pFunc;
}

CPDF_Function::CPDF_Function(Type type) : m_Type(type) {}

CPDF_Function::~CPDF_Function() = default;

bool CPDF_Function::Init(const CPDF_Object* pObj,
                         std::set<const CPDF_Object*>* pVisited) {
  const CPDF_Stream* pStream = pObj->AsStream();
  const CPDF_Dictionary* pDict =
      pStream ? pStream->GetDict() : pObj->AsDictionary();

  const CPDF_Array* pDomains = pDict->GetArrayFor("Domain");
  if (!pDomains)
    return false;

  m_nInputs = pDomains->size() / 2;
  if (m_nInputs == 0)
    return false;

  size_t nInputs = m_nInputs * 2;
  m_Domains = ReadArrayElementsToVector(pDomains, nInputs);

  const CPDF_Array* pRanges = pDict->GetArrayFor("Range");
  m_nOutputs = pRanges ? pRanges->size() / 2 : 0;

  // Ranges are required for type 0 and type 4 functions. A non-zero
  // |m_nOutputs| here implied Ranges meets the requirements.
  bool bRangeRequired =
      m_Type == Type::kType0Sampled || m_Type == Type::kType4PostScript;
  if (bRangeRequired && m_nOutputs == 0)
    return false;

  if (m_nOutputs > 0) {
    size_t nOutputs = m_nOutputs * 2;
    m_Ranges = ReadArrayElementsToVector(pRanges, nOutputs);
  }

  uint32_t old_outputs = m_nOutputs;
  if (!v_Init(pObj, pVisited))
    return false;

  if (!m_Ranges.empty() && m_nOutputs > old_outputs) {
    FX_SAFE_SIZE_T nOutputs = m_nOutputs;
    nOutputs *= 2;
    m_Ranges.resize(nOutputs.ValueOrDie());
  }
  return true;
}

Optional<uint32_t> CPDF_Function::Call(pdfium::span<const float> inputs,
                                       pdfium::span<float> results) const {
  if (m_nInputs != inputs.size())
    return pdfium::nullopt;

  std::vector<float> clamped_inputs(m_nInputs);
  for (uint32_t i = 0; i < m_nInputs; i++) {
    float domain1 = m_Domains[i * 2];
    float domain2 = m_Domains[i * 2 + 1];
    if (domain1 > domain2)
      return pdfium::nullopt;

    clamped_inputs[i] = pdfium::clamp(inputs[i], domain1, domain2);
  }
  if (!v_Call(clamped_inputs, results))
    return pdfium::nullopt;

  if (m_Ranges.empty())
    return m_nOutputs;

  for (uint32_t i = 0; i < m_nOutputs; i++) {
    float range1 = m_Ranges[i * 2];
    float range2 = m_Ranges[i * 2 + 1];
    if (range1 > range2)
      return pdfium::nullopt;

    results[i] = pdfium::clamp(results[i], range1, range2);
  }
  return m_nOutputs;
}

// See PDF Reference 1.7, page 170.
float CPDF_Function::Interpolate(float x,
                                 float xmin,
                                 float xmax,
                                 float ymin,
                                 float ymax) const {
  float divisor = xmax - xmin;
  return ymin + (divisor ? (x - xmin) * (ymax - ymin) / divisor : 0);
}

#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
const CPDF_SampledFunc* CPDF_Function::ToSampledFunc() const {
  return m_Type == Type::kType0Sampled
             ? static_cast<const CPDF_SampledFunc*>(this)
             : nullptr;
}

const CPDF_ExpIntFunc* CPDF_Function::ToExpIntFunc() const {
  return m_Type == Type::kType2ExponentialInterpolation
             ? static_cast<const CPDF_ExpIntFunc*>(this)
             : nullptr;
}

const CPDF_StitchFunc* CPDF_Function::ToStitchFunc() const {
  return m_Type == Type::kType3Stitching
             ? static_cast<const CPDF_StitchFunc*>(this)
             : nullptr;
}
#endif  // defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
