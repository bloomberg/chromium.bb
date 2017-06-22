// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextMatchMarkerListImpl_h
#define TextMatchMarkerListImpl_h

#include "core/editing/markers/DocumentMarkerList.h"

namespace blink {

class IntRect;

// Implementation of DocumentMarkerList for TextMatch markers.
// Markers are kept sorted by start offset, under the assumption that
// TextMatch markers are typically inserted in an order.
class CORE_EXPORT TextMatchMarkerListImpl final : public DocumentMarkerList {
 public:
  TextMatchMarkerListImpl() = default;

  // DocumentMarkerList implementations
  DocumentMarker::MarkerType MarkerType() const final;

  bool IsEmpty() const final;

  void Add(DocumentMarker*) final;
  void Clear() final;

  const HeapVector<Member<DocumentMarker>>& GetMarkers() const final;
  HeapVector<Member<DocumentMarker>> MarkersIntersectingRange(
      unsigned start_offset,
      unsigned end_offset) const final;

  bool MoveMarkers(int length, DocumentMarkerList* dst_list) final;
  bool RemoveMarkers(unsigned start_offset, int length) final;
  bool ShiftMarkers(unsigned offset,
                    unsigned old_length,
                    unsigned new_length) final;
  DECLARE_VIRTUAL_TRACE();

  // TextMatchMarkerListImpl-specific
  Vector<IntRect> LayoutRects(const Node&) const;
  // Returns true if markers within a range defined by |startOffset| and
  // |endOffset| are found.
  bool SetTextMatchMarkersActive(unsigned start_offset,
                                 unsigned end_offset,
                                 bool);

 private:
  HeapVector<Member<DocumentMarker>> markers_;

  DISALLOW_COPY_AND_ASSIGN(TextMatchMarkerListImpl);
};

DEFINE_TYPE_CASTS(TextMatchMarkerListImpl,
                  DocumentMarkerList,
                  list,
                  list->MarkerType() == DocumentMarker::kTextMatch,
                  list.MarkerType() == DocumentMarker::kTextMatch);

}  // namespace blink

#endif  // TextMatchMarkerListImpl_h
