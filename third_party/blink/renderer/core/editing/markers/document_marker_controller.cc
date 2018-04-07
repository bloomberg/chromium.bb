/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"

#include <algorithm>
#include "third_party/blink/renderer/core/dom/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/active_suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/markers/active_suggestion_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"
#include "third_party/blink/renderer/core/editing/markers/composition_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/grammar_marker.h"
#include "third_party/blink/renderer/core/editing/markers/grammar_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/sorted_document_marker_list_editor.h"
#include "third_party/blink/renderer/core/editing/markers/spelling_marker.h"
#include "third_party/blink/renderer/core/editing/markers/spelling_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

namespace {

DocumentMarker::MarkerTypeIndex MarkerTypeToMarkerIndex(
    DocumentMarker::MarkerType type) {
  switch (type) {
    case DocumentMarker::kSpelling:
      return DocumentMarker::kSpellingMarkerIndex;
    case DocumentMarker::kGrammar:
      return DocumentMarker::kGrammarMarkerIndex;
    case DocumentMarker::kTextMatch:
      return DocumentMarker::kTextMatchMarkerIndex;
    case DocumentMarker::kComposition:
      return DocumentMarker::kCompositionMarkerIndex;
    case DocumentMarker::kActiveSuggestion:
      return DocumentMarker::kActiveSuggestionMarkerIndex;
    case DocumentMarker::kSuggestion:
      return DocumentMarker::kSuggestionMarkerIndex;
  }

  NOTREACHED();
  return DocumentMarker::kSpellingMarkerIndex;
}

DocumentMarkerList* CreateListForType(DocumentMarker::MarkerType type) {
  switch (type) {
    case DocumentMarker::kActiveSuggestion:
      return new ActiveSuggestionMarkerListImpl();
    case DocumentMarker::kComposition:
      return new CompositionMarkerListImpl();
    case DocumentMarker::kSpelling:
      return new SpellingMarkerListImpl();
    case DocumentMarker::kGrammar:
      return new GrammarMarkerListImpl();
    case DocumentMarker::kSuggestion:
      return new SuggestionMarkerListImpl();
    case DocumentMarker::kTextMatch:
      return new TextMatchMarkerListImpl();
  }

  NOTREACHED();
  return nullptr;
}

void InvalidatePaintForNode(const Node& node) {
  if (!node.GetLayoutObject())
    return;

  node.GetLayoutObject()->SetShouldDoFullPaintInvalidation(
      PaintInvalidationReason::kDocumentMarker);

  // Tell accessibility about the new marker.
  AXObjectCache* ax_object_cache = node.GetDocument().ExistingAXObjectCache();
  if (!ax_object_cache)
    return;
  // TODO(nektar): Do major refactoring of all AX classes to comply with const
  // correctness.
  Node* non_const_node = &const_cast<Node&>(node);
  ax_object_cache->HandleTextMarkerDataAdded(non_const_node, non_const_node);
}

}  // namespace

Member<DocumentMarkerList>& DocumentMarkerController::ListForType(
    MarkerLists* marker_lists,
    DocumentMarker::MarkerType type) {
  const size_t marker_list_index = MarkerTypeToMarkerIndex(type);
  return (*marker_lists)[marker_list_index];
}

inline bool DocumentMarkerController::PossiblyHasMarkers(
    DocumentMarker::MarkerTypes types) {
  if (markers_.IsEmpty()) {
    // It's possible for markers_ to become empty through garbage collection if
    // all its Nodes are GC'ed since we only hold weak references, in which case
    // possibly_existing_marker_types_ isn't reset to 0 as it is in the other
    // codepaths that remove from markers_. Therefore, we check for this case
    // here.

    // Alternatively, we could handle this case at the time the Node is GC'ed,
    // but that operation is more performance-sensitive than anywhere
    // PossiblyHasMarkers() is used.
    possibly_existing_marker_types_ = 0;
    SetContext(nullptr);
    return false;
  }

  return possibly_existing_marker_types_.Intersects(types);
}

DocumentMarkerController::DocumentMarkerController(Document& document)
    : possibly_existing_marker_types_(0), document_(&document) {
}

void DocumentMarkerController::Clear() {
  markers_.clear();
  possibly_existing_marker_types_ = 0;
  SetContext(nullptr);
}

void DocumentMarkerController::AddSpellingMarker(const EphemeralRange& range,
                                                 const String& description) {
  AddMarkerInternal(range, [&description](int start_offset, int end_offset) {
    return new SpellingMarker(start_offset, end_offset, description);
  });
}

void DocumentMarkerController::AddGrammarMarker(const EphemeralRange& range,
                                                const String& description) {
  AddMarkerInternal(range, [&description](int start_offset, int end_offset) {
    return new GrammarMarker(start_offset, end_offset, description);
  });
}

void DocumentMarkerController::AddTextMatchMarker(
    const EphemeralRange& range,
    TextMatchMarker::MatchStatus match_status) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  AddMarkerInternal(range, [match_status](int start_offset, int end_offset) {
    return new TextMatchMarker(start_offset, end_offset, match_status);
  });
  // Don't invalidate tickmarks here. TextFinder invalidates tickmarks using a
  // throttling algorithm. crbug.com/6819.
}

void DocumentMarkerController::AddCompositionMarker(
    const EphemeralRange& range,
    Color underline_color,
    ui::mojom::ImeTextSpanThickness thickness,
    Color background_color) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  AddMarkerInternal(range, [underline_color, thickness, background_color](
                               int start_offset, int end_offset) {
    return new CompositionMarker(start_offset, end_offset, underline_color,
                                 thickness, background_color);
  });
}

void DocumentMarkerController::AddActiveSuggestionMarker(
    const EphemeralRange& range,
    Color underline_color,
    ui::mojom::ImeTextSpanThickness thickness,
    Color background_color) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  AddMarkerInternal(range, [underline_color, thickness, background_color](
                               int start_offset, int end_offset) {
    return new ActiveSuggestionMarker(start_offset, end_offset, underline_color,
                                      thickness, background_color);
  });
}

void DocumentMarkerController::AddSuggestionMarker(
    const EphemeralRange& range,
    const SuggestionMarkerProperties& properties) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  AddMarkerInternal(
      range, [this, &properties](int start_offset, int end_offset) {
        return new SuggestionMarker(start_offset, end_offset, properties);
      });
}

void DocumentMarkerController::PrepareForDestruction() {
  Clear();
}

void DocumentMarkerController::RemoveMarkers(
    TextIterator& marked_text,
    DocumentMarker::MarkerTypes marker_types) {
  for (; !marked_text.AtEnd(); marked_text.Advance()) {
    if (!PossiblyHasMarkers(marker_types))
      return;
    DCHECK(!markers_.IsEmpty());

    int start_offset = marked_text.StartOffsetInCurrentContainer();
    int end_offset = marked_text.EndOffsetInCurrentContainer();
    RemoveMarkersInternal(marked_text.CurrentContainer(), start_offset,
                          end_offset - start_offset, marker_types);
  }
}

void DocumentMarkerController::RemoveMarkersInRange(
    const EphemeralRange& range,
    DocumentMarker::MarkerTypes marker_types) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());

  TextIterator marked_text(range.StartPosition(), range.EndPosition());
  DocumentMarkerController::RemoveMarkers(marked_text, marker_types);
}

void DocumentMarkerController::AddMarkerInternal(
    const EphemeralRange& range,
    std::function<DocumentMarker*(int, int)> create_marker_from_offsets) {
  for (TextIterator marked_text(range.StartPosition(), range.EndPosition());
       !marked_text.AtEnd(); marked_text.Advance()) {
    const int start_offset_in_current_container =
        marked_text.StartOffsetInCurrentContainer();
    const int end_offset_in_current_container =
        marked_text.EndOffsetInCurrentContainer();

    DCHECK_GE(end_offset_in_current_container,
              start_offset_in_current_container);

    // TODO(editing-dev): TextIterator sometimes emits ranges where the start
    // and end offsets are the same. Investigate if TextIterator should be
    // changed to not do this. See crbug.com/727929
    if (end_offset_in_current_container == start_offset_in_current_container)
      continue;

    // Ignore text emitted by TextIterator for non-text nodes (e.g. implicit
    // newlines)
    const Node* const node = marked_text.CurrentContainer();
    if (!node->IsTextNode())
      continue;

    DocumentMarker* const new_marker = create_marker_from_offsets(
        start_offset_in_current_container, end_offset_in_current_container);
    AddMarkerToNode(node, new_marker);
  }
}

void DocumentMarkerController::AddMarkerToNode(const Node* node,
                                               DocumentMarker* new_marker) {
  possibly_existing_marker_types_.Add(new_marker->GetType());
  SetContext(document_);

  Member<MarkerLists>& markers =
      markers_.insert(node, nullptr).stored_value->value;
  if (!markers) {
    markers = new MarkerLists;
    markers->Grow(DocumentMarker::kMarkerTypeIndexesCount);
  }

  const DocumentMarker::MarkerType new_marker_type = new_marker->GetType();
  if (!ListForType(markers, new_marker_type))
    ListForType(markers, new_marker_type) = CreateListForType(new_marker_type);

  DocumentMarkerList* const list = ListForType(markers, new_marker_type);
  list->Add(new_marker);

  InvalidatePaintForNode(*node);
}

// Moves markers from src_node to dst_node. Markers are moved if their start
// offset is less than length. Markers that run past that point are truncated.
void DocumentMarkerController::MoveMarkers(const Node* src_node,
                                           int length,
                                           const Node* dst_node) {
  if (length <= 0)
    return;

  if (!PossiblyHasMarkers(DocumentMarker::AllMarkers()))
    return;
  DCHECK(!markers_.IsEmpty());

  MarkerLists* src_markers = markers_.at(src_node);
  if (!src_markers)
    return;

  if (!markers_.Contains(dst_node)) {
    markers_.insert(dst_node,
                    new MarkerLists(DocumentMarker::kMarkerTypeIndexesCount));
  }
  MarkerLists* dst_markers = markers_.at(dst_node);

  bool doc_dirty = false;
  for (DocumentMarker::MarkerType type : DocumentMarker::AllMarkers()) {
    DocumentMarkerList* const src_list = ListForType(src_markers, type);
    if (!src_list)
      continue;

    if (!ListForType(dst_markers, type))
      ListForType(dst_markers, type) = CreateListForType(type);

    DocumentMarkerList* const dst_list = ListForType(dst_markers, type);
    if (src_list->MoveMarkers(length, dst_list))
      doc_dirty = true;
  }

  if (!doc_dirty)
    return;

  InvalidatePaintForNode(*dst_node);
}

void DocumentMarkerController::RemoveMarkersInternal(
    const Node* node,
    unsigned start_offset,
    int length,
    DocumentMarker::MarkerTypes marker_types) {
  if (length <= 0)
    return;

  if (!PossiblyHasMarkers(marker_types))
    return;
  DCHECK(!(markers_.IsEmpty()));

  MarkerLists* markers = markers_.at(node);
  if (!markers)
    return;

  bool doc_dirty = false;
  size_t empty_lists_count = 0;
  for (DocumentMarker::MarkerType type : DocumentMarker::AllMarkers()) {
    DocumentMarkerList* const list = ListForType(markers, type);
    if (!list || list->IsEmpty()) {
      if (list && list->IsEmpty())
        ListForType(markers, type) = nullptr;
      ++empty_lists_count;
      continue;
    }
    if (!marker_types.Contains(type))
      continue;

    if (list->RemoveMarkers(start_offset, length))
      doc_dirty = true;

    if (list->IsEmpty()) {
      ListForType(markers, type) = nullptr;
      ++empty_lists_count;
    }
  }

  if (empty_lists_count == DocumentMarker::kMarkerTypeIndexesCount) {
    markers_.erase(node);
    if (markers_.IsEmpty()) {
      possibly_existing_marker_types_ = 0;
      SetContext(nullptr);
    }
  }

  if (!doc_dirty)
    return;

  InvalidatePaintForNode(*node);
}

DocumentMarker* DocumentMarkerController::FirstMarkerIntersectingOffsetRange(
    const Text& node,
    unsigned start_offset,
    unsigned end_offset,
    DocumentMarker::MarkerTypes types) {
  if (!PossiblyHasMarkers(types))
    return nullptr;

  // Minor optimization: if we have an empty range at a node boundary, it
  // doesn't fall in the interior of any marker.
  if (start_offset == 0 && end_offset == 0)
    return nullptr;
  const unsigned node_length = node.length();
  if (start_offset == node_length && end_offset == node_length)
    return nullptr;

  MarkerLists* const markers = markers_.at(&node);
  if (!markers)
    return nullptr;

  for (DocumentMarker::MarkerType type : types) {
    const DocumentMarkerList* const list = ListForType(markers, type);
    if (!list)
      continue;

    DocumentMarker* found_marker =
        list->FirstMarkerIntersectingRange(start_offset, end_offset);
    if (found_marker)
      return found_marker;
  }

  return nullptr;
}

HeapVector<std::pair<Member<Node>, Member<DocumentMarker>>>
DocumentMarkerController::MarkersIntersectingRange(
    const EphemeralRangeInFlatTree& range,
    DocumentMarker::MarkerTypes types) {
  HeapVector<std::pair<Member<Node>, Member<DocumentMarker>>> node_marker_pairs;
  if (!PossiblyHasMarkers(types))
    return node_marker_pairs;

  const Node* const range_start_container =
      range.StartPosition().ComputeContainerNode();
  const unsigned range_start_offset =
      range.StartPosition().ComputeOffsetInContainerNode();
  const Node* const range_end_container =
      range.EndPosition().ComputeContainerNode();
  const unsigned range_end_offset =
      range.EndPosition().ComputeOffsetInContainerNode();

  for (Node& node : range.Nodes()) {
    MarkerLists* const markers = markers_.at(&node);
    if (!markers)
      continue;

    for (DocumentMarker::MarkerType type : types) {
      const DocumentMarkerList* const list = ListForType(markers, type);
      if (!list)
        continue;

      const unsigned start_offset =
          node == range_start_container ? range_start_offset : 0;
      const unsigned max_character_offset = ToCharacterData(node).length();
      const unsigned end_offset =
          node == range_end_container ? range_end_offset : max_character_offset;

      // Minor optimization: if we have an empty offset range at the boundary
      // of a text node, it doesn't fall into the interior of any marker.
      if (start_offset == 0 && end_offset == 0)
        continue;
      if (start_offset == max_character_offset && end_offset == 0)
        continue;

      const DocumentMarkerVector& markers_from_this_list =
          list->MarkersIntersectingRange(start_offset, end_offset);
      for (DocumentMarker* marker : markers_from_this_list)
        node_marker_pairs.push_back(std::make_pair(&node, marker));
    }
  }

  return node_marker_pairs;
}

DocumentMarkerVector DocumentMarkerController::MarkersFor(
    const Node* node,
    DocumentMarker::MarkerTypes marker_types) {
  DocumentMarkerVector result;
  if (!PossiblyHasMarkers(marker_types))
    return result;

  MarkerLists* markers = markers_.at(node);
  if (!markers)
    return result;

  for (DocumentMarker::MarkerType type : marker_types) {
    DocumentMarkerList* const list = ListForType(markers, type);
    if (!list || list->IsEmpty())
      continue;

    result.AppendVector(list->GetMarkers());
  }

  std::sort(result.begin(), result.end(),
            [](const Member<DocumentMarker>& marker1,
               const Member<DocumentMarker>& marker2) {
              return marker1->StartOffset() < marker2->StartOffset();
            });
  return result;
}

DocumentMarkerVector DocumentMarkerController::Markers() {
  DocumentMarkerVector result;
  for (MarkerMap::iterator i = markers_.begin(); i != markers_.end(); ++i) {
    MarkerLists* markers = i->value.Get();
    for (DocumentMarker::MarkerType type : DocumentMarker::AllMarkers()) {
      DocumentMarkerList* const list = ListForType(markers, type);
      if (!list)
        continue;
      result.AppendVector(list->GetMarkers());
    }
  }
  std::sort(result.begin(), result.end(),
            [](const Member<DocumentMarker>& marker1,
               const Member<DocumentMarker>& marker2) {
              return marker1->StartOffset() < marker2->StartOffset();
            });
  return result;
}

Vector<IntRect> DocumentMarkerController::LayoutRectsForTextMatchMarkers() {
  DCHECK(!document_->View()->NeedsLayout());
  DCHECK(!document_->NeedsLayoutTreeUpdate());

  Vector<IntRect> result;

  if (!PossiblyHasMarkers(DocumentMarker::kTextMatch))
    return result;
  DCHECK(!(markers_.IsEmpty()));

  // outer loop: process each node
  MarkerMap::iterator end = markers_.end();
  for (MarkerMap::iterator node_iterator = markers_.begin();
       node_iterator != end; ++node_iterator) {
    // inner loop; process each marker in this node
    const Node& node = *node_iterator->key;
    if (!node.isConnected())
      continue;
    MarkerLists* markers = node_iterator->value.Get();
    DocumentMarkerList* const list =
        ListForType(markers, DocumentMarker::kTextMatch);
    if (!list)
      continue;
    result.AppendVector(ToTextMatchMarkerListImpl(list)->LayoutRects(node));
  }

  return result;
}

static void InvalidatePaintForTickmarks(const Node& node) {
  if (LocalFrameView* frame_view = node.GetDocument().View())
    frame_view->InvalidatePaintForTickmarks();
}

void DocumentMarkerController::InvalidateRectsForTextMatchMarkersInNode(
    const Node& node) {
  MarkerLists* markers = markers_.at(&node);

  const DocumentMarkerList* const marker_list =
      ListForType(markers, DocumentMarker::kTextMatch);
  if (!marker_list || marker_list->IsEmpty())
    return;

  const HeapVector<Member<DocumentMarker>>& markers_in_list =
      marker_list->GetMarkers();
  for (auto& marker : markers_in_list)
    ToTextMatchMarker(marker)->Invalidate();

  InvalidatePaintForTickmarks(node);
}

void DocumentMarkerController::InvalidateRectsForAllTextMatchMarkers() {
  for (auto& node_markers : markers_) {
    const Node& node = *node_markers.key;
    InvalidateRectsForTextMatchMarkersInNode(node);
  }
}

void DocumentMarkerController::Trace(blink::Visitor* visitor) {
  visitor->Trace(markers_);
  visitor->Trace(document_);
  SynchronousMutationObserver::Trace(visitor);
}

void DocumentMarkerController::RemoveMarkersForNode(
    const Node* node,
    DocumentMarker::MarkerTypes marker_types) {
  if (!PossiblyHasMarkers(marker_types))
    return;
  DCHECK(!markers_.IsEmpty());

  MarkerMap::iterator iterator = markers_.find(node);
  if (iterator != markers_.end())
    RemoveMarkersFromList(iterator, marker_types);
}

void DocumentMarkerController::RemoveSpellingMarkersUnderWords(
    const Vector<String>& words) {
  for (auto& node_markers : markers_) {
    const Node& node = *node_markers.key;
    if (!node.IsTextNode())
      continue;
    MarkerLists* markers = node_markers.value;
    for (DocumentMarker::MarkerType type :
         DocumentMarker::MisspellingMarkers()) {
      DocumentMarkerList* const list = ListForType(markers, type);
      if (!list)
        continue;
      ToSpellCheckMarkerListImpl(list)->RemoveMarkersUnderWords(
          ToText(node).data(), words);
    }
  }
}

void DocumentMarkerController::RemoveSuggestionMarkerByTag(const Node* node,
                                                           int32_t marker_tag) {
  MarkerLists* markers = markers_.at(node);
  SuggestionMarkerListImpl* const list = ToSuggestionMarkerListImpl(
      ListForType(markers, DocumentMarker::kSuggestion));
  if (!list->RemoveMarkerByTag(marker_tag))
    return;
  InvalidatePaintForNode(*node);
}

void DocumentMarkerController::RemoveMarkersOfTypes(
    DocumentMarker::MarkerTypes marker_types) {
  if (!PossiblyHasMarkers(marker_types))
    return;
  DCHECK(!markers_.IsEmpty());

  HeapVector<Member<const Node>> nodes_with_markers;
  CopyKeysToVector(markers_, nodes_with_markers);
  unsigned size = nodes_with_markers.size();
  for (unsigned i = 0; i < size; ++i) {
    MarkerMap::iterator iterator = markers_.find(nodes_with_markers[i]);
    if (iterator != markers_.end())
      RemoveMarkersFromList(iterator, marker_types);
  }

  possibly_existing_marker_types_.Remove(marker_types);
  if (PossiblyHasMarkers(DocumentMarker::AllMarkers()))
    return;
  SetContext(nullptr);
}

void DocumentMarkerController::RemoveMarkersFromList(
    MarkerMap::iterator iterator,
    DocumentMarker::MarkerTypes marker_types) {
  bool needs_repainting = false;
  bool node_can_be_removed;

  size_t empty_lists_count = 0;
  if (marker_types == DocumentMarker::AllMarkers()) {
    needs_repainting = true;
    node_can_be_removed = true;
  } else {
    MarkerLists* markers = iterator->value.Get();

    for (DocumentMarker::MarkerType type : DocumentMarker::AllMarkers()) {
      DocumentMarkerList* const list = ListForType(markers, type);
      if (!list || list->IsEmpty()) {
        if (list && list->IsEmpty())
          ListForType(markers, type) = nullptr;
        ++empty_lists_count;
        continue;
      }
      if (marker_types.Contains(type)) {
        list->Clear();
        ListForType(markers, type) = nullptr;
        ++empty_lists_count;
        needs_repainting = true;
      }
    }

    node_can_be_removed =
        empty_lists_count == DocumentMarker::kMarkerTypeIndexesCount;
  }

  if (needs_repainting) {
    const Node& node = *iterator->key;
    InvalidatePaintForNode(node);
    InvalidatePaintForTickmarks(node);
  }

  if (node_can_be_removed) {
    markers_.erase(iterator);
    if (markers_.IsEmpty()) {
      possibly_existing_marker_types_ = 0;
      SetContext(nullptr);
    }
  }
}

void DocumentMarkerController::RepaintMarkers(
    DocumentMarker::MarkerTypes marker_types) {
  if (!PossiblyHasMarkers(marker_types))
    return;
  DCHECK(!markers_.IsEmpty());

  // outer loop: process each markered node in the document
  MarkerMap::iterator end = markers_.end();
  for (MarkerMap::iterator i = markers_.begin(); i != end; ++i) {
    const Node* node = i->key;

    // inner loop: process each marker in the current node
    MarkerLists* markers = i->value.Get();
    for (DocumentMarker::MarkerType type : DocumentMarker::AllMarkers()) {
      DocumentMarkerList* const list = ListForType(markers, type);
      if (!list || list->IsEmpty() || !marker_types.Contains(type))
        continue;

      InvalidatePaintForNode(*node);
    }
  }
}

bool DocumentMarkerController::SetTextMatchMarkersActive(
    const EphemeralRange& range,
    bool active) {
  if (!PossiblyHasMarkers(DocumentMarker::kTextMatch))
    return false;

  DCHECK(!markers_.IsEmpty());

  const Node* const start_container =
      range.StartPosition().ComputeContainerNode();
  DCHECK(start_container);
  const Node* const end_container = range.EndPosition().ComputeContainerNode();
  DCHECK(end_container);

  const unsigned container_start_offset =
      range.StartPosition().ComputeOffsetInContainerNode();
  const unsigned container_end_offset =
      range.EndPosition().ComputeOffsetInContainerNode();

  bool marker_found = false;
  for (Node& node : range.Nodes()) {
    int start_offset = node == start_container ? container_start_offset : 0;
    int end_offset = node == end_container ? container_end_offset : INT_MAX;
    marker_found |=
        SetTextMatchMarkersActive(&node, start_offset, end_offset, active);
  }
  return marker_found;
}

bool DocumentMarkerController::SetTextMatchMarkersActive(const Node* node,
                                                         unsigned start_offset,
                                                         unsigned end_offset,
                                                         bool active) {
  MarkerLists* markers = markers_.at(node);
  if (!markers)
    return false;

  DocumentMarkerList* const list =
      ListForType(markers, DocumentMarker::kTextMatch);
  if (!list)
    return false;

  bool doc_dirty = ToTextMatchMarkerListImpl(list)->SetTextMatchMarkersActive(
      start_offset, end_offset, active);

  if (!doc_dirty)
    return false;
  InvalidatePaintForNode(*node);
  return true;
}

#ifndef NDEBUG
void DocumentMarkerController::ShowMarkers() const {
  StringBuilder builder;
  MarkerMap::const_iterator end = markers_.end();
  for (MarkerMap::const_iterator node_iterator = markers_.begin();
       node_iterator != end; ++node_iterator) {
    const Node* node = node_iterator->key;
    builder.Append(String::Format("%p", node));
    MarkerLists* markers = markers_.at(node);
    for (DocumentMarker::MarkerType type : DocumentMarker::AllMarkers()) {
      DocumentMarkerList* const list = ListForType(markers, type);
      if (!list)
        continue;

      const HeapVector<Member<DocumentMarker>>& markers_in_list =
          list->GetMarkers();
      for (const DocumentMarker* marker : markers_in_list) {
        builder.Append(" ");
        builder.AppendNumber(marker->GetType());
        builder.Append(":[");
        builder.AppendNumber(marker->StartOffset());
        builder.Append(":");
        builder.AppendNumber(marker->EndOffset());
        builder.Append("](");
        builder.AppendNumber(type == DocumentMarker::kTextMatch
                                 ? ToTextMatchMarker(marker)->IsActiveMatch()
                                 : 0);
        builder.Append(")");
      }
    }
    builder.Append("\n");
  }
  LOG(INFO) << markers_.size() << " nodes have markers:\n"
            << builder.ToString().Utf8().data();
}
#endif

// SynchronousMutationObserver
void DocumentMarkerController::DidUpdateCharacterData(CharacterData* node,
                                                      unsigned offset,
                                                      unsigned old_length,
                                                      unsigned new_length) {
  if (!PossiblyHasMarkers(DocumentMarker::AllMarkers()))
    return;
  DCHECK(!markers_.IsEmpty());

  MarkerLists* markers = markers_.at(node);
  if (!markers)
    return;

  bool did_shift_marker = false;
  for (DocumentMarkerList* const list : *markers) {
    if (!list)
      continue;

    if (list->ShiftMarkers(node->data(), offset, old_length, new_length))
      did_shift_marker = true;
  }

  if (!did_shift_marker)
    return;
  if (!node->GetLayoutObject())
    return;
  InvalidateRectsForTextMatchMarkersInNode(*node);
  InvalidatePaintForNode(*node);
}

}  // namespace blink

#ifndef NDEBUG
void showDocumentMarkers(const blink::DocumentMarkerController* controller) {
  if (controller)
    controller->ShowMarkers();
}
#endif
