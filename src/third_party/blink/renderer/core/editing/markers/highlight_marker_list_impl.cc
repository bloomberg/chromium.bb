/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "third_party/blink/renderer/core/editing/markers/highlight_marker_list_impl.h"

#include "third_party/blink/renderer/core/editing/markers/unsorted_document_marker_list_editor.h"

namespace blink {

DocumentMarker::MarkerType HighlightMarkerListImpl::MarkerType() const {
  return DocumentMarker::kHighlight;
}

bool HighlightMarkerListImpl::IsEmpty() const {
  return markers_.IsEmpty();
}

void HighlightMarkerListImpl::Add(DocumentMarker* marker) {
  DCHECK_EQ(DocumentMarker::kHighlight, marker->GetType());
  markers_.push_back(marker);
}

void HighlightMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>&
HighlightMarkerListImpl::GetMarkers() const {
  return markers_;
}

DocumentMarker* HighlightMarkerListImpl::FirstMarkerIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  return UnsortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
      markers_, start_offset, end_offset);
}

HeapVector<Member<DocumentMarker>>
HighlightMarkerListImpl::MarkersIntersectingRange(unsigned start_offset,
                                                    unsigned end_offset) const {
  return UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(
      markers_, start_offset, end_offset);
}

bool HighlightMarkerListImpl::MoveMarkers(int length,
                                            DocumentMarkerList* dst_markers_) {
  return UnsortedDocumentMarkerListEditor::MoveMarkers(&markers_, length,
                                                       dst_markers_);
}

bool HighlightMarkerListImpl::RemoveMarkers(unsigned start_offset,
                                              int length) {
  return UnsortedDocumentMarkerListEditor::RemoveMarkers(&markers_,
                                                         start_offset, length);
}

bool HighlightMarkerListImpl::ShiftMarkers(const String&,
                                             unsigned offset,
                                             unsigned old_length,
                                             unsigned new_length) {
  return UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(
      &markers_, offset, old_length, new_length);
}

void HighlightMarkerListImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

}  // namespace blink
