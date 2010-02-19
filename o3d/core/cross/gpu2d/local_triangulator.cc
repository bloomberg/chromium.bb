/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/cross/gpu2d/local_triangulator.h"

#include <algorithm>

#include "core/cross/gpu2d/cubic_math_utils.h"

namespace o3d {
namespace gpu2d {

using cubic::ApproxEqual;
using cubic::Distance;
using cubic::LinesIntersect;
using cubic::PointInTriangle;

bool LocalTriangulator::Triangle::Contains(LocalTriangulator::Vertex* v) {
  for (int i = 0; i < 3; i++) {
    if (v_[i] == v)
      return true;
  }
  return false;
}

LocalTriangulator::Vertex* LocalTriangulator::Triangle::NextVertex(
    LocalTriangulator::Vertex* cur,
    bool ccw) {
  int idx = IndexForVertex(cur);
  DCHECK(idx >= 0);
  if (ccw) {
    ++idx;
  } else {
    --idx;
  }
  if (idx < 0) {
    idx += 3;
  } else {
    idx = idx % 3;
  }
  return v_[idx];
}

int LocalTriangulator::Triangle::IndexForVertex(
    LocalTriangulator::Vertex* vertex) {
  for (int i = 0; i < 3; i++) {
    if (v_[i] == vertex)
      return i;
  }
  return -1;
}

void LocalTriangulator::Triangle::MakeCCW() {
  // Possibly swaps two vertices so that the triangle's vertices are
  // always specified in counterclockwise order. This orders the
  // vertices canonically when walking the interior edges from the
  // start to the end vertex.
  Vector3 vec0(v_[0]->x(), v_[0]->y(), 0.0f);
  Vector3 vec1(v_[1]->x(), v_[1]->y(), 0.0f);
  Vector3 vec2(v_[2]->x(), v_[2]->y(), 0.0f);
  Vector3 diff1 = vec1 - vec0;
  Vector3 diff2 = vec2 - vec0;
  Vector3 cp = cross(diff1, diff2);
  if (cp.getZ() < 0.0f) {
    std::swap(v_[1], v_[2]);
  }
}

LocalTriangulator::LocalTriangulator() {
  Reset();
}

void LocalTriangulator::Reset() {
  num_triangles_ = 0;
  num_interior_vertices_ = 0;
  for (int i = 0; i < 4; i++) {
    interior_vertices_[i] = NULL;
    vertices_[i].ResetFlags();
  }
}

void LocalTriangulator::Triangulate(bool compute_interior_vertices,
                                    bool fill_right_side) {
  Reset();

  bool done = false;

  vertices_[3].set_end(true);

  // First test for degenerate cases.
  for (int i = 0; i < 4 && !done; i++) {
    for (int j = i + 1; j < 4 && !done; j++) {
      if (ApproxEqual(vertices_[i].x(), vertices_[i].y(),
                      vertices_[j].x(), vertices_[j].y())) {
        // Two of the vertices are coincident, so we can eliminate at
        // least one triangle. We might be able to eliminate the other
        // as well, but this seems sufficient to avoid degenerate
        // triangulations.
        int indices[3] = { 0 };
        int index = 0;
        for (int k = 0; k < 4; k++) {
          if (k != j)
            indices[index++] = k;
        }
        AddTriangle(&vertices_[indices[0]],
                    &vertices_[indices[1]],
                    &vertices_[indices[2]]);
        done = true;
      }
    }
  }

  if (!done) {
    // See whether any of the points are fully contained in the
    // triangle defined by the other three.
    for (int i = 0; i < 4 && !done; i++) {
      int indices[3] = { 0 };
      int index = 0;
      for (int j = 0; j < 4; j++) {
        if (i != j)
          indices[index++] = j;
      }
      if (PointInTriangle(
              vertices_[i].x(), vertices_[i].y(),
              vertices_[indices[0]].x(), vertices_[indices[0]].y(),
              vertices_[indices[1]].x(), vertices_[indices[1]].y(),
              vertices_[indices[2]].x(), vertices_[indices[2]].y())) {
        // Produce three triangles surrounding this interior vertex.
        for (int j = 0; j < 3; j++) {
          AddTriangle(&vertices_[indices[j % 3]],
                      &vertices_[indices[(j + 1) % 3]],
                      &vertices_[i]);
        }
        // Mark the interior vertex so we ignore it if trying to trace
        // the interior edge.
        vertices_[i].set_interior(true);
        done = true;
      }
    }
  }

  if (!done) {
    // There are only a few permutations of the vertices, ignoring
    // rotations, which are irrelevant:
    //
    //  0--3  0--2  0--3  0--1  0--2  0--1
    //  |  |  |  |  |  |  |  |  |  |  |  |
    //  |  |  |  |  |  |  |  |  |  |  |  |
    //  1--2  1--3  2--1  2--3  3--1  3--2
    //
    // Note that three of these are reflections of each other.
    // Therefore there are only three possible triangulations:
    //
    //  0--3  0--2  0--3
    //  |\ |  |\ |  |\ |
    //  | \|  | \|  | \|
    //  1--2  1--3  2--1
    //
    // From which we can choose by seeing which of the potential
    // diagonals intersect. Note that we choose the shortest diagonal
    // to split the quad.
    if (LinesIntersect(vertices_[0].x(), vertices_[0].y(),
                       vertices_[2].x(), vertices_[2].y(),
                       vertices_[1].x(), vertices_[1].y(),
                       vertices_[3].x(), vertices_[3].y())) {
      if (Distance(vertices_[0].x(), vertices_[0].y(),
                   vertices_[2].x(), vertices_[2].y()) <
          Distance(vertices_[1].x(), vertices_[1].y(),
                   vertices_[3].x(), vertices_[3].y())) {
        AddTriangle(&vertices_[0], &vertices_[1], &vertices_[2]);
        AddTriangle(&vertices_[0], &vertices_[2], &vertices_[3]);
      } else {
        AddTriangle(&vertices_[0], &vertices_[1], &vertices_[3]);
        AddTriangle(&vertices_[1], &vertices_[2], &vertices_[3]);
      }
    } else if (LinesIntersect(
        vertices_[0].x(), vertices_[0].y(),
        vertices_[3].x(), vertices_[3].y(),
        vertices_[1].x(), vertices_[1].y(),
        vertices_[2].x(), vertices_[2].y())) {
      if (Distance(vertices_[0].x(), vertices_[0].y(),
                   vertices_[3].x(), vertices_[3].y()) <
          Distance(vertices_[1].x(), vertices_[1].y(),
                   vertices_[2].x(), vertices_[2].y())) {
        AddTriangle(&vertices_[0], &vertices_[1], &vertices_[3]);
        AddTriangle(&vertices_[0], &vertices_[3], &vertices_[2]);
      } else {
        AddTriangle(&vertices_[0], &vertices_[1], &vertices_[2]);
        AddTriangle(&vertices_[2], &vertices_[1], &vertices_[3]);
      }
    } else {
      // Lines (0->1), (2->3) intersect -- or should, modulo numerical
      // precision issues
      if (Distance(vertices_[0].x(), vertices_[0].y(),
                   vertices_[1].x(), vertices_[1].y()) <
          Distance(vertices_[2].x(), vertices_[2].y(),
                   vertices_[3].x(), vertices_[3].y())) {
        AddTriangle(&vertices_[0], &vertices_[2], &vertices_[1]);
        AddTriangle(&vertices_[0], &vertices_[1], &vertices_[3]);
      } else {
        AddTriangle(&vertices_[0], &vertices_[2], &vertices_[3]);
        AddTriangle(&vertices_[3], &vertices_[2], &vertices_[1]);
      }
    }
  }

  if (compute_interior_vertices) {
    // We need to compute which vertices describe the path along the
    // interior portion of the shape, to feed these vertices to the
    // more general tessellation algorithm. It is possible that we
    // could determine this directly while producing triangles above.
    // Here we try to do it generally just by examining the triangles
    // that have already been produced. We walk around them in a
    // specific direction determined by which side of the curve is
    // being filled. We ignore the interior vertex unless it is also
    // the ending vertex, and skip the edges shared between two
    // triangles.
    Vertex* v = &vertices_[0];
    AddInteriorVertex(v);
    int num_steps = 0;
    while (!v->end() && num_steps < 4) {
      // Find the next vertex according to the above rules
      bool got_next = false;
      for (int i = 0; i < num_triangles() && !got_next; i++) {
        Triangle* tri = get_triangle(i);
        if (tri->Contains(v)) {
          Vertex* next = tri->NextVertex(v, fill_right_side);
          if (!next->marked() &&
              !IsSharedEdge(v, next) &&
              (!next->interior() || next->end())) {
            AddInteriorVertex(next);
            v = next;
            // Break out of for loop
            got_next = true;
          }
        }
      }
      ++num_steps;
    }
    if (!v->end()) {
      // Something went wrong with the above algorithm; add the last
      // vertex to the interior vertices anyway.
      AddInteriorVertex(&vertices_[3]);
    }
  }
}

void LocalTriangulator::AddTriangle(Vertex* v0, Vertex* v1, Vertex* v2) {
  DCHECK(num_triangles_ < 3);
  triangles_[num_triangles_++].SetVertices(v0, v1, v2);
}

void LocalTriangulator::AddInteriorVertex(Vertex* v) {
  DCHECK(num_interior_vertices_ < 4);
  interior_vertices_[num_interior_vertices_++] = v;
  v->set_marked(true);
}

bool LocalTriangulator::IsSharedEdge(Vertex* v0, Vertex* v1) {
  bool haveEdge01 = false;
  bool haveEdge10 = false;
  for (int i = 0; i < num_triangles(); i++) {
    Triangle* tri = get_triangle(i);
    if (tri->Contains(v0) && tri->NextVertex(v0, true) == v1)
      haveEdge01 = true;
    if (tri->Contains(v1) && tri->NextVertex(v1, true) == v0)
      haveEdge10 = true;
  }
  return haveEdge01 && haveEdge10;
}

}  // namespace gpu2d
}  // namespace o3d

