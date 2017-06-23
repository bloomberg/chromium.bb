// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyNode_h
#define PaintPropertyNode_h

#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

template <typename NodeType>
class PaintPropertyNode : public RefCounted<NodeType> {
 public:
  // Parent property node, or nullptr if this is the root node.
  const NodeType* Parent() const { return parent_.Get(); }
  bool IsRoot() const { return !parent_; }

 protected:
  PaintPropertyNode(PassRefPtr<const NodeType> parent)
      : parent_(std::move(parent)) {}

  bool Update(PassRefPtr<const NodeType> parent) {
    DCHECK(!IsRoot());
    DCHECK(parent != this);
    if (parent == parent_)
      return false;

    parent_ = std::move(parent);
    return true;
  }

 private:
  RefPtr<const NodeType> parent_;
};

}  // namespace blink

#endif  // PaintPropertyNode_h
