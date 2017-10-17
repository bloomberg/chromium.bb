// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IncrementLoadEventDelayCount.h"

#include <memory>
#include "core/dom/Document.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

std::unique_ptr<IncrementLoadEventDelayCount>
IncrementLoadEventDelayCount::Create(Document& document) {
  return WTF::WrapUnique(new IncrementLoadEventDelayCount(document));
}

IncrementLoadEventDelayCount::IncrementLoadEventDelayCount(Document& document)
    : document_(&document) {
  document.IncrementLoadEventDelayCount();
}

IncrementLoadEventDelayCount::~IncrementLoadEventDelayCount() {
  if (document_)
    document_->DecrementLoadEventDelayCount();
}

void IncrementLoadEventDelayCount::ClearAndCheckLoadEvent() {
  if (document_)
    document_->DecrementLoadEventDelayCountAndCheckLoadEvent();
  document_ = nullptr;
}

void IncrementLoadEventDelayCount::DocumentChanged(Document& new_document) {
  new_document.IncrementLoadEventDelayCount();
  if (document_)
    document_->DecrementLoadEventDelayCount();
  document_ = &new_document;
}
}  // namespace blink
