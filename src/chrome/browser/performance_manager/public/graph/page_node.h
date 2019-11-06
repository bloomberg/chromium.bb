// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_

#include "base/macros.h"

namespace performance_manager {

class Graph;

// A PageNode represents the root of a FrameTree, or equivalently a WebContents.
// These may correspond to normal tabs, WebViews, Portals, Chrome Apps or
// Extensions.
class PageNode {
 public:
  PageNode();
  virtual ~PageNode();

  // Returns the graph to which this node belongs.
  virtual Graph* GetGraph() const = 0;

  // Returns the private key which is used for indexing this object in the
  // graph. This is an opaque pointer strictly used for implementation.
  virtual const void* GetIndexingKey() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageNode);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_
