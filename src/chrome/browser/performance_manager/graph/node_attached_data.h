// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"

namespace performance_manager {

class NodeBase;

// Abstract base class used for attaching user data to nodes in the graph.
// Implementations should derive from NodeAttachedDataImpl. See
// node_attached_data_impl.h for details.
class NodeAttachedData {
 public:
  NodeAttachedData();
  virtual ~NodeAttachedData();

  // Returns the 'key' associated with this type of NodeAttachedData. This needs
  // to be unique per data type or bad things happen.
  virtual const void* key() const = 0;

  // Returns true if the implementation is able to be attached to a node of the
  // given type.
  virtual bool CanAttach(
      resource_coordinator::CoordinationUnitType node_type) const = 0;
  bool CanAttach(const NodeBase* node) const;

 protected:
  friend class NodeAttachedDataTest;

  // For creating / retrieving / destroying data that is stored in a global map
  // owned by the graph. These are not intended to be used directly, rather
  // they are invoked by the typed helper functions provided via
  // NodeAttachedDataImpl.

  // Attaches the provided |data| to the provided |node|. This should only be
  // called if the data does not exist (GetFromMap returns nullptr), and will
  // DCHECK otherwise. This should also only be called if |data| can be attached
  // to the given node type (also enforced by a DCHECK).
  static void AttachInMap(const NodeBase* node,
                          std::unique_ptr<NodeAttachedData> data);

  // Retrieves the data associated with the provided |node| and |key|. This
  // returns nullptr if no data exists.
  static NodeAttachedData* GetFromMap(const NodeBase* node, const void* key);

  // Detaches the data associated with the provided |node| and |key|. It is
  // harmless to call this when no data exists.
  static std::unique_ptr<NodeAttachedData> DetachFromMap(const NodeBase* node,
                                                         const void* key);
};

// Helper class for providing internal storage of a NodeAttachedData
// implementation directly in a node. The storage is provided as a raw buffer of
// bytes which is initialized externally by the NodeAttachedDataImpl via a
// placement new. In this way the node only needs to know about the
// NodeAttachedData base class, and the size of the required storage.
template <size_t DataSize>
class InternalNodeAttachedDataStorage {
 public:
  static constexpr size_t kDataSize = DataSize;

  InternalNodeAttachedDataStorage() {}

  ~InternalNodeAttachedDataStorage() { Reset(); }

  // Returns a pointer to the data object, if allocated.
  NodeAttachedData* Get() { return data_; }

  void Reset() {
    if (data_)
      data_->~NodeAttachedData();
    data_ = nullptr;
  }

  uint8_t* buffer() { return buffer_; }

 protected:
  friend class InternalNodeAttachedDataStorageAccess;

  // Transitions this object to being allocated.
  void Set(NodeAttachedData* data) {
    DCHECK(!data_);
    // Depending on the object layout, once it has been cast to a
    // NodeAttachedData there's no guarantee that the pointer will be at the
    // head of the object, only that the pointer will be somewhere inside of the
    // full object extent.
    DCHECK_LE(buffer_, reinterpret_cast<uint8_t*>(data));
    DCHECK_GT(buffer_ + kDataSize, reinterpret_cast<uint8_t*>(data));
    data_ = data;
  }

 private:
  NodeAttachedData* data_ = nullptr;
  uint8_t buffer_[kDataSize];
  DISALLOW_COPY_AND_ASSIGN(InternalNodeAttachedDataStorage);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_H_
