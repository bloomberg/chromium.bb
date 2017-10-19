// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositionMarkerListImpl_h
#define CompositionMarkerListImpl_h

#include "core/editing/markers/DocumentMarkerList.h"

namespace blink {

// Implementation of DocumentMarkerList for Composition markers. Composition
// markers can overlap (e.g. an IME might pass two markers on the same region of
// text, one to underline it and one to add a background color), so we store
// them unsorted.
class CORE_EXPORT CompositionMarkerListImpl final : public DocumentMarkerList {
 public:
  CompositionMarkerListImpl() = default;

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

  virtual void Trace(blink::Visitor*);

 private:
  HeapVector<Member<DocumentMarker>> markers_;

  DISALLOW_COPY_AND_ASSIGN(CompositionMarkerListImpl);
};

}  // namespace blink

#endif  // CompositionMarkerListImpl_h
