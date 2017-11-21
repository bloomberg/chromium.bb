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

#include "public/web/WebSelector.h"

#include "core/css/CSSSelectorList.h"
#include "core/css/parser/CSSParser.h"
#include "public/platform/WebString.h"

namespace blink {

WebString CanonicalizeSelector(WebString web_selector,
                               WebSelectorType restriction) {
  // NOTE: We will always parse the selector in an insecure context mode, if we
  // have selectors which are only parsed in secure contexts, this will need to
  // accept a SecureContextMode as an argument.
  CSSSelectorList selector_list = CSSParser::ParseSelector(
      StrictCSSParserContext(SecureContextMode::kInsecureContext), nullptr,
      web_selector);

  if (restriction == kWebSelectorTypeCompound) {
    for (const CSSSelector* selector = selector_list.First(); selector;
         selector = selector_list.Next(*selector)) {
      if (!selector->IsCompound())
        return WebString();
    }
  }
  return selector_list.SelectorsText();
}

}  // namespace blink
