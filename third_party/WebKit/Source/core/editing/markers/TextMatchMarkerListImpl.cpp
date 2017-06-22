// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/TextMatchMarkerListImpl.h"

#include "core/dom/Node.h"
#include "core/dom/Range.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/markers/DocumentMarkerListEditor.h"
#include "core/editing/markers/TextMatchMarker.h"
#include "third_party/WebKit/Source/core/editing/VisibleUnits.h"

namespace blink {

DocumentMarker::MarkerType TextMatchMarkerListImpl::MarkerType() const {
  return DocumentMarker::kTextMatch;
}

bool TextMatchMarkerListImpl::IsEmpty() const {
  return markers_.IsEmpty();
}

void TextMatchMarkerListImpl::Add(DocumentMarker* marker) {
  DocumentMarkerListEditor::AddMarkerWithoutMergingOverlapping(&markers_,
                                                               marker);
}

void TextMatchMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>& TextMatchMarkerListImpl::GetMarkers()
    const {
  return markers_;
}

HeapVector<Member<DocumentMarker>>
TextMatchMarkerListImpl::MarkersIntersectingRange(unsigned start_offset,
                                                  unsigned end_offset) const {
  return DocumentMarkerListEditor::MarkersIntersectingRange(
      markers_, start_offset, end_offset);
}

bool TextMatchMarkerListImpl::MoveMarkers(int length,
                                          DocumentMarkerList* dst_list) {
  return DocumentMarkerListEditor::MoveMarkers(&markers_, length, dst_list);
}

bool TextMatchMarkerListImpl::RemoveMarkers(unsigned start_offset, int length) {
  return DocumentMarkerListEditor::RemoveMarkers(&markers_, start_offset,
                                                 length);
}

bool TextMatchMarkerListImpl::ShiftMarkers(unsigned offset,
                                           unsigned old_length,
                                           unsigned new_length) {
  return DocumentMarkerListEditor::ShiftMarkersContentDependent(
      &markers_, offset, old_length, new_length);
}

DEFINE_TRACE(TextMatchMarkerListImpl) {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

static void UpdateMarkerLayoutRect(const Node& node, TextMatchMarker& marker) {
  const Position start_position(node, marker.StartOffset());
  const Position end_position(node, marker.EndOffset());
  EphemeralRange range(start_position, end_position);
  marker.SetLayoutRect(LayoutRect(ComputeTextRect(range)));
}

Vector<IntRect> TextMatchMarkerListImpl::LayoutRects(const Node& node) const {
  Vector<IntRect> result;

  for (DocumentMarker* marker : markers_) {
    TextMatchMarker* const text_match_marker = ToTextMatchMarker(marker);
    if (!text_match_marker->IsValid())
      UpdateMarkerLayoutRect(node, *text_match_marker);
    if (!text_match_marker->IsRendered())
      continue;
    result.push_back(text_match_marker->GetLayoutRect());
  }

  return result;
}

bool TextMatchMarkerListImpl::SetTextMatchMarkersActive(unsigned start_offset,
                                                        unsigned end_offset,
                                                        bool active) {
  bool doc_dirty = false;
  const auto start = std::upper_bound(
      markers_.begin(), markers_.end(), start_offset,
      [](size_t start_offset, const Member<DocumentMarker>& marker) {
        return start_offset < marker->EndOffset();
      });
  for (auto it = start; it != markers_.end(); ++it) {
    DocumentMarker& marker = **it;
    // Markers are returned in order, so stop if we are now past the specified
    // range.
    if (marker.StartOffset() >= end_offset)
      break;
    ToTextMatchMarker(marker).SetIsActiveMatch(active);
    doc_dirty = true;
  }
  return doc_dirty;
}

}  // namespace blink
