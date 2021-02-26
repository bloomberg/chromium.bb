// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_meshstream.h"

#include "core/fpdfapi/page/cpdf_colorspace.h"
#include "core/fpdfapi/page/cpdf_function.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "third_party/base/span.h"

namespace {

// See PDF Reference 1.7, page 315, table 4.32. (Also table 4.33 and 4.34)
bool ShouldCheckBPC(ShadingType type) {
  switch (type) {
    case kFreeFormGouraudTriangleMeshShading:
    case kLatticeFormGouraudTriangleMeshShading:
    case kCoonsPatchMeshShading:
    case kTensorProductPatchMeshShading:
      return true;
    default:
      return false;
  }
}

// Same references as ShouldCheckBPC() above.
bool IsValidBitsPerComponent(uint32_t x) {
  switch (x) {
    case 1:
    case 2:
    case 4:
    case 8:
    case 12:
    case 16:
      return true;
    default:
      return false;
  }
}

// Same references as ShouldCheckBPC() above.
bool IsValidBitsPerCoordinate(uint32_t x) {
  switch (x) {
    case 1:
    case 2:
    case 4:
    case 8:
    case 12:
    case 16:
    case 24:
    case 32:
      return true;
    default:
      return false;
  }
}

// See PDF Reference 1.7, page 315, table 4.32. (Also table 4.34)
bool ShouldCheckBitsPerFlag(ShadingType type) {
  switch (type) {
    case kFreeFormGouraudTriangleMeshShading:
    case kCoonsPatchMeshShading:
    case kTensorProductPatchMeshShading:
      return true;
    default:
      return false;
  }
}

// Same references as ShouldCheckBitsPerFlag() above.
bool IsValidBitsPerFlag(uint32_t x) {
  switch (x) {
    case 2:
    case 4:
    case 8:
      return true;
    default:
      return false;
  }
}

}  // namespace

CPDF_MeshVertex::CPDF_MeshVertex() = default;

CPDF_MeshVertex::CPDF_MeshVertex(const CPDF_MeshVertex&) = default;

CPDF_MeshVertex::~CPDF_MeshVertex() = default;

CPDF_MeshStream::CPDF_MeshStream(
    ShadingType type,
    const std::vector<std::unique_ptr<CPDF_Function>>& funcs,
    const CPDF_Stream* pShadingStream,
    const RetainPtr<CPDF_ColorSpace>& pCS)
    : m_type(type),
      m_funcs(funcs),
      m_pShadingStream(pShadingStream),
      m_pCS(pCS),
      m_pStream(pdfium::MakeRetain<CPDF_StreamAcc>(pShadingStream)) {
}

CPDF_MeshStream::~CPDF_MeshStream() = default;

bool CPDF_MeshStream::Load() {
  m_pStream->LoadAllDataFiltered();
  m_BitStream = std::make_unique<CFX_BitStream>(m_pStream->GetSpan());
  const CPDF_Dictionary* pDict = m_pShadingStream->GetDict();
  m_nCoordBits = pDict->GetIntegerFor("BitsPerCoordinate");
  m_nComponentBits = pDict->GetIntegerFor("BitsPerComponent");
  if (ShouldCheckBPC(m_type)) {
    if (!IsValidBitsPerCoordinate(m_nCoordBits))
      return false;
    if (!IsValidBitsPerComponent(m_nComponentBits))
      return false;
  }

  m_nFlagBits = pDict->GetIntegerFor("BitsPerFlag");
  if (ShouldCheckBitsPerFlag(m_type) && !IsValidBitsPerFlag(m_nFlagBits))
    return false;

  uint32_t nComponents = m_pCS->CountComponents();
  if (nComponents > kMaxComponents)
    return false;

  m_nComponents = m_funcs.empty() ? nComponents : 1;
  const CPDF_Array* pDecode = pDict->GetArrayFor("Decode");
  if (!pDecode || pDecode->size() != 4 + m_nComponents * 2)
    return false;

  m_xmin = pDecode->GetNumberAt(0);
  m_xmax = pDecode->GetNumberAt(1);
  m_ymin = pDecode->GetNumberAt(2);
  m_ymax = pDecode->GetNumberAt(3);
  for (uint32_t i = 0; i < m_nComponents; ++i) {
    m_ColorMin[i] = pDecode->GetNumberAt(i * 2 + 4);
    m_ColorMax[i] = pDecode->GetNumberAt(i * 2 + 5);
  }

  if (ShouldCheckBPC(m_type)) {
    m_CoordMax = m_nCoordBits == 32 ? -1 : (1 << m_nCoordBits) - 1;
    m_ComponentMax = (1 << m_nComponentBits) - 1;
  }
  return true;
}

bool CPDF_MeshStream::CanReadFlag() const {
  return m_BitStream->BitsRemaining() >= m_nFlagBits;
}

bool CPDF_MeshStream::CanReadCoords() const {
  return m_BitStream->BitsRemaining() / 2 >= m_nCoordBits;
}

bool CPDF_MeshStream::CanReadColor() const {
  return m_BitStream->BitsRemaining() / m_nComponentBits >= m_nComponents;
}

uint32_t CPDF_MeshStream::ReadFlag() {
  ASSERT(ShouldCheckBitsPerFlag(m_type));
  return m_BitStream->GetBits(m_nFlagBits) & 0x03;
}

CFX_PointF CPDF_MeshStream::ReadCoords() {
  ASSERT(ShouldCheckBPC(m_type));

  CFX_PointF pos;
  if (m_nCoordBits == 32) {
    pos.x = m_xmin + m_BitStream->GetBits(m_nCoordBits) * (m_xmax - m_xmin) /
                         static_cast<double>(m_CoordMax);
    pos.y = m_ymin + m_BitStream->GetBits(m_nCoordBits) * (m_ymax - m_ymin) /
                         static_cast<double>(m_CoordMax);
  } else {
    pos.x = m_xmin +
            m_BitStream->GetBits(m_nCoordBits) * (m_xmax - m_xmin) / m_CoordMax;
    pos.y = m_ymin +
            m_BitStream->GetBits(m_nCoordBits) * (m_ymax - m_ymin) / m_CoordMax;
  }
  return pos;
}

std::tuple<float, float, float> CPDF_MeshStream::ReadColor() {
  ASSERT(ShouldCheckBPC(m_type));

  float color_value[kMaxComponents];
  for (uint32_t i = 0; i < m_nComponents; ++i) {
    color_value[i] = m_ColorMin[i] + m_BitStream->GetBits(m_nComponentBits) *
                                         (m_ColorMax[i] - m_ColorMin[i]) /
                                         m_ComponentMax;
  }

  float r = 0.0;
  float g = 0.0;
  float b = 0.0;
  if (m_funcs.empty()) {
    m_pCS->GetRGB(color_value, &r, &g, &b);
    return std::tuple<float, float, float>(r, g, b);
  }

  float result[kMaxComponents] = {};
  int nResults;
  for (const auto& func : m_funcs) {
    if (func && func->CountOutputs() <= kMaxComponents)
      func->Call(color_value, 1, result, &nResults);
  }

  m_pCS->GetRGB(result, &r, &g, &b);
  return std::tuple<float, float, float>(r, g, b);
}

bool CPDF_MeshStream::ReadVertex(const CFX_Matrix& pObject2Bitmap,
                                 CPDF_MeshVertex* vertex,
                                 uint32_t* flag) {
  if (!CanReadFlag())
    return false;
  *flag = ReadFlag();

  if (!CanReadCoords())
    return false;
  vertex->position = pObject2Bitmap.Transform(ReadCoords());

  if (!CanReadColor())
    return false;
  std::tie(vertex->r, vertex->g, vertex->b) = ReadColor();
  m_BitStream->ByteAlign();
  return true;
}

std::vector<CPDF_MeshVertex> CPDF_MeshStream::ReadVertexRow(
    const CFX_Matrix& pObject2Bitmap,
    int count) {
  std::vector<CPDF_MeshVertex> vertices;
  for (int i = 0; i < count; ++i) {
    if (m_BitStream->IsEOF() || !CanReadCoords())
      return std::vector<CPDF_MeshVertex>();

    vertices.push_back(CPDF_MeshVertex());
    CPDF_MeshVertex& vertex = vertices.back();
    vertex.position = pObject2Bitmap.Transform(ReadCoords());
    if (!CanReadColor())
      return std::vector<CPDF_MeshVertex>();

    std::tie(vertex.r, vertex.g, vertex.b) = ReadColor();
    m_BitStream->ByteAlign();
  }
  return vertices;
}
