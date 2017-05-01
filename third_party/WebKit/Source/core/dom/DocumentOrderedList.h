/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef DocumentOrderedList_h
#define DocumentOrderedList_h

#include "platform/heap/Handle.h"
#include "platform/wtf/ListHashSet.h"

namespace blink {

class Node;

class DocumentOrderedList final {
  WTF_MAKE_NONCOPYABLE(DocumentOrderedList);
  DISALLOW_NEW();

 public:
  DocumentOrderedList() {}

  void Add(Node*);
  void Remove(const Node*);
  bool IsEmpty() const { return nodes_.IsEmpty(); }
  void Clear() { nodes_.clear(); }
  size_t size() const { return nodes_.size(); }

  using iterator = HeapListHashSet<Member<Node>, 32>::iterator;
  using const_iterator = HeapListHashSet<Member<Node>, 32>::const_iterator;
  using const_reverse_iterator =
      HeapListHashSet<Member<Node>, 32>::const_reverse_iterator;

  iterator begin() { return nodes_.begin(); }
  iterator end() { return nodes_.end(); }
  const_iterator begin() const { return nodes_.begin(); }
  const_iterator end() const { return nodes_.end(); }

  const_reverse_iterator rbegin() const { return nodes_.rbegin(); }
  const_reverse_iterator rend() const { return nodes_.rend(); }

  DECLARE_TRACE();

 private:
  HeapListHashSet<Member<Node>, 32> nodes_;
};

}  // namespace blink

#endif
