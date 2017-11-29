// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/RelList.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/html_names.h"
#include "core/origin_trials/origin_trials.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/HashMap.h"

namespace blink {

RelList::RelList(Element* element)
    : DOMTokenList(*element, HTMLNames::relAttr) {}

static HashSet<AtomicString>& SupportedTokens() {
  DEFINE_STATIC_LOCAL(
      HashSet<AtomicString>, tokens,
      ({
          "preload", "preconnect", "dns-prefetch", "stylesheet", "import",
          "icon", "alternate", "prefetch", "prerender", "next", "manifest",
          "apple-touch-icon", "apple-touch-icon-precomposed", "canonical",
      }));

  return tokens;
}

bool RelList::ValidateTokenValue(const AtomicString& token_value,
                                 ExceptionState&) const {
  if (SupportedTokens().Contains(token_value))
    return true;
  if (RuntimeEnabledFeatures::ModulePreloadEnabled() &&
      token_value == "modulepreload")
    return true;
  return false;
}

}  // namespace blink
