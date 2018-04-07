// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_COLD_MODE_SPELL_CHECK_REQUESTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_COLD_MODE_SPELL_CHECK_REQUESTER_H_

#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Element;
class LocalFrame;
class IdleDeadline;
class SpellCheckRequester;

// This class is only supposed to be used by IdleSpellCheckCallback in cold mode
// invocation. Not to be confused with SpellCheckRequester.
class ColdModeSpellCheckRequester
    : public GarbageCollected<ColdModeSpellCheckRequester> {
 public:
  static ColdModeSpellCheckRequester* Create(LocalFrame&);

  void SetNeedsMoreInvocationForTesting() {
    needs_more_invocation_for_testing_ = true;
  }

  void Invoke(IdleDeadline*);
  bool FullDocumentChecked() const;

  void Trace(blink::Visitor*);

 private:
  explicit ColdModeSpellCheckRequester(LocalFrame&);

  LocalFrame& GetFrame() const { return *frame_; }
  SpellCheckRequester& GetSpellCheckRequester() const;

  // Perform checking task incrementally based on the stored state.
  void Step();

  void SearchForNextRootEditable();
  void InitializeForCurrentRootEditable();
  bool HaveMoreChunksToCheck();
  void RequestCheckingForNextChunk();
  void FinishCheckingCurrentRootEditable();

  void ResetCheckingProgress();
  void ChunkAndRequestFullCheckingFor(const Element&);

  const Member<LocalFrame> frame_;
  Member<Node> next_node_;
  Member<Element> current_root_editable_;
  int current_full_length_;
  int current_chunk_index_;
  Position current_chunk_start_;
  uint64_t last_checked_dom_tree_version_;
  mutable bool needs_more_invocation_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(ColdModeSpellCheckRequester);
};
}

#endif
