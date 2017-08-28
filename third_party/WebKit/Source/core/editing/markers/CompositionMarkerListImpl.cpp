// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/CompositionMarkerListImpl.h"

#include "core/editing/markers/SortedDocumentMarkerListEditor.h"

namespace blink {

DocumentMarker::MarkerType CompositionMarkerListImpl::MarkerType() const {
  return DocumentMarker::kComposition;
}

bool CompositionMarkerListImpl::IsEmpty() const {
  return markers_.IsEmpty();
}

void CompositionMarkerListImpl::Add(DocumentMarker* marker) {
  SortedDocumentMarkerListEditor::AddMarkerWithoutMergingOverlapping(&markers_,
                                                                     marker);
}

void CompositionMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>&
CompositionMarkerListImpl::GetMarkers() const {
  return markers_;
}

DocumentMarker* CompositionMarkerListImpl::FirstMarkerIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  return SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
      markers_, start_offset, end_offset);
}

HeapVector<Member<DocumentMarker>>
CompositionMarkerListImpl::MarkersIntersectingRange(unsigned start_offset,
                                                    unsigned end_offset) const {
  return SortedDocumentMarkerListEditor::MarkersIntersectingRange(
      markers_, start_offset, end_offset);
}

bool CompositionMarkerListImpl::MoveMarkers(int length,
                                            DocumentMarkerList* dst_markers_) {
  return SortedDocumentMarkerListEditor::MoveMarkers(&markers_, length,
                                                     dst_markers_);
}

bool CompositionMarkerListImpl::RemoveMarkers(unsigned start_offset,
                                              int length) {
  return SortedDocumentMarkerListEditor::RemoveMarkers(&markers_, start_offset,
                                                       length);
}

bool CompositionMarkerListImpl::ShiftMarkers(const String&,
                                             unsigned offset,
                                             unsigned old_length,
                                             unsigned new_length) {
  return SortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(
      &markers_, offset, old_length, new_length);
}

DEFINE_TRACE(CompositionMarkerListImpl) {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

}  // namespace blink
