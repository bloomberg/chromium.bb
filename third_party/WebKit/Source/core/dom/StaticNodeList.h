/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StaticNodeList_h
#define StaticNodeList_h

#include "core/dom/NodeList.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/wtf/Vector.h"

namespace blink {

class Node;

template <typename NodeType>
class StaticNodeTypeList final : public NodeList {
 public:
  static StaticNodeTypeList* Adopt(HeapVector<Member<NodeType>>& nodes);

  static StaticNodeTypeList* CreateEmpty() { return new StaticNodeTypeList; }

  ~StaticNodeTypeList() override;

  unsigned length() const override;
  NodeType* item(unsigned index) const override;

  virtual void Trace(blink::Visitor*);
  virtual void TraceWrappers(const ScriptWrappableVisitor* visitor) const {
    for (unsigned i = 0; i < length(); i++)
      visitor->TraceWrappers(nodes_[i]);
    NodeList::TraceWrappers(visitor);
  }

 private:
  HeapVector<TraceWrapperMember<NodeType>> nodes_;
};

using StaticNodeList = StaticNodeTypeList<Node>;

template <typename NodeType>
StaticNodeTypeList<NodeType>* StaticNodeTypeList<NodeType>::Adopt(
    HeapVector<Member<NodeType>>& nodes) {
  StaticNodeTypeList<NodeType>* node_list = new StaticNodeTypeList<NodeType>;
  swap(node_list->nodes_, nodes);
  return node_list;
}

template <typename NodeType>
StaticNodeTypeList<NodeType>::~StaticNodeTypeList() {}

template <typename NodeType>
unsigned StaticNodeTypeList<NodeType>::length() const {
  return nodes_.size();
}

template <typename NodeType>
NodeType* StaticNodeTypeList<NodeType>::item(unsigned index) const {
  if (index < nodes_.size())
    return nodes_[index].Get();
  return 0;
}

template <typename NodeType>
void StaticNodeTypeList<NodeType>::Trace(Visitor* visitor) {
  visitor->Trace(nodes_);
  NodeList::Trace(visitor);
}

}  // namespace blink

#endif  // StaticNodeList_h
