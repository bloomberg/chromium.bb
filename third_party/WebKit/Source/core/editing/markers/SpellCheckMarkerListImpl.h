// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SpellCheckMarkerListImpl_h
#define SpellCheckMarkerListImpl_h

#include "core/editing/markers/DocumentMarkerList.h"

namespace blink {

// Implementation of DocumentMarkerList for Spelling/Grammar markers.
// Markers with touching endpoints are merged on insert. Markers are kept sorted
// by start offset in order to be able to do this efficiently.
class CORE_EXPORT SpellCheckMarkerListImpl : public DocumentMarkerList {
 public:
  SpellCheckMarkerListImpl() = default;

  // DocumentMarkerList implementations
  bool IsEmpty() const final;

  void Add(DocumentMarker*) final;
  void Clear() final;

  const HeapVector<Member<RenderedDocumentMarker>>& GetMarkers() const final;

  bool MoveMarkers(int length, DocumentMarkerList* dst_list) final;
  bool RemoveMarkers(unsigned start_offset, int length) final;
  bool RemoveMarkersUnderWords(const String& node_text,
                               const Vector<String>& words);
  bool ShiftMarkers(unsigned offset,
                    unsigned old_length,
                    unsigned new_length) final;

  DECLARE_VIRTUAL_TRACE();

 private:
  HeapVector<Member<RenderedDocumentMarker>> markers_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckMarkerListImpl);
};

}  // namespace blink

#endif  // SpellCheckMarkerListImpl_h
