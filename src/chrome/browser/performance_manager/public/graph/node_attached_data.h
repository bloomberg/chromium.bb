// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_ATTACHED_DATA_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_ATTACHED_DATA_H_

#include <memory>

#include "base/macros.h"

namespace performance_manager {

// NodeAttachedData allows external observers of the graph to store data that is
// associated with a graph node, providing lifetime management as a service.
//
// External (to performance_manager) implementations of NodeAttachedData should
// derive from ExternalNodeAttachedDataImpl. For internal implementations refer
// to NodeAttachedDataImpl and see node_attached_data_impl.h.
class NodeAttachedData {
 public:
  NodeAttachedData();
  virtual ~NodeAttachedData();

  // Returns the 'key' associated with this type of NodeAttachedData. This needs
  // to be unique per data type or bad things happen.
  virtual const void* GetKey() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NodeAttachedData);
};

// Implements NodeAttachedData for a given UserDataType.
//
// In order for a UserDataType to be attached to a node of type |NodeType| it
// must have a constructor of the form "UserDataType(const NodeType* node)".
template <typename UserDataType>
class ExternalNodeAttachedDataImpl : public NodeAttachedData {
 public:
  ExternalNodeAttachedDataImpl() = default;
  ~ExternalNodeAttachedDataImpl() override = default;

  // NodeAttachedData implementation:
  const void* GetKey() const override { return &kUserDataKey; }

  // Gets the user data for the given |node|, creating it if it doesn't yet
  // exist.
  template <typename NodeType>
  static UserDataType* GetOrCreate(const NodeType* node);

  // Gets the user data for the given |node|, returning nullptr if it doesn't
  // exist.
  template <typename NodeType>
  static UserDataType* Get(const NodeType* node);

  // Destroys the user data associated with the given node, returning true
  // on success or false if the user data did not exist to begin with.
  template <typename NodeType>
  static bool Destroy(const NodeType* node);

 private:
  static constexpr int kUserDataKey = 0;
  static const void* UserDataKey() { return &kUserDataKey; }

  DISALLOW_COPY_AND_ASSIGN(ExternalNodeAttachedDataImpl);
};

}  // namespace performance_manager

// This drags in the implementation of ExternalNodeAttachedDataImpl.
#include "chrome/browser/performance_manager/graph/node_attached_data.h"

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_ATTACHED_DATA_H_
