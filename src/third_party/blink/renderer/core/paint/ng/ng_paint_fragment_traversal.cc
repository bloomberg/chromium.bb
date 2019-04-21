// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

namespace blink {

namespace {

// ------ Helpers for traversing inline fragments ------

bool IsLineBreak(const NGPaintFragmentTraversalContext& fragment) {
  DCHECK(!fragment.IsNull());
  const NGPhysicalFragment& physical_fragment =
      fragment.GetFragment()->PhysicalFragment();
  DCHECK(physical_fragment.IsInline());
  auto* physical_text_fragment =
      DynamicTo<NGPhysicalTextFragment>(physical_fragment);
  if (!physical_text_fragment)
    return false;
  return physical_text_fragment->IsLineBreak();
}

bool IsInlineLeaf(const NGPaintFragmentTraversalContext& fragment) {
  DCHECK(!fragment.IsNull());
  const NGPhysicalFragment& physical_fragment =
      fragment.GetFragment()->PhysicalFragment();
  if (!physical_fragment.IsInline())
    return false;
  return physical_fragment.IsText() || physical_fragment.IsAtomicInline();
}

NGPaintFragmentTraversalContext FirstInclusiveLeafDescendantOf(
    const NGPaintFragmentTraversalContext& fragment) {
  DCHECK(!fragment.IsNull());
  if (IsInlineLeaf(fragment))
    return fragment;
  const auto& children = fragment.GetFragment()->Children();
  for (unsigned i = 0; i < children.size(); ++i) {
    NGPaintFragmentTraversalContext maybe_leaf =
        FirstInclusiveLeafDescendantOf({fragment.GetFragment(), i});
    if (!maybe_leaf.IsNull())
      return maybe_leaf;
  }
  return NGPaintFragmentTraversalContext();
}

NGPaintFragmentTraversalContext LastInclusiveLeafDescendantOf(
    const NGPaintFragmentTraversalContext& fragment) {
  DCHECK(!fragment.IsNull());
  if (IsInlineLeaf(fragment))
    return fragment;
  const auto& children = fragment.GetFragment()->Children();
  for (unsigned i = children.size(); i != 0u; --i) {
    NGPaintFragmentTraversalContext maybe_leaf =
        LastInclusiveLeafDescendantOf({fragment.GetFragment(), i - 1});
    if (!maybe_leaf.IsNull())
      return maybe_leaf;
  }
  return NGPaintFragmentTraversalContext();
}

NGPaintFragmentTraversalContext PreviousSiblingOf(
    const NGPaintFragmentTraversalContext& fragment) {
  if (!fragment.parent || fragment.index == 0u)
    return NGPaintFragmentTraversalContext();
  return {fragment.parent, fragment.index - 1};
}

NGPaintFragmentTraversalContext NextSiblingOf(
    const NGPaintFragmentTraversalContext& fragment) {
  if (!fragment.parent)
    return NGPaintFragmentTraversalContext();
  if (fragment.index + 1 == fragment.parent->Children().size())
    return NGPaintFragmentTraversalContext();
  return {fragment.parent, fragment.index + 1};
}

unsigned IndexOf(const Vector<NGPaintFragment*, 16>& fragments,
                 const NGPaintFragment& fragment) {
  auto* const* it = std::find_if(
      fragments.begin(), fragments.end(),
      [&fragment](const auto& child) { return &fragment == child; });
  DCHECK(it != fragments.end());
  return static_cast<unsigned>(std::distance(fragments.begin(), it));
}

}  // namespace

NGPaintFragmentTraversal::NGPaintFragmentTraversal(const NGPaintFragment& root)
    : current_(root.FirstChild()), root_(root) {}

NGPaintFragmentTraversal::NGPaintFragmentTraversal(const NGPaintFragment& root,
                                                   const NGPaintFragment& start)
    : root_(root) {
  MoveTo(start);
}

void NGPaintFragmentTraversal::MoveTo(const NGPaintFragment& fragment) {
  DCHECK(fragment.IsDescendantOfNotSelf(root_));
  current_ = &fragment;
}

void NGPaintFragmentTraversal::MoveToNext() {
  if (IsAtEnd())
    return;

  if (const NGPaintFragment* first_child = current_->FirstChild()) {
    current_ = first_child;
    if (UNLIKELY(!siblings_.IsEmpty()))
      siblings_.Shrink(0);
    return;
  }

  MoveToNextSiblingOrAncestor();
}

void NGPaintFragmentTraversal::MoveToNextSiblingOrAncestor() {
  while (!IsAtEnd()) {
    // Check if we have a next sibling.
    if (const NGPaintFragment* next = current_->NextSibling()) {
      current_ = next;
      ++current_index_;
      return;
    }

    MoveToParent();
  }
}

void NGPaintFragmentTraversal::MoveToParent() {
  if (IsAtEnd())
    return;

  current_ = current_->Parent();
  if (current_ == &root_)
    current_ = nullptr;
  if (UNLIKELY(!siblings_.IsEmpty()))
    siblings_.Shrink(0);
}

void NGPaintFragmentTraversal::MoveToPrevious() {
  if (IsAtEnd())
    return;

  if (siblings_.IsEmpty()) {
    current_->Parent()->Children().ToList(&siblings_);
    current_index_ = IndexOf(siblings_, *current_);
  }

  if (!current_index_) {
    // There is no previous sibling of |current_|. We move to parent.
    MoveToParent();
    return;
  }

  current_ = siblings_[--current_index_];
  while (current_->FirstChild()) {
    current_->Children().ToList(&siblings_);
    DCHECK(!siblings_.IsEmpty());
    current_index_ = siblings_.size() - 1;
    current_ = siblings_[current_index_];
  }
}

NGPaintFragmentTraversal::AncestorRange
NGPaintFragmentTraversal::InclusiveAncestorsOf(const NGPaintFragment& start) {
  return AncestorRange(start);
}

NGPaintFragmentTraversal::InlineDescendantsRange
NGPaintFragmentTraversal::InlineDescendantsOf(
    const NGPaintFragment& container) {
  return InlineDescendantsRange(container);
}

NGPaintFragment* NGPaintFragmentTraversal::PreviousLineOf(
    const NGPaintFragment& line) {
  DCHECK(line.PhysicalFragment().IsLineBox());
  NGPaintFragment* parent = line.Parent();
  DCHECK(parent);
  NGPaintFragment* previous_line = nullptr;
  for (NGPaintFragment* sibling : parent->Children()) {
    if (sibling == &line)
      return previous_line;
    if (sibling->PhysicalFragment().IsLineBox())
      previous_line = sibling;
  }
  NOTREACHED();
  return nullptr;
}

const NGPaintFragment* NGPaintFragmentTraversalContext::GetFragment() const {
  if (!parent)
    return nullptr;
  return siblings[index];
}

NGPaintFragmentTraversalContext::NGPaintFragmentTraversalContext(
    const NGPaintFragment* fragment) {
  if (fragment) {
    parent = fragment->Parent();
    parent->Children().ToList(&siblings);
    index = IndexOf(siblings, *fragment);
  }
}

NGPaintFragmentTraversalContext::NGPaintFragmentTraversalContext(
    const NGPaintFragment* parent,
    unsigned index)
    : parent(parent), index(index) {
  DCHECK(parent);
  parent->Children().ToList(&siblings);
  DCHECK(index < siblings.size());
}

// static
NGPaintFragmentTraversalContext NGPaintFragmentTraversalContext::Create(
    const NGPaintFragment* fragment) {
  return fragment ? NGPaintFragmentTraversalContext(fragment)
                  : NGPaintFragmentTraversalContext();
}

NGPaintFragmentTraversalContext NGPaintFragmentTraversal::PreviousInlineLeafOf(
    const NGPaintFragmentTraversalContext& fragment) {
  DCHECK(!fragment.IsNull());
  DCHECK(fragment.GetFragment()->PhysicalFragment().IsInline());
  for (auto sibling = PreviousSiblingOf(fragment); !sibling.IsNull();
       sibling = PreviousSiblingOf(sibling)) {
    NGPaintFragmentTraversalContext maybe_leaf =
        LastInclusiveLeafDescendantOf(sibling);
    if (!maybe_leaf.IsNull())
      return maybe_leaf;
  }
  DCHECK(fragment.parent);
  if (fragment.parent->PhysicalFragment().IsLineBox())
    return NGPaintFragmentTraversalContext();
  return PreviousInlineLeafOf(
      NGPaintFragmentTraversalContext::Create(fragment.parent));
}

NGPaintFragmentTraversalContext NGPaintFragmentTraversal::NextInlineLeafOf(
    const NGPaintFragmentTraversalContext& fragment) {
  DCHECK(!fragment.IsNull());
  DCHECK(fragment.GetFragment()->PhysicalFragment().IsInline());
  for (auto sibling = NextSiblingOf(fragment); !sibling.IsNull();
       sibling = NextSiblingOf(sibling)) {
    NGPaintFragmentTraversalContext maybe_leaf =
        FirstInclusiveLeafDescendantOf(sibling);
    if (!maybe_leaf.IsNull())
      return maybe_leaf;
  }
  DCHECK(fragment.parent);
  if (fragment.parent->PhysicalFragment().IsLineBox())
    return NGPaintFragmentTraversalContext();
  return NextInlineLeafOf(
      NGPaintFragmentTraversalContext::Create(fragment.parent));
}

NGPaintFragmentTraversalContext
NGPaintFragmentTraversal::PreviousInlineLeafOfIgnoringLineBreak(
    const NGPaintFragmentTraversalContext& fragment) {
  NGPaintFragmentTraversalContext runner = PreviousInlineLeafOf(fragment);
  while (!runner.IsNull() && IsLineBreak(runner))
    runner = PreviousInlineLeafOf(runner);
  return runner;
}

NGPaintFragmentTraversalContext
NGPaintFragmentTraversal::NextInlineLeafOfIgnoringLineBreak(
    const NGPaintFragmentTraversalContext& fragment) {
  NGPaintFragmentTraversalContext runner = NextInlineLeafOf(fragment);
  while (!runner.IsNull() && IsLineBreak(runner))
    runner = NextInlineLeafOf(runner);
  return runner;
}

// ----
NGPaintFragment* NGPaintFragmentTraversal::AncestorRange::Iterator::operator->()
    const {
  DCHECK(current_);
  return current_;
}

void NGPaintFragmentTraversal::AncestorRange::Iterator::operator++() {
  DCHECK(current_);
  current_ = current_->Parent();
}

// ----

NGPaintFragmentTraversal::InlineDescendantsRange::Iterator::Iterator(
    const NGPaintFragment& container)
    : container_(&container), current_(container.FirstChild()) {
  if (!current_ || IsInlineFragment(*current_))
    return;
  operator++();
}

NGPaintFragment* NGPaintFragmentTraversal::InlineDescendantsRange::Iterator::
operator->() const {
  DCHECK(current_);
  return current_;
}

void NGPaintFragmentTraversal::InlineDescendantsRange::Iterator::operator++() {
  DCHECK(current_);
  do {
    current_ = Next(*current_);
  } while (current_ && !IsInlineFragment(*current_));
}

// static
// Returns next fragment of |start| in DFS pre-order within |container_| and
// skipping descendants in block formatting context.
NGPaintFragment*
NGPaintFragmentTraversal::InlineDescendantsRange::Iterator::Next(
    const NGPaintFragment& start) const {
  if (ShouldTraverse(start) && start.FirstChild())
    return start.FirstChild();
  for (NGPaintFragment* runner = const_cast<NGPaintFragment*>(&start);
       runner != container_; runner = runner->Parent()) {
    if (NGPaintFragment* next_sibling = runner->NextSibling())
      return next_sibling;
  }
  return nullptr;
}

// static
bool NGPaintFragmentTraversal::InlineDescendantsRange::Iterator::
    IsInlineFragment(const NGPaintFragment& fragment) {
  return fragment.PhysicalFragment().IsInline() ||
         fragment.PhysicalFragment().IsLineBox();
}

// static
bool NGPaintFragmentTraversal::InlineDescendantsRange::Iterator::ShouldTraverse(
    const NGPaintFragment& fragment) {
  return fragment.PhysicalFragment().IsContainer() &&
         !fragment.PhysicalFragment().IsBlockFormattingContextRoot();
}

}  // namespace blink
