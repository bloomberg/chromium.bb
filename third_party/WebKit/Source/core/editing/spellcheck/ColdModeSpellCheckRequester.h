// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ColdModeSpellCheckRequester_h
#define ColdModeSpellCheckRequester_h

#include "core/editing/EphemeralRange.h"
#include "platform/heap/Handle.h"

namespace blink {

class IdleDeadline;
class SpellCheckRequester;

// This class is only supposed to be used by IdleSpellCheckCallback in cold mode
// invocation. Not to be confused with SpellCheckRequester.
class ColdModeSpellCheckRequester
    : public GarbageCollected<ColdModeSpellCheckRequester> {
 public:
  static ColdModeSpellCheckRequester* create(LocalFrame&);

  void setNeedsMoreInvocationForTesting() {
    m_needsMoreInvocationForTesting = true;
  }

  void invoke(IdleDeadline*);
  bool fullDocumentChecked() const;

  DECLARE_TRACE();

 private:
  explicit ColdModeSpellCheckRequester(LocalFrame&);

  LocalFrame& frame() const { return *m_frame; }
  SpellCheckRequester& spellCheckRequester() const;

  // Perform checking task incrementally based on the stored state.
  void step();

  void searchForNextRootEditable();
  void initializeForCurrentRootEditable();
  bool haveMoreChunksToCheck();
  void requestCheckingForNextChunk();
  void finishCheckingCurrentRootEditable();

  void resetCheckingProgress();
  void chunkAndRequestFullCheckingFor(const Element&);

  const Member<LocalFrame> m_frame;
  Member<Node> m_nextNode;
  Member<Element> m_currentRootEditable;
  int m_currentFullLength;
  int m_currentChunkIndex;
  Position m_currentChunkStart;
  uint64_t m_lastCheckedDOMTreeVersion;
  mutable bool m_needsMoreInvocationForTesting;

  DISALLOW_COPY_AND_ASSIGN(ColdModeSpellCheckRequester);
};
}

#endif
