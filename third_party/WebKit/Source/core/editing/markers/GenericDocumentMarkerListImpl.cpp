// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/GenericDocumentMarkerListImpl.h"

#include "core/editing/markers/DocumentMarkerListEditor.h"
#include "core/editing/markers/RenderedDocumentMarker.h"

namespace blink {

bool GenericDocumentMarkerListImpl::IsEmpty() const {
  return markers_.IsEmpty();
}

void GenericDocumentMarkerListImpl::Add(DocumentMarker* marker) {
  switch (marker->GetType()) {
    case DocumentMarker::kSpelling:
    case DocumentMarker::kGrammar:
      DocumentMarkerListEditor::AddMarkerAndMergeOverlapping(&markers_, marker);
      return;
    case DocumentMarker::kTextMatch:
      DocumentMarkerListEditor::AddMarkerWithoutMergingOverlapping(&markers_,
                                                                   marker);
      return;
    case DocumentMarker::kComposition:
      NOTREACHED();
  }

  NOTREACHED() << "Unhanded marker type: " << marker->GetType();
  return;
}

void GenericDocumentMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<RenderedDocumentMarker>>&
GenericDocumentMarkerListImpl::GetMarkers() const {
  return markers_;
}

bool GenericDocumentMarkerListImpl::MoveMarkers(int length,
                                                DocumentMarkerList* dst_list) {
  return DocumentMarkerListEditor::MoveMarkers(&markers_, length, dst_list);
}

bool GenericDocumentMarkerListImpl::RemoveMarkers(unsigned start_offset,
                                                  int length) {
  return DocumentMarkerListEditor::RemoveMarkers(&markers_, start_offset,
                                                 length);
}

bool GenericDocumentMarkerListImpl::RemoveMarkersUnderWords(
    const String& node_text,
    const Vector<String>& words) {
  return DocumentMarkerListEditor::RemoveMarkersUnderWords(&markers_, node_text,
                                                           words);
}

bool GenericDocumentMarkerListImpl::ShiftMarkers(unsigned offset,
                                                 unsigned old_length,
                                                 unsigned new_length) {
  return DocumentMarkerListEditor::ShiftMarkers(&markers_, offset, old_length,
                                                new_length);
}

DEFINE_TRACE(GenericDocumentMarkerListImpl) {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

}  // namespace blink
