// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/RelList.h"

#include "core/dom/Document.h"
#include "core/origin_trials/OriginTrials.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/HashMap.h"

namespace blink {

using namespace HTMLNames;

RelList::RelList(Element* element) : DOMTokenList(nullptr), element_(element) {}

unsigned RelList::length() const {
  return !element_->FastGetAttribute(relAttr).IsEmpty() ? rel_values_.size()
                                                        : 0;
}

const AtomicString RelList::item(unsigned index) const {
  if (index >= length())
    return AtomicString();
  return rel_values_[index];
}

bool RelList::ContainsInternal(const AtomicString& token) const {
  return !element_->FastGetAttribute(relAttr).IsEmpty() &&
         rel_values_.Contains(token);
}

void RelList::SetRelValues(const AtomicString& value) {
  rel_values_.Set(value, SpaceSplitString::kShouldNotFoldCase);
}

static HashSet<AtomicString>& SupportedTokens() {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tokens, ());

  if (tokens.IsEmpty()) {
    tokens = {
        "preload",
        "preconnect",
        "dns-prefetch",
        "stylesheet",
        "import",
        "icon",
        "alternate",
        "prefetch",
        "prerender",
        "next",
        "manifest",
        "apple-touch-icon",
        "apple-touch-icon-precomposed",
    };
  }

  return tokens;
}

bool RelList::ValidateTokenValue(const AtomicString& token_value,
                                 ExceptionState&) const {
  if (SupportedTokens().Contains(token_value))
    return true;
  return OriginTrials::linkServiceWorkerEnabled(
             element_->GetExecutionContext()) &&
         token_value == "serviceworker";
}

DEFINE_TRACE(RelList) {
  visitor->Trace(element_);
  DOMTokenList::Trace(visitor);
}

}  // namespace blink
