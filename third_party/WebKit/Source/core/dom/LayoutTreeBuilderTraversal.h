/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#ifndef LayoutTreeBuilderTraversal_h
#define LayoutTreeBuilderTraversal_h

#include <cstdint>
#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "core/dom/InsertionPoint.h"

namespace blink {

class LayoutObject;

class CORE_EXPORT LayoutTreeBuilderTraversal {
 public:
  static const int32_t kTraverseAllSiblings = -2;
  class ParentDetails {
    STACK_ALLOCATED();

   public:
    ParentDetails() : insertion_point_(nullptr) {}

    const InsertionPoint* GetInsertionPoint() const { return insertion_point_; }

    void DidTraverseInsertionPoint(const InsertionPoint*);

    bool operator==(const ParentDetails& other) {
      return insertion_point_ == other.insertion_point_;
    }

   private:
    Member<const InsertionPoint> insertion_point_;
  };

  static ContainerNode* Parent(const Node&, ParentDetails* = nullptr);
  static ContainerNode* LayoutParent(const Node&, ParentDetails* = nullptr);
  static Node* FirstChild(const Node&);
  static Node* NextSibling(const Node&);
  static Node* NextLayoutSibling(const Node& node) {
    int32_t limit = kTraverseAllSiblings;
    return NextLayoutSibling(node, limit);
  }
  static Node* PreviousLayoutSibling(const Node& node) {
    int32_t limit = kTraverseAllSiblings;
    return PreviousLayoutSibling(node, limit);
  }
  static Node* PreviousSibling(const Node&);
  static Node* Previous(const Node&, const Node* stay_within);
  static Node* Next(const Node&, const Node* stay_within);
  static Node* NextSkippingChildren(const Node&, const Node* stay_within);
  static LayoutObject* ParentLayoutObject(const Node&);
  static LayoutObject* NextSiblingLayoutObject(
      const Node&,
      int32_t limit = kTraverseAllSiblings);
  static LayoutObject* PreviousSiblingLayoutObject(
      const Node&,
      int32_t limit = kTraverseAllSiblings);
  static LayoutObject* NextInTopLayer(const Element&);

  static inline Element* ParentElement(const Node& node) {
    ContainerNode* found = Parent(node);
    return found && found->IsElementNode() ? ToElement(found) : nullptr;
  }

 private:
  static Node* NextLayoutSibling(const Node&, int32_t& limit);
  static Node* PreviousLayoutSibling(const Node&, int32_t& limit);
};

}  // namespace blink

#endif
