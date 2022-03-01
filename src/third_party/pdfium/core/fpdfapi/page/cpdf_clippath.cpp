// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_clippath.h"

#include <utility>

#include "core/fpdfapi/page/cpdf_textobject.h"

CPDF_ClipPath::CPDF_ClipPath() = default;

CPDF_ClipPath::CPDF_ClipPath(const CPDF_ClipPath& that) = default;

CPDF_ClipPath& CPDF_ClipPath::operator=(const CPDF_ClipPath& that) = default;

CPDF_ClipPath::~CPDF_ClipPath() = default;

size_t CPDF_ClipPath::GetPathCount() const {
  return m_Ref.GetObject()->m_PathAndTypeList.size();
}

CPDF_Path CPDF_ClipPath::GetPath(size_t i) const {
  return m_Ref.GetObject()->m_PathAndTypeList[i].first;
}

CFX_FillRenderOptions::FillType CPDF_ClipPath::GetClipType(size_t i) const {
  return m_Ref.GetObject()->m_PathAndTypeList[i].second;
}

size_t CPDF_ClipPath::GetTextCount() const {
  return m_Ref.GetObject()->m_TextList.size();
}

CPDF_TextObject* CPDF_ClipPath::GetText(size_t i) const {
  return m_Ref.GetObject()->m_TextList[i].get();
}

CFX_FloatRect CPDF_ClipPath::GetClipBox() const {
  CFX_FloatRect rect;
  bool bStarted = false;
  if (GetPathCount() > 0) {
    rect = GetPath(0).GetBoundingBox();
    for (size_t i = 1; i < GetPathCount(); ++i) {
      CFX_FloatRect path_rect = GetPath(i).GetBoundingBox();
      rect.Intersect(path_rect);
    }
    bStarted = true;
  }

  CFX_FloatRect layer_rect;
  bool bLayerStarted = false;
  for (size_t i = 0; i < GetTextCount(); ++i) {
    CPDF_TextObject* pTextObj = GetText(i);
    if (pTextObj) {
      if (bLayerStarted) {
        layer_rect.Union(CFX_FloatRect(pTextObj->GetBBox()));
      } else {
        layer_rect = CFX_FloatRect(pTextObj->GetBBox());
        bLayerStarted = true;
      }
    } else {
      if (bStarted) {
        rect.Intersect(layer_rect);
      } else {
        rect = layer_rect;
        bStarted = true;
      }
      bLayerStarted = false;
    }
  }
  return rect;
}

void CPDF_ClipPath::AppendPath(CPDF_Path path,
                               CFX_FillRenderOptions::FillType type) {
  PathData* pData = m_Ref.GetPrivateCopy();
  pData->m_PathAndTypeList.push_back(std::make_pair(path, type));
}

void CPDF_ClipPath::AppendPathWithAutoMerge(
    CPDF_Path path,
    CFX_FillRenderOptions::FillType type) {
  PathData* pData = m_Ref.GetPrivateCopy();
  if (!pData->m_PathAndTypeList.empty()) {
    const CPDF_Path& old_path = pData->m_PathAndTypeList.back().first;
    if (old_path.IsRect()) {
      CFX_PointF point0 = old_path.GetPoint(0);
      CFX_PointF point2 = old_path.GetPoint(2);
      CFX_FloatRect old_rect(point0.x, point0.y, point2.x, point2.y);
      CFX_FloatRect new_rect = path.GetBoundingBox();
      if (old_rect.Contains(new_rect))
        pData->m_PathAndTypeList.pop_back();
    }
  }
  AppendPath(path, type);
}

void CPDF_ClipPath::AppendTexts(
    std::vector<std::unique_ptr<CPDF_TextObject>>* pTexts) {
  constexpr size_t kMaxTextObjects = 1024;
  PathData* pData = m_Ref.GetPrivateCopy();
  if (pData->m_TextList.size() + pTexts->size() <= kMaxTextObjects) {
    for (size_t i = 0; i < pTexts->size(); i++)
      pData->m_TextList.push_back(std::move((*pTexts)[i]));
    pData->m_TextList.push_back(nullptr);
  }
  pTexts->clear();
}

void CPDF_ClipPath::CopyClipPath(const CPDF_ClipPath& that) {
  if (*this == that || !that.HasRef())
    return;

  for (size_t i = 0; i < that.GetPathCount(); ++i)
    AppendPath(that.GetPath(i), that.GetClipType(i));
}

void CPDF_ClipPath::Transform(const CFX_Matrix& matrix) {
  PathData* pData = m_Ref.GetPrivateCopy();
  for (auto& obj : pData->m_PathAndTypeList)
    obj.first.Transform(matrix);

  for (auto& text : pData->m_TextList) {
    if (text)
      text->Transform(matrix);
  }
}

CPDF_ClipPath::PathData::PathData() = default;

CPDF_ClipPath::PathData::PathData(const PathData& that) {
  m_PathAndTypeList = that.m_PathAndTypeList;

  m_TextList.resize(that.m_TextList.size());
  for (size_t i = 0; i < that.m_TextList.size(); ++i) {
    if (that.m_TextList[i])
      m_TextList[i] = that.m_TextList[i]->Clone();
  }
}

CPDF_ClipPath::PathData::~PathData() = default;

RetainPtr<CPDF_ClipPath::PathData> CPDF_ClipPath::PathData::Clone() const {
  return pdfium::MakeRetain<CPDF_ClipPath::PathData>(*this);
}
