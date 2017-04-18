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

#include "core/editing/markers/DocumentMarkerController.h"

#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/markers/RenderedDocumentMarker.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutObject.h"
#include <algorithm>

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

MarkerRemoverPredicate::MarkerRemoverPredicate(const Vector<String>& words)
    : words_(words) {}

bool MarkerRemoverPredicate::operator()(const DocumentMarker& document_marker,
                                        const Text& text_node) const {
  unsigned start = document_marker.StartOffset();
  unsigned length = document_marker.EndOffset() - document_marker.StartOffset();

  String marker_text = text_node.data().Substring(start, length);
  return words_.Contains(marker_text);
}

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
  }

  NOTREACHED();
  return DocumentMarker::kSpellingMarkerIndex;
}

}  // namespace

inline bool DocumentMarkerController::PossiblyHasMarkers(
    DocumentMarker::MarkerTypes types) {
  return possibly_existing_marker_types_.Intersects(types);
}

DocumentMarkerController::DocumentMarkerController(Document& document)
    : possibly_existing_marker_types_(0), document_(&document) {
  SetContext(&document);
}

void DocumentMarkerController::Clear() {
  markers_.Clear();
  possibly_existing_marker_types_ = 0;
}

void DocumentMarkerController::AddMarker(const Position& start,
                                         const Position& end,
                                         DocumentMarker::MarkerType type,
                                         const String& description) {
  // Use a TextIterator to visit the potentially multiple nodes the range
  // covers.
  for (TextIterator marked_text(start, end); !marked_text.AtEnd();
       marked_text.Advance()) {
    AddMarker(
        marked_text.CurrentContainer(),
        DocumentMarker(type, marked_text.StartOffsetInCurrentContainer(),
                       marked_text.EndOffsetInCurrentContainer(), description));
  }
}

void DocumentMarkerController::AddTextMatchMarker(
    const EphemeralRange& range,
    DocumentMarker::MatchStatus match_status) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());

  // Use a TextIterator to visit the potentially multiple nodes the range
  // covers.
  for (TextIterator marked_text(range.StartPosition(), range.EndPosition());
       !marked_text.AtEnd(); marked_text.Advance()) {
    AddMarker(marked_text.CurrentContainer(),
              DocumentMarker(marked_text.StartOffsetInCurrentContainer(),
                             marked_text.EndOffsetInCurrentContainer(),
                             match_status));
  }
  // Don't invalidate tickmarks here. TextFinder invalidates tickmarks using a
  // throttling algorithm. crbug.com/6819.
}

void DocumentMarkerController::AddCompositionMarker(const Position& start,
                                                    const Position& end,
                                                    Color underline_color,
                                                    bool thick,
                                                    Color background_color) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());

  for (TextIterator marked_text(start, end); !marked_text.AtEnd();
       marked_text.Advance())
    AddMarker(marked_text.CurrentContainer(),
              DocumentMarker(marked_text.StartOffsetInCurrentContainer(),
                             marked_text.EndOffsetInCurrentContainer(),
                             underline_color, thick, background_color));
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
    RemoveMarkers(marked_text.CurrentContainer(), start_offset,
                  end_offset - start_offset, marker_types);
  }
}

void DocumentMarkerController::RemoveMarkers(
    const EphemeralRange& range,
    DocumentMarker::MarkerTypes marker_types) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());

  TextIterator marked_text(range.StartPosition(), range.EndPosition());
  DocumentMarkerController::RemoveMarkers(marked_text, marker_types);
}

static bool StartsFurther(const Member<RenderedDocumentMarker>& lhv,
                          const DocumentMarker* rhv) {
  return lhv->StartOffset() < rhv->StartOffset();
}

static bool EndsBefore(size_t start_offset,
                       const Member<RenderedDocumentMarker>& rhv) {
  return start_offset < rhv->EndOffset();
}

static bool CompareByStart(const Member<DocumentMarker>& lhv,
                           const Member<DocumentMarker>& rhv) {
  return lhv->StartOffset() < rhv->StartOffset();
}

static bool DoesNotOverlap(const Member<RenderedDocumentMarker>& lhv,
                           const DocumentMarker* rhv) {
  return lhv->EndOffset() < rhv->StartOffset();
}

static void UpdateMarkerRenderedRect(const Node& node,
                                     RenderedDocumentMarker& marker) {
  Range* range = Range::Create(node.GetDocument());
  // The offsets of the marker may be out-dated, so check for exceptions.
  DummyExceptionStateForTesting exception_state;
  range->setStart(&const_cast<Node&>(node), marker.StartOffset(),
                  exception_state);
  if (!exception_state.HadException()) {
    range->setEnd(&const_cast<Node&>(node), marker.EndOffset(),
                  IGNORE_EXCEPTION_FOR_TESTING);
  }
  if (!exception_state.HadException()) {
    // TODO(yosin): Once we have a |EphemeralRange| version of |boundingBox()|,
    // we should use it instead of |Range| version.
    marker.SetRenderedRect(LayoutRect(range->BoundingBox()));
  } else {
    marker.NullifyRenderedRect();
  }
  range->Dispose();
}

// Markers are stored in order sorted by their start offset.
// Markers of the same type do not overlap each other.

void DocumentMarkerController::AddMarker(Node* node,
                                         const DocumentMarker& new_marker) {
  DCHECK_GE(new_marker.EndOffset(), new_marker.StartOffset());
  if (new_marker.EndOffset() == new_marker.StartOffset())
    return;

  possibly_existing_marker_types_.Add(new_marker.GetType());

  Member<MarkerLists>& markers =
      markers_.insert(node, nullptr).stored_value->value;
  if (!markers) {
    markers = new MarkerLists;
    markers->Grow(DocumentMarker::kMarkerTypeIndexesCount);
  }

  DocumentMarker::MarkerTypeIndex marker_list_index =
      MarkerTypeToMarkerIndex(new_marker.GetType());
  if (!markers->at(marker_list_index)) {
    markers->at(marker_list_index) = new MarkerList;
  }

  Member<MarkerList>& list = markers->at(marker_list_index);
  DocumentMarkerListEditor::AddMarker(list, &new_marker);

  // repaint the affected node
  if (node->GetLayoutObject()) {
    node->GetLayoutObject()->SetShouldDoFullPaintInvalidation(
        kPaintInvalidationDocumentMarkerChange);
  }
}

// TODO(rlanday): move DocumentMarkerListEditor into its own .h/.cpp files
// TODO(rlanday): this method was created by cutting and pasting code from
// DocumentMarkerController::AddMarker(), it should be refactored in a future CL
void DocumentMarkerListEditor::AddMarker(MarkerList* list,
                                         const DocumentMarker* marker) {
  RenderedDocumentMarker* rendered_marker =
      RenderedDocumentMarker::Create(*marker);
  if (list->IsEmpty() || list->back()->EndOffset() < marker->StartOffset()) {
    list->push_back(rendered_marker);
  } else {
    if (marker->GetType() != DocumentMarker::kTextMatch &&
        marker->GetType() != DocumentMarker::kComposition) {
      MergeOverlapping(list, rendered_marker);
    } else {
      MarkerList::iterator pos =
          std::lower_bound(list->begin(), list->end(), marker, StartsFurther);
      list->insert(pos - list->begin(), rendered_marker);
    }
  }
}

// TODO(rlanday): move DocumentMarkerListEditor into its own .h/.cpp files
void DocumentMarkerListEditor::MergeOverlapping(
    MarkerList* list,
    RenderedDocumentMarker* to_insert) {
  MarkerList::iterator first_overlapping =
      std::lower_bound(list->begin(), list->end(), to_insert, DoesNotOverlap);
  size_t index = first_overlapping - list->begin();
  list->insert(index, to_insert);
  MarkerList::iterator inserted = list->begin() + index;
  first_overlapping = inserted + 1;
  for (MarkerList::iterator i = first_overlapping;
       i != list->end() && (*i)->StartOffset() <= (*inserted)->EndOffset();) {
    (*inserted)->SetStartOffset(
        std::min((*inserted)->StartOffset(), (*i)->StartOffset()));
    (*inserted)->SetEndOffset(
        std::max((*inserted)->EndOffset(), (*i)->EndOffset()));
    list->erase(i - list->begin());
  }
}

// Moves markers from src_node to dst_node. Markers are moved if their start
// offset is less than length. Markers that run past that point are truncated.
void DocumentMarkerController::MoveMarkers(Node* src_node,
                                           int length,
                                           Node* dst_node) {
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
  for (size_t marker_list_index = 0; marker_list_index < src_markers->size();
       ++marker_list_index) {
    MarkerList* src_list = src_markers->at(marker_list_index);
    if (!src_list)
      continue;

    if (!dst_markers->at(marker_list_index))
      dst_markers->at(marker_list_index) = new MarkerList;
    MarkerList* dst_list = dst_markers->at(marker_list_index);

    unsigned end_offset = length - 1;
    MarkerList::iterator it;
    for (it = src_list->begin(); it != src_list->end(); ++it) {
      DocumentMarker* marker = it->Get();

      // stop if we are now past the specified range
      if (marker->StartOffset() > end_offset)
        break;

      // pin the marker to the specified range
      doc_dirty = true;
      if (marker->EndOffset() > end_offset)
        marker->SetEndOffset(end_offset);

      DocumentMarkerListEditor::AddMarker(dst_list, marker);
    }

    // Remove the range of markers that were moved to dst_node
    src_list->erase(0, it - src_list->begin());
  }

  // repaint the affected node
  if (doc_dirty && dst_node->GetLayoutObject()) {
    dst_node->GetLayoutObject()->SetShouldDoFullPaintInvalidation(
        kPaintInvalidationDocumentMarkerChange);
  }
}

void DocumentMarkerController::RemoveMarkers(
    Node* node,
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
  for (size_t marker_list_index = 0;
       marker_list_index < DocumentMarker::kMarkerTypeIndexesCount;
       ++marker_list_index) {
    Member<MarkerList>& list = (*markers)[marker_list_index];
    if (!list || list->IsEmpty()) {
      if (list.Get() && list->IsEmpty())
        list.Clear();
      ++empty_lists_count;
      continue;
    }
    if (!marker_types.Contains((*list->begin())->GetType()))
      continue;

    if (DocumentMarkerListEditor::RemoveMarkers(list, start_offset, length))
      doc_dirty = true;

    if (list->IsEmpty()) {
      list.Clear();
      ++empty_lists_count;
    }
  }

  if (empty_lists_count == DocumentMarker::kMarkerTypeIndexesCount) {
    markers_.erase(node);
    if (markers_.IsEmpty())
      possibly_existing_marker_types_ = 0;
  }

  // repaint the affected node
  if (doc_dirty && node->GetLayoutObject()) {
    node->GetLayoutObject()->SetShouldDoFullPaintInvalidation(
        kPaintInvalidationDocumentMarkerChange);
  }
}

// TODO(rlanday): move DocumentMarkerListEditor into its own .h/.cpp files
// TODO(rlanday): this method was created by cutting and pasting code from
// DocumentMarkerController::RemoveMarkers(), it should be refactored in a
// future CL
bool DocumentMarkerListEditor::RemoveMarkers(MarkerList* list,
                                             unsigned start_offset,
                                             int length) {
  bool doc_dirty = false;
  unsigned end_offset = start_offset + length;
  MarkerList::iterator start_pos =
      std::upper_bound(list->begin(), list->end(), start_offset, EndsBefore);
  for (MarkerList::iterator i = start_pos; i != list->end();) {
    DocumentMarker marker(*i->Get());

    // markers are returned in order, so stop if we are now past the specified
    // range
    if (marker.StartOffset() >= end_offset)
      break;

    list->erase(i - list->begin());
    doc_dirty = true;
  }

  return doc_dirty;
}

DocumentMarkerVector DocumentMarkerController::MarkersFor(
    Node* node,
    DocumentMarker::MarkerTypes marker_types) {
  DocumentMarkerVector result;

  MarkerLists* markers = markers_.at(node);
  if (!markers)
    return result;

  for (size_t marker_list_index = 0;
       marker_list_index < DocumentMarker::kMarkerTypeIndexesCount;
       ++marker_list_index) {
    Member<MarkerList>& list = (*markers)[marker_list_index];
    if (!list || list->IsEmpty() ||
        !marker_types.Contains((*list->begin())->GetType()))
      continue;

    for (size_t i = 0; i < list->size(); ++i)
      result.push_back(list->at(i).Get());
  }

  std::sort(result.begin(), result.end(), CompareByStart);
  return result;
}

DocumentMarkerVector DocumentMarkerController::Markers() {
  DocumentMarkerVector result;
  for (MarkerMap::iterator i = markers_.begin(); i != markers_.end(); ++i) {
    MarkerLists* markers = i->value.Get();
    for (size_t marker_list_index = 0;
         marker_list_index < DocumentMarker::kMarkerTypeIndexesCount;
         ++marker_list_index) {
      Member<MarkerList>& list = (*markers)[marker_list_index];
      for (size_t j = 0; list.Get() && j < list->size(); ++j)
        result.push_back(list->at(j).Get());
    }
  }
  std::sort(result.begin(), result.end(), CompareByStart);
  return result;
}

DocumentMarkerVector DocumentMarkerController::MarkersInRange(
    const EphemeralRange& range,
    DocumentMarker::MarkerTypes marker_types) {
  if (!PossiblyHasMarkers(marker_types))
    return DocumentMarkerVector();

  DocumentMarkerVector found_markers;

  Node* start_container = range.StartPosition().ComputeContainerNode();
  DCHECK(start_container);
  unsigned start_offset = static_cast<unsigned>(
      range.StartPosition().ComputeOffsetInContainerNode());
  Node* end_container = range.EndPosition().ComputeContainerNode();
  DCHECK(end_container);
  unsigned end_offset =
      static_cast<unsigned>(range.EndPosition().ComputeOffsetInContainerNode());

  for (Node& node : range.Nodes()) {
    for (DocumentMarker* marker : MarkersFor(&node)) {
      if (!marker_types.Contains(marker->GetType()))
        continue;
      if (node == start_container && marker->EndOffset() <= start_offset)
        continue;
      if (node == end_container && marker->StartOffset() >= end_offset)
        continue;
      found_markers.push_back(marker);
    }
  }
  return found_markers;
}

Vector<IntRect> DocumentMarkerController::RenderedRectsForMarkers(
    DocumentMarker::MarkerType marker_type) {
  Vector<IntRect> result;

  if (!PossiblyHasMarkers(marker_type))
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
    for (size_t marker_list_index = 0;
         marker_list_index < DocumentMarker::kMarkerTypeIndexesCount;
         ++marker_list_index) {
      Member<MarkerList>& list = (*markers)[marker_list_index];
      if (!list || list->IsEmpty() ||
          (*list->begin())->GetType() != marker_type)
        continue;
      for (unsigned marker_index = 0; marker_index < list->size();
           ++marker_index) {
        RenderedDocumentMarker* marker = list->at(marker_index).Get();
        UpdateMarkerRenderedRectIfNeeded(node, *marker);
        if (!marker->IsRendered())
          continue;
        result.push_back(marker->RenderedRect());
      }
    }
  }

  return result;
}

static void InvalidatePaintForTickmarks(const Node& node) {
  if (FrameView* frame_view = node.GetDocument().View())
    frame_view->InvalidatePaintForTickmarks();
}

void DocumentMarkerController::UpdateMarkerRenderedRectIfNeeded(
    const Node& node,
    RenderedDocumentMarker& marker) {
  DCHECK(!document_->View() || !document_->View()->NeedsLayout());
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  if (!marker.IsValid())
    UpdateMarkerRenderedRect(node, marker);
}

void DocumentMarkerController::InvalidateRectsForMarkersInNode(
    const Node& node) {
  MarkerLists* markers = markers_.at(&node);

  for (auto& marker_list : *markers) {
    if (!marker_list || marker_list->IsEmpty())
      continue;

    for (auto& marker : *marker_list)
      marker->Invalidate();

    if (marker_list->front()->GetType() == DocumentMarker::kTextMatch)
      InvalidatePaintForTickmarks(node);
  }
}

void DocumentMarkerController::InvalidateRectsForAllMarkers() {
  for (auto& node_markers : markers_) {
    const Node& node = *node_markers.key;
    for (auto& marker_list : *node_markers.value) {
      if (!marker_list || marker_list->IsEmpty())
        continue;

      for (auto& marker : *marker_list)
        marker->Invalidate();

      if (marker_list->front()->GetType() == DocumentMarker::kTextMatch)
        InvalidatePaintForTickmarks(node);
    }
  }
}

DEFINE_TRACE(DocumentMarkerController) {
  visitor->Trace(markers_);
  visitor->Trace(document_);
  SynchronousMutationObserver::Trace(visitor);
}

void DocumentMarkerController::RemoveMarkers(
    Node* node,
    DocumentMarker::MarkerTypes marker_types) {
  if (!PossiblyHasMarkers(marker_types))
    return;
  DCHECK(!markers_.IsEmpty());

  MarkerMap::iterator iterator = markers_.Find(node);
  if (iterator != markers_.end())
    RemoveMarkersFromList(iterator, marker_types);
}

void DocumentMarkerController::RemoveMarkers(
    const MarkerRemoverPredicate& should_remove_marker) {
  for (auto& node_markers : markers_) {
    const Node& node = *node_markers.key;
    if (!node.IsTextNode())  // MarkerRemoverPredicate requires a Text node.
      continue;
    MarkerLists& markers = *node_markers.value;
    for (size_t marker_list_index = 0;
         marker_list_index < DocumentMarker::kMarkerTypeIndexesCount;
         ++marker_list_index) {
      Member<MarkerList>& list = markers[marker_list_index];
      if (!list)
        continue;
      bool removed_markers = false;
      for (size_t j = list->size(); j > 0; --j) {
        if (should_remove_marker(*list->at(j - 1),
                                 static_cast<const Text&>(node))) {
          list->erase(j - 1);
          removed_markers = true;
        }
      }
      if (removed_markers &&
          marker_list_index == DocumentMarker::kTextMatchMarkerIndex)
        InvalidatePaintForTickmarks(node);
    }
  }
}

void DocumentMarkerController::RemoveMarkers(
    DocumentMarker::MarkerTypes marker_types) {
  if (!PossiblyHasMarkers(marker_types))
    return;
  DCHECK(!markers_.IsEmpty());

  HeapVector<Member<const Node>> nodes_with_markers;
  CopyKeysToVector(markers_, nodes_with_markers);
  unsigned size = nodes_with_markers.size();
  for (unsigned i = 0; i < size; ++i) {
    MarkerMap::iterator iterator = markers_.Find(nodes_with_markers[i]);
    if (iterator != markers_.end())
      RemoveMarkersFromList(iterator, marker_types);
  }

  possibly_existing_marker_types_.Remove(marker_types);
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

    for (size_t marker_list_index = 0;
         marker_list_index < DocumentMarker::kMarkerTypeIndexesCount;
         ++marker_list_index) {
      Member<MarkerList>& list = (*markers)[marker_list_index];
      if (!list || list->IsEmpty()) {
        if (list.Get() && list->IsEmpty())
          list.Clear();
        ++empty_lists_count;
        continue;
      }
      if (marker_types.Contains((*list->begin())->GetType())) {
        list->Clear();
        list.Clear();
        ++empty_lists_count;
        needs_repainting = true;
      }
    }

    node_can_be_removed =
        empty_lists_count == DocumentMarker::kMarkerTypeIndexesCount;
  }

  if (needs_repainting) {
    const Node& node = *iterator->key;
    if (LayoutObject* layout_object = node.GetLayoutObject()) {
      layout_object->SetShouldDoFullPaintInvalidation(
          kPaintInvalidationDocumentMarkerChange);
    }
    InvalidatePaintForTickmarks(node);
  }

  if (node_can_be_removed) {
    markers_.erase(iterator);
    if (markers_.IsEmpty())
      possibly_existing_marker_types_ = 0;
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
    for (size_t marker_list_index = 0;
         marker_list_index < DocumentMarker::kMarkerTypeIndexesCount;
         ++marker_list_index) {
      Member<MarkerList>& list = (*markers)[marker_list_index];
      if (!list || list->IsEmpty() ||
          !marker_types.Contains((*list->begin())->GetType()))
        continue;

      // cause the node to be redrawn
      if (LayoutObject* layout_object = node->GetLayoutObject()) {
        layout_object->SetShouldDoFullPaintInvalidation(
            kPaintInvalidationDocumentMarkerChange);
        break;
      }
    }
  }
}

bool DocumentMarkerController::SetMarkersActive(const EphemeralRange& range,
                                                bool active) {
  if (!PossiblyHasMarkers(DocumentMarker::AllMarkers()))
    return false;

  DCHECK(!markers_.IsEmpty());

  Node* const start_container = range.StartPosition().ComputeContainerNode();
  DCHECK(start_container);
  Node* const end_container = range.EndPosition().ComputeContainerNode();
  DCHECK(end_container);

  const unsigned container_start_offset =
      range.StartPosition().ComputeOffsetInContainerNode();
  const unsigned container_end_offset =
      range.EndPosition().ComputeOffsetInContainerNode();

  bool marker_found = false;
  for (Node& node : range.Nodes()) {
    int start_offset = node == start_container ? container_start_offset : 0;
    int end_offset = node == end_container ? container_end_offset : INT_MAX;
    marker_found |= SetMarkersActive(&node, start_offset, end_offset, active);
  }
  return marker_found;
}

bool DocumentMarkerController::SetMarkersActive(Node* node,
                                                unsigned start_offset,
                                                unsigned end_offset,
                                                bool active) {
  MarkerLists* markers = markers_.at(node);
  if (!markers)
    return false;

  bool doc_dirty = false;
  Member<MarkerList>& list =
      (*markers)[MarkerTypeToMarkerIndex(DocumentMarker::kTextMatch)];
  if (!list)
    return false;
  MarkerList::iterator start_pos =
      std::upper_bound(list->begin(), list->end(), start_offset, EndsBefore);
  for (MarkerList::iterator marker = start_pos; marker != list->end();
       ++marker) {
    // Markers are returned in order, so stop if we are now past the specified
    // range.
    if ((*marker)->StartOffset() >= end_offset)
      break;

    (*marker)->SetIsActiveMatch(active);
    doc_dirty = true;
  }

  // repaint the affected node
  if (doc_dirty && node->GetLayoutObject()) {
    node->GetLayoutObject()->SetShouldDoFullPaintInvalidation(
        kPaintInvalidationDocumentMarkerChange);
  }
  return doc_dirty;
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
    for (size_t marker_list_index = 0;
         marker_list_index < DocumentMarker::kMarkerTypeIndexesCount;
         ++marker_list_index) {
      Member<MarkerList>& list = (*markers)[marker_list_index];
      for (unsigned marker_index = 0; list.Get() && marker_index < list->size();
           ++marker_index) {
        DocumentMarker* marker = list->at(marker_index).Get();
        builder.Append(" ");
        builder.AppendNumber(marker->GetType());
        builder.Append(":[");
        builder.AppendNumber(marker->StartOffset());
        builder.Append(":");
        builder.AppendNumber(marker->EndOffset());
        builder.Append("](");
        builder.AppendNumber(marker->IsActiveMatch());
        builder.Append(")");
      }
    }
    builder.Append("\n");
  }
  LOG(INFO) << markers_.size() << " nodes have markers:\n"
            << builder.ToString().Utf8().Data();
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
  for (MarkerList* list : *markers) {
    if (!list)
      continue;

    if (DocumentMarkerListEditor::ShiftMarkers(list, offset, old_length,
                                               new_length))
      did_shift_marker = true;
  }

  if (!did_shift_marker)
    return;
  if (!node->GetLayoutObject())
    return;
  InvalidateRectsForMarkersInNode(*node);
  // repaint the affected node
  node->GetLayoutObject()->SetShouldDoFullPaintInvalidation();
}

// TODO(rlanday): move DocumentMarkerListEditor into its own .h/.cpp files
bool DocumentMarkerListEditor::ShiftMarkers(MarkerList* list,
                                            unsigned offset,
                                            unsigned old_length,
                                            unsigned new_length) {
  bool did_shift_marker = false;
  for (MarkerList::iterator it = list->begin(); it != list->end(); ++it) {
    RenderedDocumentMarker& marker = **it;
    Optional<DocumentMarker::MarkerOffsets> result =
        marker.ComputeOffsetsAfterShift(offset, old_length, new_length);
    if (result == WTF::kNullopt) {
      list->erase(it - list->begin());
      --it;
      did_shift_marker = true;
      continue;
    }

    if (marker.StartOffset() != result.value().start_offset ||
        marker.EndOffset() != result.value().end_offset) {
      did_shift_marker = true;
      marker.SetStartOffset(result.value().start_offset);
      marker.SetEndOffset(result.value().end_offset);
    }
  }

  return did_shift_marker;
}

}  // namespace blink

#ifndef NDEBUG
void showDocumentMarkers(const blink::DocumentMarkerController* controller) {
  if (controller)
    controller->ShowMarkers();
}
#endif
