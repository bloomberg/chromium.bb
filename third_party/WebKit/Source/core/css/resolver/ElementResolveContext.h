/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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

#ifndef ElementResolveContext_h
#define ElementResolveContext_h

#include "core/dom/Element.h"
#include "core/style/ComputedStyleConstants.h"

namespace blink {

class ContainerNode;
class Document;
class Element;
class ComputedStyle;

// ElementResolveContext is immutable and serves as an input to the style
// resolve process.
class CORE_EXPORT ElementResolveContext {
  STACK_ALLOCATED();

 public:
  explicit ElementResolveContext(const Document&);

  explicit ElementResolveContext(Element&);

  Element* GetElement() const { return element_; }
  const ContainerNode* ParentNode() const { return parent_node_; }
  const ContainerNode* LayoutParent() const { return layout_parent_; }
  const ComputedStyle* RootElementStyle() const { return root_element_style_; }
  const ComputedStyle* ParentStyle() const {
    return ParentNode() && ParentNode()->IsElementNode()
               ? ParentNode()->GetComputedStyle()
               : nullptr;
  }
  const ComputedStyle* LayoutParentStyle() const {
    return LayoutParent() ? LayoutParent()->GetComputedStyle() : nullptr;
  }
  EInsideLink ElementLinkState() const { return element_link_state_; }
  bool DistributedToV0InsertionPoint() const {
    return distributed_to_insertion_point_;
  }

 private:
  Member<Element> element_;
  Member<ContainerNode> parent_node_;
  Member<ContainerNode> layout_parent_;
  const ComputedStyle* root_element_style_;
  EInsideLink element_link_state_;
  bool distributed_to_insertion_point_;
};

}  // namespace blink

#endif  // ElementResolveContext_h
