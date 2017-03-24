// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/ColdModeSpellCheckRequester.h"

#include "core/dom/Element.h"
#include "core/dom/IdleDeadline.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/spellcheck/SpellCheckRequester.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/LocalFrame.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

namespace {

const int kColdModeChunkSize = 16384;  // in UTF16 code units

bool shouldCheckNode(const Node& node) {
  if (!node.isElementNode())
    return false;
  // TODO(editing-dev): Make |Position| constructors take const parameters.
  const Position& position =
      Position::firstPositionInNode(const_cast<Node*>(&node));
  if (!isEditablePosition(position))
    return false;
  return SpellChecker::isSpellCheckingEnabledAt(position);
}

}  // namespace

ColdModeSpellCheckRequester::~ColdModeSpellCheckRequester() = default;

// static
ColdModeSpellCheckRequester* ColdModeSpellCheckRequester::create(
    LocalFrame& frame) {
  return new ColdModeSpellCheckRequester(frame);
}

DEFINE_TRACE(ColdModeSpellCheckRequester) {
  visitor->trace(m_frame);
  visitor->trace(m_nextNode);
}

ColdModeSpellCheckRequester::ColdModeSpellCheckRequester(LocalFrame& frame)
    : m_frame(frame),
      m_lastCheckedDOMTreeVersion(0),
      m_needsMoreInvocationForTesting(false) {}

bool ColdModeSpellCheckRequester::fullDocumentChecked() const {
  if (m_needsMoreInvocationForTesting) {
    m_needsMoreInvocationForTesting = false;
    return false;
  }
  return !m_nextNode;
}

SpellCheckRequester& ColdModeSpellCheckRequester::spellCheckRequester() const {
  return frame().spellChecker().spellCheckRequester();
}

// TODO(xiaochengh): Deduplicate with SpellChecker::chunkAndMarkAllMisspellings.
void ColdModeSpellCheckRequester::chunkAndRequestFullCheckingFor(
    const Element& editable) {
  const EphemeralRange& fullRange = EphemeralRange::rangeOfContents(editable);
  const int fullLength = TextIterator::rangeLength(fullRange.startPosition(),
                                                   fullRange.endPosition());

  // Check the full content if it is short.
  if (fullLength <= kColdModeChunkSize) {
    spellCheckRequester().requestCheckingFor(fullRange);
    return;
  }

  // TODO(xiaochengh): Figure out if this is going to cause performance issues.
  // In that case, we need finer-grained control over request generation.
  Position chunkStart = fullRange.startPosition();
  const int chunkLimit = fullLength / kColdModeChunkSize + 1;
  for (int chunkIndex = 0; chunkIndex <= chunkLimit; ++chunkIndex) {
    const Position& chunkEnd =
        calculateCharacterSubrange(
            EphemeralRange(chunkStart, fullRange.endPosition()), 0,
            kColdModeChunkSize)
            .endPosition();
    if (chunkEnd <= chunkStart)
      break;
    const EphemeralRange chunkRange(chunkStart, chunkEnd);
    const EphemeralRange& checkRange =
        chunkIndex >= 1 ? expandEndToSentenceBoundary(chunkRange)
                        : expandRangeToSentenceBoundary(chunkRange);

    spellCheckRequester().requestCheckingFor(checkRange, chunkIndex);

    chunkStart = checkRange.endPosition();
  }
}

void ColdModeSpellCheckRequester::invoke(IdleDeadline* deadline) {
  TRACE_EVENT0("blink", "ColdModeSpellCheckRequester::invoke");

  Node* body = frame().document()->body();
  if (!body) {
    m_nextNode = nullptr;
    m_lastCheckedDOMTreeVersion = frame().document()->domTreeVersion();
    return;
  }

  // TODO(xiaochengh): Figure out if this has any performance impact.
  frame().document()->updateStyleAndLayout();

  if (m_lastCheckedDOMTreeVersion != frame().document()->domTreeVersion())
    m_nextNode = body;

  // TODO(xiaochengh): Figure out if such frequent calls of |timeRemaining()|
  // have any performance impact. We might not want to check remaining time
  // so frequently in a page with millions of nodes.
  while (m_nextNode && deadline->timeRemaining() > 0) {
    if (!shouldCheckNode(*m_nextNode)) {
      m_nextNode = FlatTreeTraversal::next(*m_nextNode, body);
      continue;
    }

    chunkAndRequestFullCheckingFor(toElement(*m_nextNode));
    m_nextNode = FlatTreeTraversal::nextSkippingChildren(*m_nextNode, body);
  }

  m_lastCheckedDOMTreeVersion = frame().document()->domTreeVersion();
}

}  // namespace blink
