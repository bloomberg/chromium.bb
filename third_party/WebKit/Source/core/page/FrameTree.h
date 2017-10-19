/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
 */

#ifndef FrameTree_h
#define FrameTree_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class Frame;

class CORE_EXPORT FrameTree final {
  WTF_MAKE_NONCOPYABLE(FrameTree);
  DISALLOW_NEW();

 public:
  explicit FrameTree(Frame* this_frame);
  ~FrameTree();

  const AtomicString& GetName() const;

  enum ReplicationPolicy {
    // Does not propagate name changes beyond this FrameTree object.
    kDoNotReplicate,

    // Kicks-off propagation of name changes to other renderers.
    kReplicate,
  };
  void SetName(const AtomicString&, ReplicationPolicy = kDoNotReplicate);

  Frame* Parent() const;
  Frame& Top() const;
  Frame* NextSibling() const;
  Frame* FirstChild() const;

  bool IsDescendantOf(const Frame* ancestor) const;
  Frame* TraverseNext(const Frame* stay_within = nullptr) const;

  Frame* Find(const AtomicString& name) const;
  unsigned ChildCount() const;

  Frame* ScopedChild(unsigned index) const;
  Frame* ScopedChild(const AtomicString& name) const;
  unsigned ScopedChildCount() const;
  void InvalidateScopedChildCount();

  void Trace(blink::Visitor*);

 private:
  Member<Frame> this_frame_;

  AtomicString name_;  // The actual frame name (may be empty).

  mutable unsigned scoped_child_count_;
};

}  // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showFrameTree(const blink::Frame*);
#endif

#endif  // FrameTree_h
