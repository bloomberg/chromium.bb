// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositionMarkerListImpl_h
#define CompositionMarkerListImpl_h

#include "core/editing/markers/DocumentMarkerList.h"

namespace blink {

// Implementation of DocumentMarkerList for Composition markers.
// Composition markers are always inserted in order, aside from some potential
// oddball cases (e.g. splitting the marker list into two nodes, then undoing
// the split). This means we can keep the list in sorted order to do some
// operations more efficiently, while still being able to do inserts in O(1)
// time at the end of the list.
class CORE_EXPORT CompositionMarkerListImpl final : public DocumentMarkerList {
 public:
  CompositionMarkerListImpl() = default;

  // DocumentMarkerList implementations
  bool IsEmpty() const final;

  void Add(DocumentMarker*) final;
  void Clear() final;

  const HeapVector<Member<RenderedDocumentMarker>>& GetMarkers() const final;

  bool MoveMarkers(int length, DocumentMarkerList* dst_list) final;
  bool RemoveMarkers(unsigned start_offset, int length) final;
  bool RemoveMarkersUnderWords(const String& node_text,
                               const Vector<String>& words) final;
  bool ShiftMarkers(unsigned offset,
                    unsigned old_length,
                    unsigned new_length) final;

  DECLARE_VIRTUAL_TRACE();

 private:
  HeapVector<Member<RenderedDocumentMarker>> markers_;

  DISALLOW_COPY_AND_ASSIGN(CompositionMarkerListImpl);
};

}  // namespace blink

#endif  // CompositionMarkerListImpl_h
