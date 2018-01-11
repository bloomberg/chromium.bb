/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2007 Apple Inc. All rights reserved.
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

#include "core/dom/NameNodeList.h"

#include "core/dom/Element.h"
#include "core/dom/NodeRareData.h"
#include "platform/wtf/Assertions.h"

namespace blink {

using namespace HTMLNames;

NameNodeList::NameNodeList(ContainerNode& root_node, const AtomicString& name)
    : LiveNodeList(root_node, kNameNodeListType, kInvalidateOnNameAttrChange),
      name_(name) {}

NameNodeList::~NameNodeList() = default;

bool NameNodeList::ElementMatches(const Element& element) const {
  return element.GetNameAttribute() == name_;
}

}  // namespace blink
