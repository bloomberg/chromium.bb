/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TEST_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TEST_HELPER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class Document;
class CSSStyleSheet;

// A helper class for writing tests of CSS rules. Example usage:
//
// CSSTestHelper helper;
// helper.addCSSRule("body { color: red} #a { position: absolute }");
// RuleSet& ruleSet = helper.ruleSet();
// ... examine RuleSet to find the rule and test properties on it.

class CSSTestHelper {
  STACK_ALLOCATED();

 public:
  CSSTestHelper();
  ~CSSTestHelper();

  const Document& GetDocument() { return *document_; };

  void AddCSSRules(const char* rule_text, bool is_empty_sheet = false);
  RuleSet& GetRuleSet();
  CSSRuleList* CssRules();

 private:
  Persistent<Document> document_;
  Persistent<CSSStyleSheet> style_sheet_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TEST_HELPER_H_
