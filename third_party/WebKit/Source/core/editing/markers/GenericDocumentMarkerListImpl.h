// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GenericDocumentMarkerListImpl_h
#define GenericDocumentMarkerListImpl_h

#include "core/editing/markers/DocumentMarkerList.h"

namespace blink {

class RenderedDocumentMarker;

// Temporary implementation of DocumentMarkerList that can handle
// DocumentMarkers of all MarkerTypes. This will be removed once we have
// specialized implementations for every MarkerType.
class GenericDocumentMarkerListImpl final : public DocumentMarkerList {
 public:
  GenericDocumentMarkerListImpl() = default;

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

  DECLARE_TRACE();

 private:
  HeapVector<Member<RenderedDocumentMarker>> markers_;

  DISALLOW_COPY_AND_ASSIGN(GenericDocumentMarkerListImpl);
};

}  // namespace blink

#endif  // GenericDocumentMarkerListImpl_h
