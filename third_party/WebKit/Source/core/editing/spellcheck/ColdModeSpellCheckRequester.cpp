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
const int kInvalidLength = -1;
const int kInvalidChunkIndex = -1;

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

// static
ColdModeSpellCheckRequester* ColdModeSpellCheckRequester::create(
    LocalFrame& frame) {
  return new ColdModeSpellCheckRequester(frame);
}

DEFINE_TRACE(ColdModeSpellCheckRequester) {
  visitor->trace(m_frame);
  visitor->trace(m_nextNode);
  visitor->trace(m_currentRootEditable);
  visitor->trace(m_currentChunkStart);
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

void ColdModeSpellCheckRequester::invoke(IdleDeadline* deadline) {
  TRACE_EVENT0("blink", "ColdModeSpellCheckRequester::invoke");

  Node* body = frame().document()->body();
  if (!body) {
    resetCheckingProgress();
    m_lastCheckedDOMTreeVersion = frame().document()->domTreeVersion();
    return;
  }

  // TODO(xiaochengh): Figure out if this has any performance impact.
  frame().document()->updateStyleAndLayout();

  if (m_lastCheckedDOMTreeVersion != frame().document()->domTreeVersion())
    resetCheckingProgress();

  while (m_nextNode && deadline->timeRemaining() > 0)
    step();
  m_lastCheckedDOMTreeVersion = frame().document()->domTreeVersion();
}

void ColdModeSpellCheckRequester::resetCheckingProgress() {
  m_nextNode = frame().document()->body();
  m_currentRootEditable = nullptr;
  m_currentFullLength = kInvalidLength;
  m_currentChunkIndex = kInvalidChunkIndex;
  m_currentChunkStart = Position();
}

void ColdModeSpellCheckRequester::step() {
  if (!m_nextNode)
    return;

  if (!m_currentRootEditable) {
    searchForNextRootEditable();
    return;
  }

  if (m_currentFullLength == kInvalidLength) {
    initializeForCurrentRootEditable();
    return;
  }

  DCHECK(m_currentChunkIndex != kInvalidChunkIndex);
  requestCheckingForNextChunk();
}

void ColdModeSpellCheckRequester::searchForNextRootEditable() {
  // TODO(xiaochengh): Figure out if such small steps, which result in frequent
  // calls of |timeRemaining()|, have any performance impact. We might not want
  // to check remaining time so frequently in a page with millions of nodes.

  if (shouldCheckNode(*m_nextNode)) {
    m_currentRootEditable = toElement(m_nextNode);
    return;
  }

  m_nextNode = FlatTreeTraversal::next(*m_nextNode, frame().document()->body());
}

void ColdModeSpellCheckRequester::initializeForCurrentRootEditable() {
  const EphemeralRange& fullRange =
      EphemeralRange::rangeOfContents(*m_currentRootEditable);
  m_currentFullLength = TextIterator::rangeLength(fullRange.startPosition(),
                                                  fullRange.endPosition());

  m_currentChunkIndex = 0;
  m_currentChunkStart = fullRange.startPosition();
}

void ColdModeSpellCheckRequester::requestCheckingForNextChunk() {
  // Check the full content if it is short.
  if (m_currentFullLength <= kColdModeChunkSize) {
    spellCheckRequester().requestCheckingFor(
        EphemeralRange::rangeOfContents(*m_currentRootEditable));
    finishCheckingCurrentRootEditable();
    return;
  }

  const Position& chunkEnd =
      calculateCharacterSubrange(
          EphemeralRange(m_currentChunkStart,
                         Position::lastPositionInNode(m_currentRootEditable)),
          0, kColdModeChunkSize)
          .endPosition();
  if (chunkEnd <= m_currentChunkStart) {
    finishCheckingCurrentRootEditable();
    return;
  }
  const EphemeralRange chunkRange(m_currentChunkStart, chunkEnd);
  const EphemeralRange& checkRange = expandEndToSentenceBoundary(chunkRange);
  spellCheckRequester().requestCheckingFor(checkRange, m_currentChunkIndex);

  m_currentChunkStart = checkRange.endPosition();
  ++m_currentChunkIndex;

  if (m_currentChunkIndex * kColdModeChunkSize >= m_currentFullLength)
    finishCheckingCurrentRootEditable();
}

void ColdModeSpellCheckRequester::finishCheckingCurrentRootEditable() {
  DCHECK_EQ(m_nextNode, m_currentRootEditable);
  m_nextNode = FlatTreeTraversal::nextSkippingChildren(
      *m_nextNode, frame().document()->body());

  m_currentRootEditable = nullptr;
  m_currentFullLength = kInvalidLength;
  m_currentChunkIndex = kInvalidChunkIndex;
  m_currentChunkStart = Position();
}

}  // namespace blink
