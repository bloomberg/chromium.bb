// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/cfx_pathdata.h"

#include "core/fxcrt/fx_system.h"
#include "third_party/base/numerics/safe_math.h"

namespace {

bool IsFoldingVerticalLine(const CFX_PointF& a,
                           const CFX_PointF& b,
                           const CFX_PointF& c) {
  return a.x == b.x && b.x == c.x && (b.y - a.y) * (b.y - c.y) > 0;
}

bool IsFoldingHorizontalLine(const CFX_PointF& a,
                             const CFX_PointF& b,
                             const CFX_PointF& c) {
  return a.y == b.y && b.y == c.y && (b.x - a.x) * (b.x - c.x) > 0;
}

bool IsFoldingDiagonalLine(const CFX_PointF& a,
                           const CFX_PointF& b,
                           const CFX_PointF& c) {
  return a.x != b.x && c.x != b.x && a.y != b.y && c.y != b.y &&
         (a.y - b.y) * (c.x - b.x) == (c.y - b.y) * (a.x - b.x);
}

void UpdateLineEndPoints(CFX_FloatRect* rect,
                         const CFX_PointF& start_pos,
                         const CFX_PointF& end_pos,
                         float hw) {
  if (start_pos.x == end_pos.x) {
    if (start_pos.y == end_pos.y) {
      rect->UpdateRect(end_pos + CFX_PointF(hw, hw));
      rect->UpdateRect(end_pos - CFX_PointF(hw, hw));
      return;
    }

    float point_y;
    if (end_pos.y < start_pos.y)
      point_y = end_pos.y - hw;
    else
      point_y = end_pos.y + hw;

    rect->UpdateRect(CFX_PointF(end_pos.x + hw, point_y));
    rect->UpdateRect(CFX_PointF(end_pos.x - hw, point_y));
    return;
  }

  if (start_pos.y == end_pos.y) {
    float point_x;
    if (end_pos.x < start_pos.x)
      point_x = end_pos.x - hw;
    else
      point_x = end_pos.x + hw;

    rect->UpdateRect(CFX_PointF(point_x, end_pos.y + hw));
    rect->UpdateRect(CFX_PointF(point_x, end_pos.y - hw));
    return;
  }

  CFX_PointF diff = end_pos - start_pos;
  float ll = FXSYS_sqrt2(diff.x, diff.y);
  float mx = end_pos.x + hw * diff.x / ll;
  float my = end_pos.y + hw * diff.y / ll;
  float dx1 = hw * diff.y / ll;
  float dy1 = hw * diff.x / ll;
  rect->UpdateRect(CFX_PointF(mx - dx1, my + dy1));
  rect->UpdateRect(CFX_PointF(mx + dx1, my - dy1));
}

void UpdateLineJoinPoints(CFX_FloatRect* rect,
                          const CFX_PointF& start_pos,
                          const CFX_PointF& mid_pos,
                          const CFX_PointF& end_pos,
                          float half_width,
                          float miter_limit) {
  float start_k = 0;
  float start_c = 0;
  float end_k = 0;
  float end_c = 0;
  float start_len = 0;
  float start_dc = 0;
  float end_len = 0;
  float end_dc = 0;
  float one_twentieth = 1.0f / 20;

  bool bStartVert = fabs(start_pos.x - mid_pos.x) < one_twentieth;
  bool bEndVert = fabs(mid_pos.x - end_pos.x) < one_twentieth;
  if (bStartVert && bEndVert) {
    int start_dir = mid_pos.y > start_pos.y ? 1 : -1;
    float point_y = mid_pos.y + half_width * start_dir;
    rect->UpdateRect(CFX_PointF(mid_pos.x + half_width, point_y));
    rect->UpdateRect(CFX_PointF(mid_pos.x - half_width, point_y));
    return;
  }

  if (!bStartVert) {
    CFX_PointF start_to_mid = start_pos - mid_pos;
    start_k = (mid_pos.y - start_pos.y) / (mid_pos.x - start_pos.x);
    start_c = mid_pos.y - (start_k * mid_pos.x);
    start_len = FXSYS_sqrt2(start_to_mid.x, start_to_mid.y);
    start_dc =
        static_cast<float>(fabs(half_width * start_len / start_to_mid.x));
  }
  if (!bEndVert) {
    CFX_PointF end_to_mid = end_pos - mid_pos;
    end_k = end_to_mid.y / end_to_mid.x;
    end_c = mid_pos.y - (end_k * mid_pos.x);
    end_len = FXSYS_sqrt2(end_to_mid.x, end_to_mid.y);
    end_dc = static_cast<float>(fabs(half_width * end_len / end_to_mid.x));
  }
  if (bStartVert) {
    CFX_PointF outside(start_pos.x, 0);
    if (end_pos.x < start_pos.x)
      outside.x += half_width;
    else
      outside.x -= half_width;

    if (start_pos.y < (end_k * start_pos.x) + end_c)
      outside.y = (end_k * outside.x) + end_c + end_dc;
    else
      outside.y = (end_k * outside.x) + end_c - end_dc;

    rect->UpdateRect(outside);
    return;
  }

  if (bEndVert) {
    CFX_PointF outside(end_pos.x, 0);
    if (start_pos.x < end_pos.x)
      outside.x += half_width;
    else
      outside.x -= half_width;

    if (end_pos.y < (start_k * end_pos.x) + start_c)
      outside.y = (start_k * outside.x) + start_c + start_dc;
    else
      outside.y = (start_k * outside.x) + start_c - start_dc;

    rect->UpdateRect(outside);
    return;
  }

  if (fabs(start_k - end_k) < one_twentieth) {
    int start_dir = mid_pos.x > start_pos.x ? 1 : -1;
    int end_dir = end_pos.x > mid_pos.x ? 1 : -1;
    if (start_dir == end_dir)
      UpdateLineEndPoints(rect, mid_pos, end_pos, half_width);
    else
      UpdateLineEndPoints(rect, start_pos, mid_pos, half_width);
    return;
  }

  float start_outside_c = start_c;
  if (end_pos.y < (start_k * end_pos.x) + start_c)
    start_outside_c += start_dc;
  else
    start_outside_c -= start_dc;

  float end_outside_c = end_c;
  if (start_pos.y < (end_k * start_pos.x) + end_c)
    end_outside_c += end_dc;
  else
    end_outside_c -= end_dc;

  float join_x = (end_outside_c - start_outside_c) / (start_k - end_k);
  float join_y = start_k * join_x + start_outside_c;
  rect->UpdateRect(CFX_PointF(join_x, join_y));
}

}  // namespace

FX_PATHPOINT::FX_PATHPOINT() = default;

FX_PATHPOINT::FX_PATHPOINT(const CFX_PointF& point, FXPT_TYPE type, bool close)
    : m_Point(point), m_Type(type), m_CloseFigure(close) {}

FX_PATHPOINT::FX_PATHPOINT(const FX_PATHPOINT& other) = default;

FX_PATHPOINT::~FX_PATHPOINT() = default;

CFX_PathData::CFX_PathData() = default;

CFX_PathData::CFX_PathData(const CFX_PathData& src) = default;

CFX_PathData::CFX_PathData(CFX_PathData&& src) = default;

CFX_PathData::~CFX_PathData() = default;

void CFX_PathData::Clear() {
  m_Points.clear();
}

void CFX_PathData::ClosePath() {
  if (m_Points.empty())
    return;
  m_Points.back().m_CloseFigure = true;
}

void CFX_PathData::Append(const CFX_PathData* pSrc, const CFX_Matrix* pMatrix) {
  if (pSrc->m_Points.empty())
    return;

  size_t cur_size = m_Points.size();
  m_Points.insert(m_Points.end(), pSrc->m_Points.begin(), pSrc->m_Points.end());

  if (!pMatrix)
    return;

  for (size_t i = cur_size; i < m_Points.size(); i++)
    m_Points[i].m_Point = pMatrix->Transform(m_Points[i].m_Point);
}

void CFX_PathData::AppendPoint(const CFX_PointF& point,
                               FXPT_TYPE type,
                               bool closeFigure) {
  m_Points.push_back(FX_PATHPOINT(point, type, closeFigure));
}

void CFX_PathData::AppendLine(const CFX_PointF& pt1, const CFX_PointF& pt2) {
  if (m_Points.empty() || fabs(m_Points.back().m_Point.x - pt1.x) > 0.001 ||
      fabs(m_Points.back().m_Point.y - pt1.y) > 0.001) {
    AppendPoint(pt1, FXPT_TYPE::MoveTo, false);
  }
  AppendPoint(pt2, FXPT_TYPE::LineTo, false);
}

void CFX_PathData::AppendFloatRect(const CFX_FloatRect& rect) {
  return AppendRect(rect.left, rect.bottom, rect.right, rect.top);
}

void CFX_PathData::AppendRect(float left,
                              float bottom,
                              float right,
                              float top) {
  CFX_PointF left_bottom(left, bottom);
  CFX_PointF left_top(left, top);
  CFX_PointF right_top(right, top);
  CFX_PointF right_bottom(right, bottom);

  AppendLine(left_bottom, left_top);
  AppendLine(left_top, right_top);
  AppendLine(right_top, right_bottom);
  AppendLine(right_bottom, left_bottom);
  ClosePath();
}

CFX_FloatRect CFX_PathData::GetBoundingBox() const {
  if (m_Points.empty())
    return CFX_FloatRect();

  CFX_FloatRect rect;
  rect.InitRect(m_Points[0].m_Point);
  for (size_t i = 1; i < m_Points.size(); i++)
    rect.UpdateRect(m_Points[i].m_Point);
  return rect;
}

CFX_FloatRect CFX_PathData::GetBoundingBox(float line_width,
                                           float miter_limit) const {
  CFX_FloatRect rect(100000.0f, 100000.0f, -100000.0f, -100000.0f);
  size_t iPoint = 0;
  float half_width = line_width;
  int iStartPoint = 0;
  int iEndPoint = 0;
  int iMiddlePoint = 0;
  bool bJoin;
  while (iPoint < m_Points.size()) {
    if (m_Points[iPoint].IsTypeAndOpen(FXPT_TYPE::MoveTo)) {
      if (iPoint + 1 == m_Points.size())
        break;

      iStartPoint = iPoint + 1;
      iEndPoint = iPoint;
      bJoin = false;
    } else {
      if (m_Points[iPoint].IsTypeAndOpen(FXPT_TYPE::BezierTo)) {
        rect.UpdateRect(m_Points[iPoint].m_Point);
        rect.UpdateRect(m_Points[iPoint + 1].m_Point);
        iPoint += 2;
      }
      if (iPoint == m_Points.size() - 1 ||
          m_Points[iPoint + 1].IsTypeAndOpen(FXPT_TYPE::MoveTo)) {
        iStartPoint = iPoint - 1;
        iEndPoint = iPoint;
        bJoin = false;
      } else {
        iStartPoint = iPoint - 1;
        iMiddlePoint = iPoint;
        iEndPoint = iPoint + 1;
        bJoin = true;
      }
    }

    CFX_PointF start_pos = m_Points[iStartPoint].m_Point;
    CFX_PointF end_pos = m_Points[iEndPoint].m_Point;
    if (bJoin) {
      CFX_PointF mid_pos = m_Points[iMiddlePoint].m_Point;
      UpdateLineJoinPoints(&rect, start_pos, mid_pos, end_pos, half_width,
                           miter_limit);
    } else {
      UpdateLineEndPoints(&rect, start_pos, end_pos, half_width);
    }
    iPoint++;
  }
  return rect;
}

void CFX_PathData::Transform(const CFX_Matrix& matrix) {
  for (auto& point : m_Points)
    point.m_Point = matrix.Transform(point.m_Point);
}

bool CFX_PathData::GetZeroAreaPath(const CFX_Matrix* pMatrix,
                                   bool bAdjust,
                                   CFX_PathData* NewPath,
                                   bool* bThin,
                                   bool* setIdentity) const {
  *setIdentity = false;
  if (m_Points.size() < 3)
    return false;

  if (m_Points.size() == 3 && m_Points[0].m_Type == FXPT_TYPE::MoveTo &&
      m_Points[1].m_Type == FXPT_TYPE::LineTo &&
      m_Points[2].m_Type == FXPT_TYPE::LineTo &&
      m_Points[0].m_Point == m_Points[2].m_Point) {
    for (size_t i = 0; i < 2; i++) {
      CFX_PointF point = m_Points[i].m_Point;
      if (bAdjust) {
        if (pMatrix)
          point = pMatrix->Transform(point);

        point = CFX_PointF(static_cast<int>(point.x) + 0.5f,
                           static_cast<int>(point.y) + 0.5f);
      }
      NewPath->AppendPoint(
          point, i == 0 ? FXPT_TYPE::MoveTo : FXPT_TYPE::LineTo, false);
    }
    if (bAdjust && pMatrix)
      *setIdentity = true;

    // Note, they both have to be not equal.
    if (m_Points[0].m_Point.x != m_Points[1].m_Point.x &&
        m_Points[0].m_Point.y != m_Points[1].m_Point.y) {
      *bThin = true;
    }
    return true;
  }

  if (((m_Points.size() > 3) && (m_Points.size() % 2))) {
    int mid = m_Points.size() / 2;
    bool bZeroArea = false;
    CFX_PathData t_path;
    for (int i = 0; i < mid; i++) {
      if (!(m_Points[mid - i - 1].m_Point == m_Points[mid + i + 1].m_Point &&
            m_Points[mid - i - 1].m_Type != FXPT_TYPE::BezierTo &&
            m_Points[mid + i + 1].m_Type != FXPT_TYPE::BezierTo)) {
        bZeroArea = true;
        break;
      }

      t_path.AppendPoint(m_Points[mid - i].m_Point, FXPT_TYPE::MoveTo, false);
      t_path.AppendPoint(m_Points[mid - i - 1].m_Point, FXPT_TYPE::LineTo,
                         false);
    }
    if (!bZeroArea) {
      NewPath->Append(&t_path, nullptr);
      *bThin = true;
      return true;
    }
  }

  int startPoint = 0;
  for (size_t i = 0; i < m_Points.size(); i++) {
    FXPT_TYPE point_type = m_Points[i].m_Type;
    if (point_type == FXPT_TYPE::MoveTo) {
      startPoint = i;
      continue;
    }

    if (point_type == FXPT_TYPE::BezierTo) {
      i += 2;
      continue;
    }

    ASSERT(point_type == FXPT_TYPE::LineTo);
    int next_index =
        (i + 1 - startPoint) % (m_Points.size() - startPoint) + startPoint;
    const FX_PATHPOINT& next = m_Points[next_index];
    if (next.m_Type == FXPT_TYPE::BezierTo || next.m_Type == FXPT_TYPE::MoveTo)
      continue;

    const FX_PATHPOINT& prev = m_Points[i - 1];
    const FX_PATHPOINT& cur = m_Points[i];
    if (IsFoldingVerticalLine(prev.m_Point, cur.m_Point, next.m_Point)) {
      bool use_prev = fabs(cur.m_Point.y - prev.m_Point.y) <
                      fabs(cur.m_Point.y - next.m_Point.y);
      const FX_PATHPOINT& start = use_prev ? prev : cur;
      const FX_PATHPOINT& end = use_prev ? m_Points[next_index - 1] : next;
      NewPath->AppendPoint(start.m_Point, FXPT_TYPE::MoveTo, false);
      NewPath->AppendPoint(end.m_Point, FXPT_TYPE::LineTo, false);
      continue;
    }

    if (IsFoldingHorizontalLine(prev.m_Point, cur.m_Point, next.m_Point) ||
        IsFoldingDiagonalLine(prev.m_Point, cur.m_Point, next.m_Point)) {
      bool use_prev = fabs(cur.m_Point.x - prev.m_Point.x) <
                      fabs(cur.m_Point.x - next.m_Point.x);
      const FX_PATHPOINT& start = use_prev ? prev : cur;
      const FX_PATHPOINT& end = use_prev ? m_Points[next_index - 1] : next;
      NewPath->AppendPoint(start.m_Point, FXPT_TYPE::MoveTo, false);
      NewPath->AppendPoint(end.m_Point, FXPT_TYPE::LineTo, false);
      continue;
    }
  }

  size_t new_path_size = NewPath->GetPoints().size();
  if (m_Points.size() > 3 && new_path_size > 0)
    *bThin = true;
  return new_path_size != 0;
}

bool CFX_PathData::IsRect() const {
  if (m_Points.size() != 5 && m_Points.size() != 4)
    return false;

  if ((m_Points.size() == 5 && m_Points[0].m_Point != m_Points[4].m_Point) ||
      m_Points[0].m_Point == m_Points[2].m_Point ||
      m_Points[1].m_Point == m_Points[3].m_Point) {
    return false;
  }
  // Note, both x,y have to not equal.
  if (m_Points[0].m_Point.x != m_Points[3].m_Point.x &&
      m_Points[0].m_Point.y != m_Points[3].m_Point.y) {
    return false;
  }

  for (int i = 1; i < 4; i++) {
    if (m_Points[i].m_Type != FXPT_TYPE::LineTo)
      return false;
    // Note, both x,y have to not equal.
    if (m_Points[i].m_Point.x != m_Points[i - 1].m_Point.x &&
        m_Points[i].m_Point.y != m_Points[i - 1].m_Point.y) {
      return false;
    }
  }
  return m_Points.size() == 5 || m_Points[3].m_CloseFigure;
}

bool CFX_PathData::IsRect(const CFX_Matrix* pMatrix,
                          CFX_FloatRect* pRect) const {
  if (!pMatrix) {
    if (!IsRect())
      return false;

    if (pRect) {
      pRect->left = m_Points[0].m_Point.x;
      pRect->right = m_Points[2].m_Point.x;
      pRect->bottom = m_Points[0].m_Point.y;
      pRect->top = m_Points[2].m_Point.y;
      pRect->Normalize();
    }
    return true;
  }

  if (m_Points.size() != 5 && m_Points.size() != 4)
    return false;

  if ((m_Points.size() == 5 && m_Points[0].m_Point != m_Points[4].m_Point) ||
      m_Points[1].m_Point == m_Points[3].m_Point) {
    return false;
  }
  // Note, both x,y not equal.
  if (m_Points.size() == 4 && m_Points[0].m_Point.x != m_Points[3].m_Point.x &&
      m_Points[0].m_Point.y != m_Points[3].m_Point.y) {
    return false;
  }

  CFX_PointF points[5];
  for (size_t i = 0; i < m_Points.size(); i++) {
    points[i] = pMatrix->Transform(m_Points[i].m_Point);

    if (i == 0)
      continue;
    if (m_Points[i].m_Type != FXPT_TYPE::LineTo)
      return false;
    if (points[i].x != points[i - 1].x && points[i].y != points[i - 1].y)
      return false;
  }

  if (pRect) {
    pRect->left = points[0].x;
    pRect->right = points[2].x;
    pRect->bottom = points[0].y;
    pRect->top = points[2].y;
    pRect->Normalize();
  }
  return true;
}

CFX_RetainablePathData::CFX_RetainablePathData() = default;

// Note: can't default the copy constructor since Retainable<> has a deleted
// copy constructor (as it should). Instead, we want the default Retainable<>
// constructor to be invoked so as to create a copy with a ref-count of 1 as
// of the time it is created, then populate the remainder of the members from
// the |src| object.
CFX_RetainablePathData::CFX_RetainablePathData(
    const CFX_RetainablePathData& src)
    : CFX_PathData(src) {}

CFX_RetainablePathData::~CFX_RetainablePathData() = default;

RetainPtr<CFX_RetainablePathData> CFX_RetainablePathData::Clone() const {
  return pdfium::MakeRetain<CFX_RetainablePathData>(*this);
}
