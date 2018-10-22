/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/geometry/float_polygon.h"
#include "third_party/blink/renderer/platform/geometry/float_shape_helpers.h"

#include <memory>
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

static inline bool AreCollinearPoints(const FloatPoint& p0,
                                      const FloatPoint& p1,
                                      const FloatPoint& p2) {
  return !Determinant(p1 - p0, p2 - p0);
}

static inline bool AreCoincidentPoints(const FloatPoint& p0,
                                       const FloatPoint& p1) {
  return p0.X() == p1.X() && p0.Y() == p1.Y();
}

static inline bool IsPointOnLineSegment(const FloatPoint& vertex1,
                                        const FloatPoint& vertex2,
                                        const FloatPoint& point) {
  return point.X() >= std::min(vertex1.X(), vertex2.X()) &&
         point.X() <= std::max(vertex1.X(), vertex2.X()) &&
         AreCollinearPoints(vertex1, vertex2, point);
}

static inline unsigned NextVertexIndex(unsigned vertex_index,
                                       unsigned n_vertices,
                                       bool clockwise) {
  return ((clockwise) ? vertex_index + 1 : vertex_index - 1 + n_vertices) %
         n_vertices;
}

static unsigned FindNextEdgeVertexIndex(const FloatPolygon& polygon,
                                        unsigned vertex_index1,
                                        bool clockwise) {
  unsigned n_vertices = polygon.NumberOfVertices();
  unsigned vertex_index2 =
      NextVertexIndex(vertex_index1, n_vertices, clockwise);

  while (vertex_index2 && AreCoincidentPoints(polygon.VertexAt(vertex_index1),
                                              polygon.VertexAt(vertex_index2)))
    vertex_index2 = NextVertexIndex(vertex_index2, n_vertices, clockwise);

  while (vertex_index2) {
    unsigned vertex_index3 =
        NextVertexIndex(vertex_index2, n_vertices, clockwise);
    if (!AreCollinearPoints(polygon.VertexAt(vertex_index1),
                            polygon.VertexAt(vertex_index2),
                            polygon.VertexAt(vertex_index3)))
      break;
    vertex_index2 = vertex_index3;
  }

  return vertex_index2;
}

FloatPolygon::FloatPolygon(Vector<FloatPoint> vertices)
    : vertices_(std::move(vertices)) {
  unsigned n_vertices = NumberOfVertices();
  edges_.resize(n_vertices);
  empty_ = n_vertices < 3;

  if (n_vertices)
    bounding_box_.SetLocation(VertexAt(0));

  if (empty_)
    return;

  unsigned min_vertex_index = 0;
  for (unsigned i = 1; i < n_vertices; ++i) {
    const FloatPoint& vertex = VertexAt(i);
    if (vertex.Y() < VertexAt(min_vertex_index).Y() ||
        (vertex.Y() == VertexAt(min_vertex_index).Y() &&
         vertex.X() < VertexAt(min_vertex_index).X()))
      min_vertex_index = i;
  }
  FloatPoint next_vertex = VertexAt((min_vertex_index + 1) % n_vertices);
  FloatPoint prev_vertex =
      VertexAt((min_vertex_index + n_vertices - 1) % n_vertices);
  bool clockwise = Determinant(VertexAt(min_vertex_index) - prev_vertex,
                               next_vertex - prev_vertex) > 0;

  unsigned edge_index = 0;
  unsigned vertex_index1 = 0;
  do {
    bounding_box_.Extend(VertexAt(vertex_index1));
    unsigned vertex_index2 =
        FindNextEdgeVertexIndex(*this, vertex_index1, clockwise);
    edges_[edge_index].polygon_ = this;
    edges_[edge_index].vertex_index1_ = vertex_index1;
    edges_[edge_index].vertex_index2_ = vertex_index2;
    edges_[edge_index].edge_index_ = edge_index;
    ++edge_index;
    vertex_index1 = vertex_index2;
  } while (vertex_index1);

  if (edge_index > 3) {
    const FloatPolygonEdge& first_edge = edges_[0];
    const FloatPolygonEdge& last_edge = edges_[edge_index - 1];
    if (AreCollinearPoints(last_edge.Vertex1(), last_edge.Vertex2(),
                           first_edge.Vertex2())) {
      edges_[0].vertex_index1_ = last_edge.vertex_index1_;
      edge_index--;
    }
  }

  edges_.resize(edge_index);
  empty_ = edges_.size() < 3;

  if (empty_)
    return;

  for (unsigned i = 0; i < edges_.size(); ++i) {
    FloatPolygonEdge* edge = &edges_[i];
    edge_tree_.Add(EdgeInterval(edge->MinY(), edge->MaxY(), edge));
  }
}

bool FloatPolygon::OverlappingEdges(
    float min_y,
    float max_y,
    Vector<const FloatPolygonEdge*>& result) const {
  Vector<FloatPolygon::EdgeInterval> overlapping_edge_intervals;
  edge_tree_.AllOverlaps(FloatPolygon::EdgeInterval(min_y, max_y, 0),
                         overlapping_edge_intervals);
  unsigned overlapping_edge_intervals_size = overlapping_edge_intervals.size();
  result.resize(overlapping_edge_intervals_size);
  for (unsigned i = 0; i < overlapping_edge_intervals_size; ++i) {
    const FloatPolygonEdge* edge = static_cast<const FloatPolygonEdge*>(
        overlapping_edge_intervals[i].Data());
    DCHECK(edge);
    result[i] = edge;
  }
  return overlapping_edge_intervals_size > 0;
}

static inline float LeftSide(const FloatPoint& vertex1,
                             const FloatPoint& vertex2,
                             const FloatPoint& point) {
  return ((point.X() - vertex1.X()) * (vertex2.Y() - vertex1.Y())) -
         ((vertex2.X() - vertex1.X()) * (point.Y() - vertex1.Y()));
}

bool FloatPolygon::ContainsEvenOdd(const FloatPoint& point) const {
  if (!bounding_box_.Contains(point))
    return false;
  unsigned crossing_count = 0;
  for (unsigned i = 0; i < NumberOfEdges(); ++i) {
    const FloatPoint& vertex1 = EdgeAt(i).Vertex1();
    const FloatPoint& vertex2 = EdgeAt(i).Vertex2();
    if (IsPointOnLineSegment(vertex1, vertex2, point))
      return true;
    if ((vertex1.Y() <= point.Y() && vertex2.Y() > point.Y()) ||
        (vertex1.Y() > point.Y() && vertex2.Y() <= point.Y())) {
      float vt = (point.Y() - vertex1.Y()) / (vertex2.Y() - vertex1.Y());
      if (point.X() < vertex1.X() + vt * (vertex2.X() - vertex1.X()))
        ++crossing_count;
    }
  }
  return crossing_count & 1;
}

bool FloatPolygon::ContainsNonZero(const FloatPoint& point) const {
  if (!bounding_box_.Contains(point))
    return false;
  int winding_number = 0;
  for (unsigned i = 0; i < NumberOfEdges(); ++i) {
    const FloatPoint& vertex1 = EdgeAt(i).Vertex1();
    const FloatPoint& vertex2 = EdgeAt(i).Vertex2();
    if (IsPointOnLineSegment(vertex1, vertex2, point))
      return true;
    if (vertex2.Y() <= point.Y()) {
      if ((vertex1.Y() > point.Y()) && (LeftSide(vertex1, vertex2, point) > 0))
        ++winding_number;
    } else if (vertex2.Y() >= point.Y()) {
      if ((vertex1.Y() <= point.Y()) && (LeftSide(vertex1, vertex2, point) < 0))
        --winding_number;
    }
  }
  return winding_number;
}

bool VertexPair::Intersection(const VertexPair& other,
                              FloatPoint& point) const {
  // See: http://paulbourke.net/geometry/pointlineplane/,
  // "Intersection point of two lines in 2 dimensions"

  const FloatSize& this_delta = Vertex2() - Vertex1();
  const FloatSize& other_delta = other.Vertex2() - other.Vertex1();
  float denominator = Determinant(this_delta, other_delta);
  if (!denominator)
    return false;

  // The two line segments: "this" vertex1,vertex2 and "other" vertex1,vertex2,
  // have been defined in parametric form. Each point on the line segment is:
  // vertex1 + u * (vertex2 - vertex1), when 0 <= u <= 1. We're computing the
  // values of u for each line at their intersection point.

  const FloatSize& vertex1_delta = Vertex1() - other.Vertex1();
  float u_this_line = Determinant(other_delta, vertex1_delta) / denominator;
  float u_other_line = Determinant(this_delta, vertex1_delta) / denominator;

  if (u_this_line < 0 || u_other_line < 0 || u_this_line > 1 ||
      u_other_line > 1)
    return false;

  point = Vertex1() + u_this_line * this_delta;
  return true;
}

}  // namespace blink
