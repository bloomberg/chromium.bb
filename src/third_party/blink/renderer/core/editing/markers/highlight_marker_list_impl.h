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

#ifndef HighlightMarkerListImpl_h
#define HighlightMarkerListImpl_h

#include "third_party/blink/renderer/core/editing/markers/document_marker_list.h"

namespace blink {

// Implementation of DocumentMarkerList for Highlight markers.
// Markers are kept sorted by start offset, under the assumption that
// Highlight markers are typically inserted in an order.
class CORE_EXPORT HighlightMarkerListImpl final : public DocumentMarkerList {
 public:
  HighlightMarkerListImpl() = default;

  // DocumentMarkerList implementations
  DocumentMarker::MarkerType MarkerType() const final;

  bool IsEmpty() const final;

  void Add(DocumentMarker*) final;
  void Clear() final;

  const HeapVector<Member<DocumentMarker>>& GetMarkers() const final;
  DocumentMarker* FirstMarkerIntersectingRange(unsigned start_offset,
                                               unsigned end_offset) const final;
  HeapVector<Member<DocumentMarker>> MarkersIntersectingRange(
      unsigned start_offset,
      unsigned end_offset) const final;

  bool MoveMarkers(int length, DocumentMarkerList* dst_list) final;
  bool RemoveMarkers(unsigned start_offset, int length) final;
  bool ShiftMarkers(const String& node_text,
                    unsigned offset,
                    unsigned old_length,
                    unsigned new_length) final;

  void Trace(blink::Visitor*) override;

 private:
  HeapVector<Member<DocumentMarker>> markers_;

  DISALLOW_COPY_AND_ASSIGN(HighlightMarkerListImpl);
};

DEFINE_TYPE_CASTS(HighlightMarkerListImpl,
                  DocumentMarkerList,
                  list,
                  list->MarkerType() == DocumentMarker::kHighlight,
                  list.MarkerType() == DocumentMarker::kHighlight);

}  // namespace blink

#endif  // HighlightMarkerListImpl_h
