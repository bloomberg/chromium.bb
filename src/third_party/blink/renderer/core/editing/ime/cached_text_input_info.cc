// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/cached_text_input_info.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

EphemeralRange ComputeWholeContentRange(const ContainerNode& container) {
  const auto& range = EphemeralRange::RangeOfContents(container);
  auto* const text_control_element = EnclosingTextControl(&container);
  if (!text_control_element)
    return range;
  auto* const inner_editor = text_control_element->InnerEditorElement();
  if (container != inner_editor)
    return range;
  auto* const last_child = inner_editor->lastChild();
  if (!IsA<HTMLBRElement>(last_child))
    return range;
  const Node* const before_placeholder = last_child->previousSibling();
  if (!before_placeholder) {
    // In case of <div><br></div>.
    return EphemeralRange(Position::FirstPositionInNode(container),
                          Position::FirstPositionInNode(container));
  }
  // We ignore placeholder <br> in <textarea> added by
  // |TextControlElement::AddPlaceholderBreakElementIfNecessary()|.
  // See http://crbug.com/1194349
  return EphemeralRange(Position::FirstPositionInNode(container),
                        Position::AfterNode(*before_placeholder));
}

}  // namespace

// static
TextIteratorBehavior CachedTextInputInfo::Behavior() {
  return TextIteratorBehavior::Builder()
      .SetEmitsObjectReplacementCharacter(true)
      .SetEmitsSpaceForNbsp(true)
      .Build();
}

void CachedTextInputInfo::ClearIfNeeded(const LayoutObject& layout_object) {
  if (layout_object_ != &layout_object)
    return;
  container_ = nullptr;
  layout_object_ = nullptr;
  text_ = g_empty_string;
  composition_.Clear();
  selection_.Clear();
  offset_map_.clear();
}

void CachedTextInputInfo::DidLayoutSubtree(const LayoutObject& layout_object) {
  // <div style="contain:strict; ...">abc</div> reaches here.
  if (!container_)
    return;
  Node* const node = layout_object.NonPseudoNode();
  if (!node)
    return;
  const ContainerNode* const container =
      RootEditableElementOrTreeScopeRootNodeOf(Position(node, 0));
  if (!container || !container->GetLayoutObject())
    return;
  ClearIfNeeded(*container->GetLayoutObject());
}

void CachedTextInputInfo::DidUpdateLayout(const LayoutObject& layout_object) {
  ClearIfNeeded(layout_object);
}

void CachedTextInputInfo::EnsureCached(const ContainerNode& container) const {
  if (IsValidFor(container))
    return;
  offset_map_.clear();
  container_ = &container;
  layout_object_ = container.GetLayoutObject();
  composition_.Clear();
  selection_.Clear();
  text_ = g_empty_string;

  TextIteratorAlgorithm<EditingStrategy> it(ComputeWholeContentRange(container),
                                            Behavior());
  if (it.AtEnd())
    return;

  const bool needs_text = HasEditableStyle(*container_);

  // The initial buffer size can be critical for performance:
  // https://bugs.webkit.org/show_bug.cgi?id=81192
  constexpr unsigned kInitialCapacity = 1 << 15;

  StringBuilder builder;
  if (needs_text) {
    unsigned capacity = kInitialCapacity;
    if (auto* block_flow =
            DynamicTo<LayoutBlockFlow>(container.GetLayoutObject())) {
      if (block_flow->HasNGInlineNodeData()) {
        if (const auto* mapping = NGInlineNode::GetOffsetMapping(block_flow))
          capacity = mapping->GetText().length();
      }
    }
    builder.ReserveCapacity(capacity);
  }

  const Node* last_text_node = nullptr;
  unsigned length = 0;
  for (; !it.AtEnd(); it.Advance()) {
    const Node* node = it.GetTextState().PositionNode();
    if (last_text_node != node && IsA<Text>(node)) {
      last_text_node = node;
      offset_map_.insert(To<Text>(node), length);
    }
    if (needs_text)
      it.GetTextState().AppendTextToStringBuilder(builder);
    length += it.GetTextState().length();
  }

  if (!builder.IsEmpty())
    text_ = builder.ToString();
}

PlainTextRange CachedTextInputInfo::GetComposition(
    const EphemeralRange& range) const {
  DCHECK(container_);
  return GetPlainTextRangeWithCache(range, &composition_);
}

PlainTextRange CachedTextInputInfo::GetPlainTextRangeWithCache(
    const EphemeralRange& range,
    CachedPlainTextRange* text_range) const {
  if (!text_range->IsValidFor(range))
    text_range->Set(range, GetPlainTextRange(range));
  return text_range->Get();
}

PlainTextRange CachedTextInputInfo::GetPlainTextRange(
    const EphemeralRange& range) const {
  if (range.IsNull())
    return PlainTextRange();
  const Position container_start = Position(*container_, 0);
  // When selection is moved to another editable during IME composition,
  // |range| may not in |container|. See http://crbug.com/1161562
  if (container_start > range.StartPosition())
    return PlainTextRange();
  const unsigned start_offset =
      RangeLength(EphemeralRange(container_start, range.StartPosition()));
  const unsigned end_offset =
      range.IsCollapsed()
          ? start_offset
          : RangeLength(EphemeralRange(container_start, range.EndPosition()));
  DCHECK_EQ(
      static_cast<unsigned>(TextIterator::RangeLength(
          EphemeralRange(container_start, range.EndPosition()), Behavior())),
      end_offset);
  return PlainTextRange(start_offset, end_offset);
}

PlainTextRange CachedTextInputInfo::GetSelection(
    const EphemeralRange& range) const {
  DCHECK(container_);
  return GetPlainTextRangeWithCache(range, &selection_);
}

String CachedTextInputInfo::GetText() const {
  DCHECK(container_);
  DCHECK(HasEditableStyle(*container_));
  return text_;
}

bool CachedTextInputInfo::IsValidFor(const ContainerNode& container) const {
  return container_ == container &&
         layout_object_ == container.GetLayoutObject();
}

void CachedTextInputInfo::LayoutObjectWillBeDestroyed(
    const LayoutObject& layout_object) {
  ClearIfNeeded(layout_object);
}

unsigned CachedTextInputInfo::RangeLength(const EphemeralRange& range) const {
  const Node* const node = range.EndPosition().AnchorNode();
  if (range.StartPosition() == Position(*container_, 0) && IsA<Text>(node)) {
    const auto it = offset_map_.find(To<Text>(node));
    if (it != offset_map_.end()) {
      const unsigned length =
          it->value +
          TextIterator::RangeLength(
              EphemeralRange(Position(node, 0), range.EndPosition()),
              Behavior());
      DCHECK_EQ(
          static_cast<unsigned>(TextIterator::RangeLength(range, Behavior())),
          length)
          << it->value << " " << range;
      return length;
    }
  }
  return TextIterator::RangeLength(range, Behavior());
}

void CachedTextInputInfo::Trace(Visitor* visitor) const {
  visitor->Trace(container_);
  visitor->Trace(composition_);
  visitor->Trace(offset_map_);
  visitor->Trace(selection_);
}

void CachedTextInputInfo::CachedPlainTextRange::Clear() {
  start_ = end_ = Position();
  start_offset_ = end_offset_ = kNotFound;
}

PlainTextRange CachedTextInputInfo::CachedPlainTextRange::Get() const {
  if (start_offset_ == kNotFound)
    return PlainTextRange();
  return PlainTextRange(start_offset_, end_offset_);
}

bool CachedTextInputInfo::CachedPlainTextRange::IsValidFor(
    const EphemeralRange& range) const {
  return range.StartPosition() == start_ && range.EndPosition() == end_;
}

void CachedTextInputInfo::CachedPlainTextRange::Set(
    const EphemeralRange& range,
    const PlainTextRange& text_range) {
  start_ = range.StartPosition();
  end_ = range.EndPosition();
  if (text_range.IsNull()) {
    start_offset_ = end_offset_ = kNotFound;
  } else {
    start_offset_ = text_range.Start();
    end_offset_ = text_range.End();
  }
}

void CachedTextInputInfo::CachedPlainTextRange::Trace(Visitor* visitor) const {
  visitor->Trace(start_);
  visitor->Trace(end_);
}

}  // namespace blink
