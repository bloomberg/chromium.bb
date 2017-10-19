/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef NodeIteratorBase_h
#define NodeIteratorBase_h

#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class Node;
class V8NodeFilterCondition;

class NodeIteratorBase : public GarbageCollectedMixin {
 public:
  Node* root() const { return root_.Get(); }
  unsigned whatToShow() const { return what_to_show_; }
  V8NodeFilterCondition* filter() const { return filter_.Get(); }

  virtual void Trace(blink::Visitor*);
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 protected:
  NodeIteratorBase(Node*, unsigned what_to_show, V8NodeFilterCondition*);
  unsigned AcceptNode(Node*, ExceptionState&) const;

 private:
  Member<Node> root_;
  unsigned what_to_show_;
  TraceWrapperMember<V8NodeFilterCondition> filter_;
};

}  // namespace blink

#endif  // NodeIteratorBase_h
