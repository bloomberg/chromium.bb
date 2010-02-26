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

#include "core/cross/gpu2d/path_cache.h"

namespace o3d {
namespace gpu2d {

unsigned int PathCache::num_vertices() const {
  return vertices_.size() / 2;
}

const float* PathCache::vertices() const {
  if (num_vertices() == 0)
    return NULL;
  return &vertices_.front();
}

const float* PathCache::texcoords() const {
  if (num_vertices() == 0)
    return NULL;
  return &texcoords_.front();
}

void PathCache::AddVertex(float x, float y,
                          float k, float l, float m) {
  vertices_.push_back(x);
  vertices_.push_back(y);
  texcoords_.push_back(k);
  texcoords_.push_back(l);
  texcoords_.push_back(m);
}

void PathCache::Clear() {
  vertices_.clear();
  texcoords_.clear();
  interior_vertices_.clear();
#ifdef O3D_CORE_CROSS_GPU2D_PATH_CACHE_DEBUG_INTERIOR_EDGES
  interior_edge_vertices_.clear();
#endif  // O3D_CORE_CROSS_GPU2D_PATH_CACHE_DEBUG_INTERIOR_EDGES
}

unsigned int PathCache::num_interior_vertices() const {
  return interior_vertices_.size() / 2;
}

const float* PathCache::interior_vertices() const {
  if (num_interior_vertices() == 0)
    return NULL;
  return &interior_vertices_.front();
}

void PathCache::AddInteriorVertex(float x, float y) {
  interior_vertices_.push_back(x);
  interior_vertices_.push_back(y);
}

#ifdef O3D_CORE_CROSS_GPU2D_PATH_CACHE_DEBUG_INTERIOR_EDGES
unsigned int PathCache::num_interior_edge_vertices() const {
  return interior_edge_vertices_.size() / 2;
}

const float* PathCache::interior_edge_vertices() const {
  if (num_interior_edge_vertices() == 0)
    return NULL;
  return &interior_edge_vertices_.front();
}

void PathCache::AddInteriorEdgeVertex(float x, float y) {
  interior_edge_vertices_.push_back(x);
  interior_edge_vertices_.push_back(y);
}
#endif  // O3D_CORE_CROSS_GPU2D_PATH_CACHE_DEBUG_INTERIOR_EDGES

}  // namespace gpu2d
}  // namespace o3d

