/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSFontFaceSrcValue_h
#define CSSFontFaceSrcValue_h

#include "core/css/CSSValue.h"
#include "core/loader/resource/FontResource.h"
#include "platform/loader/fetch/ResourceOwner.h"
#include "platform/weborigin/Referrer.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;

class CORE_EXPORT CSSFontFaceSrcValue : public CSSValue {
 public:
  static CSSFontFaceSrcValue* Create(
      const String& specified_resource,
      const String& absolute_resource,
      const Referrer& referrer,
      ContentSecurityPolicyDisposition should_check_content_security_policy) {
    return new CSSFontFaceSrcValue(specified_resource, absolute_resource,
                                   referrer, false,
                                   should_check_content_security_policy);
  }
  static CSSFontFaceSrcValue* CreateLocal(
      const String& absolute_resource,
      ContentSecurityPolicyDisposition should_check_content_security_policy) {
    return new CSSFontFaceSrcValue(g_empty_string, absolute_resource,
                                   Referrer(), true,
                                   should_check_content_security_policy);
  }

  const String& GetResource() const { return absolute_resource_; }
  const String& Format() const { return format_; }
  bool IsLocal() const { return is_local_; }

  void SetFormat(const String& format) { format_ = format; }

  bool IsSupportedFormat() const;

  String CustomCSSText() const;

  bool HasFailedOrCanceledSubresources() const;

  FontResource* Fetch(ExecutionContext*) const;

  bool Equals(const CSSFontFaceSrcValue&) const;

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    visitor->Trace(fetched_);
    CSSValue::TraceAfterDispatch(visitor);
  }

 private:
  CSSFontFaceSrcValue(
      const String& specified_resource,
      const String& absolute_resource,
      const Referrer& referrer,
      bool local,
      ContentSecurityPolicyDisposition should_check_content_security_policy)
      : CSSValue(kFontFaceSrcClass),
        absolute_resource_(absolute_resource),
        specified_resource_(specified_resource),
        referrer_(referrer),
        is_local_(local),
        should_check_content_security_policy_(
            should_check_content_security_policy) {}

  void RestoreCachedResourceIfNeeded(ExecutionContext*) const;

  String absolute_resource_;
  String specified_resource_;
  String format_;
  Referrer referrer_;
  bool is_local_;
  ContentSecurityPolicyDisposition should_check_content_security_policy_;

  class FontResourceHelper
      : public GarbageCollectedFinalized<FontResourceHelper>,
        public ResourceOwner<FontResource> {
    USING_GARBAGE_COLLECTED_MIXIN(FontResourceHelper);

   public:
    static FontResourceHelper* Create(FontResource* resource) {
      return new FontResourceHelper(resource);
    }

    DEFINE_INLINE_VIRTUAL_TRACE() {
      ResourceOwner<FontResource>::Trace(visitor);
    }

   private:
    FontResourceHelper(FontResource* resource) { SetResource(resource); }

    String DebugName() const override {
      return "CSSFontFaceSrcValue::FontResourceHelper";
    }
  };
  mutable Member<FontResourceHelper> fetched_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSFontFaceSrcValue, IsFontFaceSrcValue());

}  // namespace blink

#endif
