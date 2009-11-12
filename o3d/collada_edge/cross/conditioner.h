/*
 * Copyright 2009, Google Inc.
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
#ifndef O3D_COLLADA_EDGE_CONDITIONER_H_
#define O3D_COLLADA_EDGE_CONDITIONER_H_

#include <vector>
#include "utility.h"

struct Options {
  // default constructor.
  Options()
    : enable_soften_edge(false),
      soften_edge_threshold(0),
      sharp_edge_color(1, 0, 0) {}

  // Add edges with dihedral angle larger than soften_edge_threshold.
  bool enable_soften_edge;

  // The threshold of normal angle of edges, if angle of certain
  // edge is larger than this threshold, it will be added into
  // the output file.
  float soften_edge_threshold;

  // Color of extra edges.
  Vector3 sharp_edge_color;
};

// This takes ownership of its children NodeInstances.
class NodeInstance {
 public:
  typedef std::vector<NodeInstance *> NodeInstanceList;

  explicit NodeInstance(FCDSceneNode *node) : node_(node) {}
  ~NodeInstance() {
    for (unsigned int i = 0; i < children_.size(); ++i) {
      delete children_[i];
    }
  }

  // Gets the Collada node associated with this node instance.
  FCDSceneNode *node() const { return node_; }

  // Gets the list of this node instance's children.
  NodeInstanceList &children() { return children_; }

  // Finds the NodeInstance representing a scene node in the direct
  // children of this NodeInstance.
  NodeInstance *FindNodeShallow(FCDSceneNode *node) {
    for (unsigned int i = 0; i < children_.size(); ++i) {
      NodeInstance *child = children_[i];
      if (child->node() == node) return child;
    }
    return NULL;
  }

  // Finds the NodeInstance representing a scene node in the sub-tree
  // starting at this NodeInstance.
  NodeInstance *FindNodeInTree(FCDSceneNode *node);

 private:
  FCDSceneNode *node_;
  std::vector<NodeInstance *> children_;
};

// Edge class is used in detect sharp edge process. It stores two points
// in a specific order, and it serves as key class in edge-triangle map.
struct Edge {
  Edge(Point3 p1, Point3 p2, uint32 i1, uint32 i2);
  std::vector<Point3> pts;
  std::vector<uint32> indices;
};

// Triangle class is used in detect sharp edge process.
struct Triangle {
  Triangle(Point3 p1, Point3 p2, Point3 p3,
           uint32 i1, uint32 i2, uint32 i3) {
    pts.push_back(p1);
    pts.push_back(p2);
    pts.push_back(p3);
    indices.push_back(i1);
    indices.push_back(i2);
    indices.push_back(i3);
  }
  std::vector<Point3> pts;
  std::vector<uint32> indices;
};

// Conditions the given file to emphasize the sharp edges.
// Returns false on failure, and writes the relevant erros to stderr.
bool Condition(const wchar_t* in_filename, const wchar_t* out_filename,
               const Options& options);

#endif