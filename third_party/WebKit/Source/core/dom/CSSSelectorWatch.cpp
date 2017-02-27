/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/CSSSelectorWatch.h"

#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/style/StyleRareNonInheritedData.h"

namespace blink {

// The address of this string is important; its value is just documentation.
static const char kSupplementName[] = "CSSSelectorWatch";

CSSSelectorWatch::CSSSelectorWatch(Document& document)
    : Supplement<Document>(document),
      m_callbackSelectorChangeTimer(
          TaskRunnerHelper::get(TaskType::UnspecedTimer, &document),
          this,
          &CSSSelectorWatch::callbackSelectorChangeTimerFired),
      m_timerExpirations(0) {}

CSSSelectorWatch& CSSSelectorWatch::from(Document& document) {
  CSSSelectorWatch* watch = fromIfExists(document);
  if (!watch) {
    watch = new CSSSelectorWatch(document);
    Supplement<Document>::provideTo(document, kSupplementName, watch);
  }
  return *watch;
}

CSSSelectorWatch* CSSSelectorWatch::fromIfExists(Document& document) {
  return static_cast<CSSSelectorWatch*>(
      Supplement<Document>::from(document, kSupplementName));
}

void CSSSelectorWatch::callbackSelectorChangeTimerFired(TimerBase*) {
  // Should be ensured by updateSelectorMatches():
  DCHECK(!m_addedSelectors.isEmpty() || !m_removedSelectors.isEmpty());

  if (m_timerExpirations < 1) {
    m_timerExpirations++;
    m_callbackSelectorChangeTimer.startOneShot(0, BLINK_FROM_HERE);
    return;
  }
  if (supplementable()->frame()) {
    Vector<String> addedSelectors;
    Vector<String> removedSelectors;
    copyToVector(m_addedSelectors, addedSelectors);
    copyToVector(m_removedSelectors, removedSelectors);
    supplementable()->frame()->loader().client()->selectorMatchChanged(
        addedSelectors, removedSelectors);
  }
  m_addedSelectors.clear();
  m_removedSelectors.clear();
  m_timerExpirations = 0;
}

void CSSSelectorWatch::updateSelectorMatches(
    const Vector<String>& removedSelectors,
    const Vector<String>& addedSelectors) {
  bool shouldUpdateTimer = false;

  for (const auto& selector : removedSelectors) {
    if (!m_matchingCallbackSelectors.remove(selector))
      continue;

    // Count reached 0.
    shouldUpdateTimer = true;
    auto it = m_addedSelectors.find(selector);
    if (it != m_addedSelectors.end())
      m_addedSelectors.erase(it);
    else
      m_removedSelectors.insert(selector);
  }

  for (const auto& selector : addedSelectors) {
    HashCountedSet<String>::AddResult result =
        m_matchingCallbackSelectors.add(selector);
    if (!result.isNewEntry)
      continue;

    shouldUpdateTimer = true;
    auto it = m_removedSelectors.find(selector);
    if (it != m_removedSelectors.end())
      m_removedSelectors.erase(it);
    else
      m_addedSelectors.insert(selector);
  }

  if (!shouldUpdateTimer)
    return;

  if (m_removedSelectors.isEmpty() && m_addedSelectors.isEmpty()) {
    if (m_callbackSelectorChangeTimer.isActive()) {
      m_timerExpirations = 0;
      m_callbackSelectorChangeTimer.stop();
    }
  } else {
    m_timerExpirations = 0;
    if (!m_callbackSelectorChangeTimer.isActive())
      m_callbackSelectorChangeTimer.startOneShot(0, BLINK_FROM_HERE);
  }
}

static bool allCompound(const CSSSelectorList& selectorList) {
  for (const CSSSelector* selector = selectorList.first(); selector;
       selector = selectorList.next(*selector)) {
    if (!selector->isCompound())
      return false;
  }
  return true;
}

void CSSSelectorWatch::watchCSSSelectors(const Vector<String>& selectors) {
  m_watchedCallbackSelectors.clear();

  StylePropertySet* callbackPropertySet =
      ImmutableStylePropertySet::create(nullptr, 0, UASheetMode);

  CSSParserContext* context = CSSParserContext::create(UASheetMode);
  for (const auto& selector : selectors) {
    CSSSelectorList selectorList =
        CSSParser::parseSelector(context, nullptr, selector);
    if (!selectorList.isValid())
      continue;

    // Only accept Compound Selectors, since they're cheaper to match.
    if (!allCompound(selectorList))
      continue;

    m_watchedCallbackSelectors.push_back(
        StyleRule::create(std::move(selectorList), callbackPropertySet));
  }
  supplementable()->styleEngine().watchedSelectorsChanged();
}

DEFINE_TRACE(CSSSelectorWatch) {
  visitor->trace(m_watchedCallbackSelectors);
  Supplement<Document>::trace(visitor);
}

}  // namespace blink
