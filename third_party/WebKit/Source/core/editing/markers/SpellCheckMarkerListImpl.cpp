// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/SpellCheckMarkerListImpl.h"

#include "core/editing/markers/DocumentMarkerListEditor.h"
#include "core/editing/markers/RenderedDocumentMarker.h"

namespace blink {

bool SpellCheckMarkerListImpl::IsEmpty() const {
  return markers_.IsEmpty();
}

void SpellCheckMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<RenderedDocumentMarker>>&
SpellCheckMarkerListImpl::GetMarkers() const {
  return markers_;
}

bool SpellCheckMarkerListImpl::MoveMarkers(int length,
                                           DocumentMarkerList* dst_list) {
  return DocumentMarkerListEditor::MoveMarkers(&markers_, length, dst_list);
}

bool SpellCheckMarkerListImpl::RemoveMarkers(unsigned start_offset,
                                             int length) {
  return DocumentMarkerListEditor::RemoveMarkers(&markers_, start_offset,
                                                 length);
}

bool SpellCheckMarkerListImpl::RemoveMarkersUnderWords(
    const String& node_text,
    const Vector<String>& words) {
  return DocumentMarkerListEditor::RemoveMarkersUnderWords(&markers_, node_text,
                                                           words);
}

bool SpellCheckMarkerListImpl::ShiftMarkers(unsigned offset,
                                            unsigned old_length,
                                            unsigned new_length) {
  return DocumentMarkerListEditor::ShiftMarkers(&markers_, offset, old_length,
                                                new_length);
}

DEFINE_TRACE(SpellCheckMarkerListImpl) {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

}  // namespace blink
