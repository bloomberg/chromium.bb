// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/TextMatchMarkerListImpl.h"

#include "core/dom/Node.h"
#include "core/dom/Range.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/markers/DocumentMarkerListEditor.h"
#include "core/editing/markers/RenderedDocumentMarker.h"
#include "third_party/WebKit/Source/core/editing/VisibleUnits.h"

namespace blink {

DocumentMarker::MarkerType TextMatchMarkerListImpl::MarkerType() const {
  return DocumentMarker::kTextMatch;
}

bool TextMatchMarkerListImpl::IsEmpty() const {
  return markers_.IsEmpty();
}

void TextMatchMarkerListImpl::Add(DocumentMarker* marker) {
  DocumentMarkerListEditor::AddMarkerWithoutMergingOverlapping(
      &markers_, RenderedDocumentMarker::Create(*marker));
}

void TextMatchMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>& TextMatchMarkerListImpl::GetMarkers()
    const {
  return markers_;
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

static void UpdateMarkerRenderedRect(const Node& node,
                                     RenderedDocumentMarker& marker) {
  const Position start_position(&const_cast<Node&>(node), marker.StartOffset());
  const Position end_position(&const_cast<Node&>(node), marker.EndOffset());
  EphemeralRange range(start_position, end_position);
  marker.SetRenderedRect(LayoutRect(ComputeTextRect(range)));
}

Vector<IntRect> TextMatchMarkerListImpl::RenderedRects(const Node& node) const {
  Vector<IntRect> result;

  for (DocumentMarker* marker : markers_) {
    RenderedDocumentMarker* const rendered_marker =
        ToRenderedDocumentMarker(marker);
    if (!rendered_marker->IsValid())
      UpdateMarkerRenderedRect(node, *rendered_marker);
    if (!rendered_marker->IsRendered())
      continue;
    result.push_back(rendered_marker->RenderedRect());
  }

  return result;
}

}  // namespace blink
