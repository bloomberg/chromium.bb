// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ChildFrameDisconnector_h
#define ChildFrameDisconnector_h

#include "platform/heap/Handle.h"

namespace blink {

class ElementShadow;
class HTMLFrameOwnerElement;
class Node;

class ChildFrameDisconnector {
  STACK_ALLOCATED();

 public:
  enum DisconnectPolicy { kRootAndDescendants, kDescendantsOnly };

  explicit ChildFrameDisconnector(Node& root) : root_(root) {}

  void Disconnect(DisconnectPolicy = kRootAndDescendants);

 private:
  void CollectFrameOwners(Node&);
  void CollectFrameOwners(ElementShadow&);
  void DisconnectCollectedFrameOwners();
  Node& Root() const { return *root_; }

  HeapVector<Member<HTMLFrameOwnerElement>, 10> frame_owners_;
  Member<Node> root_;
};

}  // namespace blink

#endif  // ChildFrameDisconnector_h
