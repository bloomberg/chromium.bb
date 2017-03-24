// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ColdModeSpellCheckRequester_h
#define ColdModeSpellCheckRequester_h

#include "platform/heap/Handle.h"

namespace blink {

class Element;
class IdleDeadline;
class LocalFrame;
class Node;
class SpellCheckRequester;

// This class is only supposed to be used by IdleSpellCheckCallback in cold mode
// invocation. Not to be confused with SpellCheckRequester.
class ColdModeSpellCheckRequester
    : public GarbageCollectedFinalized<ColdModeSpellCheckRequester> {
 public:
  static ColdModeSpellCheckRequester* create(LocalFrame&);
  ~ColdModeSpellCheckRequester();

  void setNeedsMoreInvocationForTesting() {
    m_needsMoreInvocationForTesting = true;
  }

  void invoke(IdleDeadline*);
  bool fullDocumentChecked() const;

 private:
  explicit ColdModeSpellCheckRequester(LocalFrame&);

  LocalFrame& frame() const { return *m_frame; }
  SpellCheckRequester& spellCheckRequester() const;

  void chunkAndRequestFullCheckingFor(const Element&);

  const Member<LocalFrame> m_frame;
  Member<Node> m_nextNode;
  uint64_t m_lastCheckedDOMTreeVersion;
  mutable bool m_needsMoreInvocationForTesting;

  DECLARE_TRACE();
  DISALLOW_COPY_AND_ASSIGN(ColdModeSpellCheckRequester);
};
}

#endif
