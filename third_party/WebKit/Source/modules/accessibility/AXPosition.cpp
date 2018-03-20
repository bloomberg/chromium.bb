// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/AXPosition.h"

#include "core/dom/AXObjectCache.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/Position.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/iterators/TextIterator.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

// static
const AXPosition AXPosition::CreatePositionBeforeObject(const AXObject& child) {
  // If |child| is a text object, make behavior the same as
  // |CreateFirstPositionInObject| so that equality would hold.
  if (child.GetNode() && child.GetNode()->IsTextNode())
    return CreateFirstPositionInContainerObject(child);

  const AXObject* parent = child.ParentObjectUnignored();
  DCHECK(parent);
  AXPosition position(*parent);
  position.text_offset_or_child_index_ = child.IndexInParent();
  DCHECK(position.IsValid());
  return position;
}

// static
const AXPosition AXPosition::CreatePositionAfterObject(const AXObject& child) {
  // If |child| is a text object, make behavior the same as
  // |CreateLastPositionInObject| so that equality would hold.
  if (child.GetNode() && child.GetNode()->IsTextNode())
    return CreateLastPositionInContainerObject(child);

  const AXObject* parent = child.ParentObjectUnignored();
  DCHECK(parent);
  AXPosition position(*parent);
  position.text_offset_or_child_index_ = child.IndexInParent() + 1;
  DCHECK(position.IsValid());
  return position;
}

// static
const AXPosition AXPosition::CreateFirstPositionInContainerObject(
    const AXObject& container) {
  if (container.GetNode() && container.GetNode()->IsTextNode()) {
    AXPosition position(container);
    position.text_offset_or_child_index_ = 0;
    DCHECK(position.IsValid());
    return position;
  }
  AXPosition position(container);
  position.text_offset_or_child_index_ = 0;
  DCHECK(position.IsValid());
  return position;
}

// static
const AXPosition AXPosition::CreateLastPositionInContainerObject(
    const AXObject& container) {
  if (container.GetNode() && container.GetNode()->IsTextNode()) {
    AXPosition position(container);
    const auto first_position =
        Position::FirstPositionInNode(*container.GetNode());
    const auto last_position =
        Position::LastPositionInNode(*container.GetNode());
    position.text_offset_or_child_index_ =
        TextIterator::RangeLength(first_position, last_position);
    DCHECK(position.IsValid());
    return position;
  }
  AXPosition position(container);
  position.text_offset_or_child_index_ =
      static_cast<int>(container.Children().size());
  DCHECK(position.IsValid());
  return position;
}

// static
const AXPosition AXPosition::CreatePositionInTextObject(
    const AXObject& container,
    int offset,
    TextAffinity affinity) {
  DCHECK(container.GetNode() && container.GetNode()->IsTextNode())
      << "Text positions should be anchored to a text node.";
  AXPosition position(container);
  position.text_offset_or_child_index_ = offset;
  position.affinity_ = affinity;
  DCHECK(position.IsValid());
  return position;
}

// static
const AXPosition AXPosition::FromPosition(const Position& position) {
  if (position.IsNull() || position.IsOrphan())
    return {};

  const Document* document = position.GetDocument();
  // Non orphan positions always have a document.
  DCHECK(document);

  AXObjectCache* ax_object_cache = document->ExistingAXObjectCache();
  if (!ax_object_cache)
    return {};

  auto* ax_object_cache_impl = static_cast<AXObjectCacheImpl*>(ax_object_cache);
  const Position& parent_anchored_position = position.ToOffsetInAnchor();
  const Node* anchor_node = parent_anchored_position.AnchorNode();
  DCHECK(anchor_node);
  const AXObject* container = ax_object_cache_impl->GetOrCreate(anchor_node);
  DCHECK(container);
  AXPosition ax_position(*container);
  if (anchor_node->IsTextNode()) {
    // Convert from a DOM offset that may have uncompressed white space to a
    // character offset.
    // TODO(ax-dev): Use LayoutNG offset mapping instead of |TextIterator|.
    const auto first_position = Position::FirstPositionInNode(*anchor_node);
    int offset =
        TextIterator::RangeLength(first_position, parent_anchored_position);
    ax_position.text_offset_or_child_index_ = offset;
    DCHECK(ax_position.IsValid());
    return ax_position;
  }

  const Node* node_after_position = position.ComputeNodeAfterPosition();
  if (!node_after_position) {
    ax_position.text_offset_or_child_index_ =
        static_cast<int>(container->Children().size());
    DCHECK(ax_position.IsValid());
    return ax_position;
  }

  const AXObject* ax_child =
      ax_object_cache_impl->GetOrCreate(node_after_position);
  DCHECK(ax_child);
  if (ax_child->IsDescendantOf(*container)) {
    ax_position.text_offset_or_child_index_ = ax_child->IndexInParent();
    DCHECK(ax_position.IsValid());
    return ax_position;
  }
  return CreatePositionBeforeObject(*ax_child);
}

// Only for use by |AXSelection| to represent empty selection ranges.
AXPosition::AXPosition()
    : container_object_(nullptr),
      text_offset_or_child_index_(),
      affinity_(TextAffinity::kDownstream) {
#if DCHECK_IS_ON()
  dom_tree_version_ = 0;
  style_version_ = 0;
#endif
}

AXPosition::AXPosition(const AXObject& container)
    : container_object_(&container),
      text_offset_or_child_index_(),
      affinity_(TextAffinity::kDownstream) {
  const Document* document = container_object_->GetDocument();
  DCHECK(document);
#if DCHECK_IS_ON()
  dom_tree_version_ = document->DomTreeVersion();
  style_version_ = document->StyleVersion();
#endif
}

const AXObject* AXPosition::ObjectAfterPosition() const {
  if (IsTextPosition() || !IsValid())
    return nullptr;
  return *(container_object_->Children().begin() + ChildIndex());
}

int AXPosition::ChildIndex() const {
  if (!IsTextPosition())
    return *text_offset_or_child_index_;
  NOTREACHED() << *this << " should not be a text position.";
  return 0;
}

int AXPosition::TextOffset() const {
  if (IsTextPosition())
    return *text_offset_or_child_index_;
  NOTREACHED() << *this << " should be a text position.";
  return 0;
}

bool AXPosition::IsValid() const {
  if (!container_object_ || container_object_->IsDetached())
    return false;
  if (!container_object_->GetNode() ||
      !container_object_->GetNode()->isConnected())
    return false;

  // We can't have both an object and a text anchored position, but we must have
  // at least one of them.
  DCHECK(text_offset_or_child_index_);
  if (text_offset_or_child_index_ &&
      !container_object_->GetNode()->IsTextNode()) {
    if (text_offset_or_child_index_ >
        static_cast<int>(container_object_->Children().size()))
      return false;
  }

  DCHECK(container_object_->GetNode()->GetDocument().IsActive());
  DCHECK(!container_object_->GetNode()->GetDocument().NeedsLayoutTreeUpdate());
#if DCHECK_IS_ON()
  DCHECK_EQ(container_object_->GetNode()->GetDocument().DomTreeVersion(),
            dom_tree_version_);
  DCHECK_EQ(container_object_->GetNode()->GetDocument().StyleVersion(),
            style_version_);
#endif  // DCHECK_IS_ON()
  return true;
}

bool AXPosition::IsTextPosition() const {
  return IsValid() && container_object_->GetNode()->IsTextNode();
}

const PositionWithAffinity AXPosition::ToPositionWithAffinity() const {
  if (!IsValid())
    return {};

  const Node* container_node = container_object_->GetNode();
  if (!IsTextPosition()) {
    if (ChildIndex() ==
        static_cast<int>(container_object_->Children().size())) {
      return PositionWithAffinity(Position::LastPositionInNode(*container_node),
                                  affinity_);
    }
    const AXObject* ax_child =
        *(container_object_->Children().begin() + ChildIndex());
    return PositionWithAffinity(
        Position::InParentBeforeNode(*(ax_child->GetNode())), affinity_);
  }

  // TODO(ax-dev): Use LayoutNG offset mapping instead of |TextIterator|.
  const auto first_position = Position::FirstPositionInNode(*container_node);
  const auto last_position = Position::LastPositionInNode(*container_node);
  CharacterIterator character_iterator(first_position, last_position);
  const EphemeralRange range = character_iterator.CalculateCharacterSubrange(
      0, *text_offset_or_child_index_);
  return PositionWithAffinity(range.EndPosition(), affinity_);
}

bool operator==(const AXPosition& a, const AXPosition& b) {
  DCHECK(a.IsValid() && b.IsValid());
  if (*a.ContainerObject() != *b.ContainerObject())
    return false;
  if (a.IsTextPosition() && b.IsTextPosition())
    return a.TextOffset() == b.TextOffset() && a.Affinity() == b.Affinity();
  if (!a.IsTextPosition() && !b.IsTextPosition())
    return a.ChildIndex() == b.ChildIndex();
  NOTREACHED() << "AXPosition objects having the same container object should "
                  "have the same type.";
  return false;
}

bool operator!=(const AXPosition& a, const AXPosition& b) {
  return !(a == b);
}

bool operator<(const AXPosition& a, const AXPosition& b) {
  DCHECK(a.IsValid() && b.IsValid());
  if (*a.ContainerObject() > *b.ContainerObject())
    return false;
  if (*a.ContainerObject() < *b.ContainerObject())
    return true;
  if (a.IsTextPosition() && b.IsTextPosition())
    return a.TextOffset() < b.TextOffset();
  if (!a.IsTextPosition() && !b.IsTextPosition())
    return a.ChildIndex() < b.ChildIndex();
  NOTREACHED() << "AXPosition objects having the same container object should "
                  "have the same type.";
  return false;
}

bool operator<=(const AXPosition& a, const AXPosition& b) {
  return a < b || a == b;
}

bool operator>(const AXPosition& a, const AXPosition& b) {
  DCHECK(a.IsValid() && b.IsValid());
  if (*a.ContainerObject() < *b.ContainerObject())
    return false;
  if (*a.ContainerObject() > *b.ContainerObject())
    return true;
  if (a.IsTextPosition() && b.IsTextPosition())
    return a.TextOffset() > b.TextOffset();
  if (!a.IsTextPosition() && !b.IsTextPosition())
    return a.ChildIndex() > b.ChildIndex();
  NOTREACHED() << "AXPosition objects having the same container object should "
                  "have the same type.";
  return false;
}

bool operator>=(const AXPosition& a, const AXPosition& b) {
  return a > b || a == b;
}

std::ostream& operator<<(std::ostream& ostream, const AXPosition& position) {
  if (!position.IsValid())
    return ostream << "Invalid AXPosition";
  if (position.IsTextPosition()) {
    return ostream << "AX text position in " << position.ContainerObject()
                   << ", " << position.TextOffset();
  }
  return ostream << "AX object anchored position in "
                 << position.ContainerObject() << ", " << position.ChildIndex();
}

}  // namespace blink
