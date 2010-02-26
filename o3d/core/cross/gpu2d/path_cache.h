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

#ifndef O3D_CORE_CROSS_GPU2D_PATH_CACHE_H_
#define O3D_CORE_CROSS_GPU2D_PATH_CACHE_H_

#include <vector>

#include "base/basictypes.h"

namespace o3d {
namespace gpu2d {

// A cache of the processed triangle mesh for a given path. Because
// these might be expensive to allocate (using malloc/free
// internally), it is recommended to try to reuse them when possible.

// Uncomment the following to obtain debugging information for the edges facing
// the interior region of the mesh.
// #define O3D_CORE_CROSS_GPU2D_PATH_CACHE_DEBUG_INTERIOR_EDGES

class PathCache {
 public:
  PathCache() {
  }

  // The number of vertices in the mesh.
  unsigned int num_vertices() const;

  // Get the base pointer to the vertex information. There are two
  // coordinates per vertex. This pointer is valid until the cache is
  // cleared or another vertex is added. Returns NULL if there are no
  // vertices in the mesh.
  const float* vertices() const;

  // Get the base pointer to the texture coordinate information. There
  // are three coordinates per vertex. This pointer is valid until the
  // cache is cleared or another vertex is added. Returns NULL if
  // there are no vertices in the mesh.
  const float* texcoords() const;

  // Adds a vertex's information to the cache. The first two arguments
  // are the x and y coordinates of the vertex on the plane; the last
  // three arguments are the cubic texture coordinates associated with
  // this vertex.
  void AddVertex(float x, float y,
                 float k, float l, float m);

  // The number of interior vertices.
  unsigned int num_interior_vertices() const;
  // Base pointer to the interior vertices; two coordinates per
  // vertex, which can be drawn as GL_TRIANGLES. Returns NULL if there
  // are no interior vertices in the mesh.
  const float* interior_vertices() const;
  // Adds an interior vertex to the cache.
  void AddInteriorVertex(float x, float y);

  // Clears all of the stored vertex information in this cache.
  void Clear();

#ifdef O3D_CORE_CROSS_GPU2D_PATH_CACHE_DEBUG_INTERIOR_EDGES
  // The number of interior edge vertices
  unsigned int num_interior_edge_vertices() const;
  // Base pointer to the interior vertices; two coordinates per
  // vertex, which can be drawn as GL_LINES. Returns NULL if there are
  // no interior edge vertices in the mesh.
  const float* interior_edge_vertices() const;
  void AddInteriorEdgeVertex(float x, float y);
#endif  // O3D_CORE_CROSS_GPU2D_PATH_CACHE_DEBUG_INTERIOR_EDGES

 private:
  // The two-dimensional vertices of the triangle mesh.
  std::vector<float> vertices_;

  // The three-dimensional cubic texture coordinates.
  std::vector<float> texcoords_;

  std::vector<float> interior_vertices_;

#ifdef O3D_CORE_CROSS_GPU2D_PATH_CACHE_DEBUG_INTERIOR_EDGES
  // The following is only for debugging
  std::vector<float> interior_edge_vertices_;
#endif  // O3D_CORE_CROSS_GPU2D_PATH_CACHE_DEBUG_INTERIOR_EDGES

  DISALLOW_COPY_AND_ASSIGN(PathCache);
};

}  // namespace gpu2d
}  // namespace o3d

#endif  // O3D_CORE_CROSS_GPU2D_PATH_CACHE_H_

