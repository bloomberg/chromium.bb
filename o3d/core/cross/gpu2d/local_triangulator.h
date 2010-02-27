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

#ifndef O3D_CORE_CROSS_GPU2D_LOCAL_TRIANGULATOR_H_
#define O3D_CORE_CROSS_GPU2D_LOCAL_TRIANGULATOR_H_

#include "base/basictypes.h"
#include "base/logging.h"

namespace o3d {
namespace gpu2d {

// Performs a localized triangulation of the triangle mesh
// corresponding to the four control point vertices of a cubic curve
// segment.
class LocalTriangulator {
 public:
  // The vertices that the triangulator operates upon, containing both
  // the position information as well as the cubic texture
  // coordinates.
  class Vertex {
   public:
    Vertex() {
    }

    float x() { return x_; }
    float y() { return y_; }

    float k() { return k_; }
    float l() { return l_; }
    float m() { return m_; }

    // Sets the position and texture coordinates of the vertex.
    void Set(float x, float y,
             float k, float l, float m) {
      x_ = x;
      y_ = y;
      k_ = k;
      l_ = l;
      m_ = m;
    }

    // Flags for walking from the start vertex to the end vertex.
    bool end() {
      return end_;
    }

    void set_end(bool end) {
      end_ = end;
    }

    bool marked() {
      return marked_;
    }

    void set_marked(bool marked) {
      marked_ = marked;
    }

    bool interior() {
      return interior_;
    }

    void set_interior(bool interior) {
      interior_ = interior;
    }

    void ResetFlags() {
      end_ = false;
      marked_ = false;
      interior_ = false;
    }

   private:
    // 2D coordinates of the vertex in the plane.
    float x_;
    float y_;
    // Cubic texture coordinates for rendering the curve.
    float k_, l_, m_;

    // Flags for walking from the start vertex to the end vertex.
    bool end_;
    bool marked_;
    bool interior_;
    DISALLOW_COPY_AND_ASSIGN(Vertex);
  };

  // The triangles the Triangulator produces.
  class Triangle {
   public:
    Triangle() {
      v_[0] = NULL;
      v_[1] = NULL;
      v_[2] = NULL;
    }

    // Gets the vertex at the given index, 0 <= index < 3.
    Vertex* get_vertex(int index) {
      DCHECK(index >= 0 && index < 3);
      return v_[index];
    }

    // Returns true if this triangle contains the given vertex (by
    // identity, not geometrically).
    bool Contains(Vertex* v);

    // Returns the vertex following the current one in the specified
    // direction, counterclockwise or clockwise.
    Vertex* NextVertex(Vertex* cur, bool ccw);

    // Sets the vertices of this triangle, potentially reordering them
    // to produce a canonical orientation.
    void SetVertices(Vertex* v0,
                     Vertex* v1,
                     Vertex* v2) {
      v_[0] = v0;
      v_[1] = v1;
      v_[2] = v2;
      MakeCCW();
    }

   private:
    // Returns the index [0..2] associated with the given vertex, or
    // -1 if not found.
    int IndexForVertex(Vertex* vertex);

    // Reorders the vertices in this triangle to make them
    // counterclockwise when viewed in the 2D plane, in order to
    // achieve a canonical ordering.
    void MakeCCW();

    Vertex* v_[3];

    DISALLOW_COPY_AND_ASSIGN(Triangle);
  };

  LocalTriangulator();

  // Resets the triangulator's state. After each triangulation and
  // before the next, call this to re-initialize the internal
  // vertices' state.
  void Reset();

  // Returns a mutable vertex stored in the triangulator. Use this to
  // set up the vertices before a triangulation.
  Vertex* get_vertex(int index) {
    DCHECK(index >= 0 && index < 4);
    return &vertices_[index];
  }

  // Once the vertices' contents have been set up, call Triangulate()
  // to recompute the triangles.
  //
  // If compute_inside_edges is true, then fill_right_side will be
  // used to determine which side of the cubic curve defined by the
  // four control points is to be filled.
  //
  // The triangulation obeys the following guarantees:
  //   - If the convex hull is a quadrilateral, then the shortest edge
  //     will be chosen for the cut into two triangles.
  //   - If one of the vertices is contained in the triangle spanned
  //     by the other three, three triangles will be produced.
  void Triangulate(bool compute_inside_edges,
                   bool fill_right_side);

  // Number of triangles computed by Triangulate().
  int num_triangles() const {
    return num_triangles_;
  }

  // Returns the computed triangle at index, 0 <= index <
  // num_triangles().
  Triangle* get_triangle(int index) {
    DCHECK(index >= 0 && index < num_triangles_);
    return &triangles_[index];
  }

  // Number of vertices facing the inside of the shape, if
  // compute_inside_edges was true when Triangulate() was called.
  int num_interior_vertices() const {
    return num_interior_vertices_;
  }

  // Fetches the given interior vertex, 0 <= index <
  // num_interior_vertices(). Returns NULL if index is out of range.
  Vertex* get_interior_vertex(int index) {
    DCHECK(index >= 0 && index < num_interior_vertices_);
    return interior_vertices_[index];
  }

 private:
  // Adds a triangle to the triangulation.
  void AddTriangle(Vertex* v0, Vertex* v1, Vertex* v2);

  // Adds a vertex to the list of interior vertices.
  void AddInteriorVertex(Vertex* v);

  // Indicates whether the edge between vertex v0 and v1 is shared
  // between two or more triangles.
  bool IsSharedEdge(Vertex* v0, Vertex* v1);

  // The vertices being triangulated.
  Vertex vertices_[4];

  // The vertices corresponding to the edges facing the inside of the
  // shape, in order from the start vertex to the end vertex. The more
  // general triangulation algorithm tessellates this interior region.
  Vertex* interior_vertices_[4];
  // The number of interior vertices that are valid for the current
  // triangulation.
  int num_interior_vertices_;

  // There can be at most three triangles computed by this local
  // algorithm, which occurs when one of the vertices is contained in
  // the triangle spanned by the other three. Most of the time the
  // algorithm computes two triangles.
  Triangle triangles_[3];
  int num_triangles_;

  DISALLOW_COPY_AND_ASSIGN(LocalTriangulator);
};

}  // namespace gpu2d
}  // namespace o3d

#endif  // O3D_CORE_CROSS_GPU2D_LOCAL_TRIANGULATOR_H_

