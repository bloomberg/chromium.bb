// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IncrementLoadEventDelayCount.h"

#include "core/dom/Document.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<IncrementLoadEventDelayCount>
IncrementLoadEventDelayCount::create(Document& document) {
  return WTF::wrapUnique(new IncrementLoadEventDelayCount(document));
}

IncrementLoadEventDelayCount::IncrementLoadEventDelayCount(Document& document)
    : m_document(&document) {
  document.incrementLoadEventDelayCount();
}

IncrementLoadEventDelayCount::~IncrementLoadEventDelayCount() {
  if (m_document)
    m_document->decrementLoadEventDelayCount();
}

void IncrementLoadEventDelayCount::clearAndCheckLoadEvent() {
  m_document->decrementLoadEventDelayCountAndCheckLoadEvent();
  m_document = nullptr;
}

void IncrementLoadEventDelayCount::documentChanged(Document& newDocument) {
  newDocument.incrementLoadEventDelayCount();
  if (m_document)
    m_document->decrementLoadEventDelayCount();
  m_document = &newDocument;
}
}  // namespace blink
