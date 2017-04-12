// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DepthOrderedLayoutObjectList_h
#define DepthOrderedLayoutObjectList_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Vector.h"

namespace blink {

class LayoutObject;

// Put data inside a forward-declared struct, to avoid including LayoutObject.h.
struct DepthOrderedLayoutObjectListData;

class DepthOrderedLayoutObjectList {
 public:
  DepthOrderedLayoutObjectList();
  ~DepthOrderedLayoutObjectList();

  void Add(LayoutObject&);
  void Remove(LayoutObject&);
  void Clear();

  int size() const;
  bool IsEmpty() const;

  struct LayoutObjectWithDepth {
    LayoutObjectWithDepth(LayoutObject* in_object)
        : object(in_object), depth(DetermineDepth(in_object)) {}

    LayoutObjectWithDepth() : object(nullptr), depth(0) {}

    LayoutObject* object;
    unsigned depth;

    LayoutObject& operator*() const { return *object; }
    LayoutObject* operator->() const { return object; }

    bool operator<(const DepthOrderedLayoutObjectList::LayoutObjectWithDepth&
                       other) const {
      return depth > other.depth;
    }

   private:
    static unsigned DetermineDepth(LayoutObject*);
  };

  const HashSet<LayoutObject*>& Unordered() const;
  const Vector<LayoutObjectWithDepth>& Ordered();

 private:
  DepthOrderedLayoutObjectListData* data_;
};

}  // namespace blink

#endif  // DepthOrderedLayoutObjectList_h
