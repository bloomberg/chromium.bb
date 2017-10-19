/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ViewportStyleResolver_h
#define ViewportStyleResolver_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/RuleSet.h"
#include "platform/Length.h"

namespace blink {

class Document;
class DocumentStyleSheetCollection;
class MutableStylePropertySet;
class StyleRuleViewport;

class CORE_EXPORT ViewportStyleResolver
    : public GarbageCollectedFinalized<ViewportStyleResolver> {
 public:
  static ViewportStyleResolver* Create(Document& document) {
    return new ViewportStyleResolver(document);
  }

  void InitialViewportChanged();
  void SetNeedsCollectRules();
  bool NeedsUpdate() const { return needs_update_; }
  void UpdateViewport(DocumentStyleSheetCollection&);

  void CollectViewportRulesFromAuthorSheet(const CSSStyleSheet&);

  void Trace(blink::Visitor*);

 private:
  explicit ViewportStyleResolver(Document&);

  void Reset();
  void Resolve();

  enum Origin { kUserAgentOrigin, kAuthorOrigin };
  enum UpdateType { kNoUpdate, kResolve, kCollectRules };

  void CollectViewportRulesFromUASheets();
  void CollectViewportChildRules(const HeapVector<Member<StyleRuleBase>>&,
                                 Origin);
  void CollectViewportRulesFromImports(StyleSheetContents&);
  void CollectViewportRulesFromAuthorSheetContents(StyleSheetContents&);
  void AddViewportRule(StyleRuleViewport&, Origin);

  float ViewportArgumentValue(CSSPropertyID) const;
  Length ViewportLengthValue(CSSPropertyID);

  Member<Document> document_;
  Member<MutableStylePropertySet> property_set_;
  Member<MediaQueryEvaluator> initial_viewport_medium_;
  MediaQueryResultList viewport_dependent_media_query_results_;
  MediaQueryResultList device_dependent_media_query_results_;
  bool has_author_style_ = false;
  bool has_viewport_units_ = false;
  UpdateType needs_update_ = kCollectRules;
};

}  // namespace blink

#endif  // ViewportStyleResolver_h
