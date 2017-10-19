/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSPreloadScanner_h
#define CSSPreloadScanner_h

#include "core/html/parser/HTMLToken.h"
#include "core/html/parser/PreloadRequest.h"
#include "core/loader/resource/CSSStyleSheetResource.h"
#include "core/loader/resource/StyleSheetResourceClient.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceOwner.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

class SegmentedString;
class HTMLResourcePreloader;

class CSSPreloadScanner {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(CSSPreloadScanner);

 public:
  CSSPreloadScanner();
  ~CSSPreloadScanner();

  void Reset();

  void Scan(const HTMLToken::DataVector&,
            const SegmentedString&,
            PreloadRequestStream&,
            const KURL&);
  void Scan(const String&,
            const SegmentedString&,
            PreloadRequestStream&,
            const KURL&);

  void SetReferrerPolicy(const ReferrerPolicy);

 private:
  enum State {
    kInitial,
    kMaybeComment,
    kComment,
    kMaybeCommentEnd,
    kRuleStart,
    kRule,
    kAfterRule,
    kRuleValue,
    kAfterRuleValue,
    kDoneParsingImportRules,
  };

  template <typename Char>
  void ScanCommon(const Char* begin,
                  const Char* end,
                  const SegmentedString&,
                  PreloadRequestStream&,
                  const KURL&);

  inline void Tokenize(UChar, const SegmentedString&);
  void EmitRule(const SegmentedString&);

  State state_ = kInitial;
  StringBuilder rule_;
  StringBuilder rule_value_;

  ReferrerPolicy referrer_policy_ = kReferrerPolicyDefault;

  // Below members only non-null during scan()
  PreloadRequestStream* requests_ = nullptr;
  const KURL* predicted_base_element_url_ = nullptr;
};

// Each CSSPreloaderResourceClient keeps track of a single CSS resource, and
// drives a CSSPreloadScanner as raw data arrives for it. This lets us preload
// @import tags before parsing.
class CORE_EXPORT CSSPreloaderResourceClient
    : public GarbageCollectedFinalized<CSSPreloaderResourceClient>,
      public StyleSheetResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(CSSPreloaderResourceClient);

 public:
  CSSPreloaderResourceClient(Resource*, HTMLResourcePreloader*);
  ~CSSPreloaderResourceClient();
  void SetCSSStyleSheet(const String& href,
                        const KURL& base_url,
                        ReferrerPolicy,
                        const WTF::TextEncoding&,
                        const CSSStyleSheetResource*) override;
  void DidAppendFirstData(const CSSStyleSheetResource*) override;
  String DebugName() const override { return "CSSPreloaderResourceClient"; }

  void Trace(blink::Visitor*);

 protected:
  // Protected for tests, which don't want to initialize a fully featured
  // DocumentLoader.
  virtual void FetchPreloads(PreloadRequestStream& preloads);

 private:
  void ScanCSS(const CSSStyleSheetResource*);
  void ClearResource();

  enum PreloadPolicy {
    kScanOnly,
    kScanAndPreload,
  };

  const PreloadPolicy policy_;
  WeakMember<HTMLResourcePreloader> preloader_;
  WeakMember<CSSStyleSheetResource> resource_;
};

}  // namespace blink

#endif
