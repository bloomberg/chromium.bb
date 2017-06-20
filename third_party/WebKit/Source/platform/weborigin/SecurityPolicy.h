/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SecurityPolicy_h
#define SecurityPolicy_h

#include "platform/PlatformExport.h"
#include "platform/weborigin/Referrer.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class KURL;
class SecurityOrigin;

enum ReferrerPolicyLegacyKeywordsSupport {
  kSupportReferrerPolicyLegacyKeywords,
  kDoNotSupportReferrerPolicyLegacyKeywords,
};

class PLATFORM_EXPORT SecurityPolicy {
  STATIC_ONLY(SecurityPolicy);

 public:
  // This must be called during initialization (before we create
  // other threads).
  static void Init();

  // True if the referrer should be omitted according to the
  // ReferrerPolicyNoReferrerWhenDowngrade. If you intend to send a
  // referrer header, you should use generateReferrer instead.
  static bool ShouldHideReferrer(const KURL&, const KURL& referrer);

  // Returns the referrer modified according to the referrer policy for a
  // navigation to a given URL. If the referrer returned is empty, the
  // referrer header should be omitted.
  static Referrer GenerateReferrer(ReferrerPolicy,
                                   const KURL&,
                                   const String& referrer);

  static void AddOriginAccessWhitelistEntry(const SecurityOrigin& source_origin,
                                            const String& destination_protocol,
                                            const String& destination_domain,
                                            bool allow_destination_subdomains);
  static void RemoveOriginAccessWhitelistEntry(
      const SecurityOrigin& source_origin,
      const String& destination_protocol,
      const String& destination_domain,
      bool allow_destination_subdomains);
  static void ResetOriginAccessWhitelists();

  static bool IsAccessWhiteListed(const SecurityOrigin* active_origin,
                                  const SecurityOrigin* target_origin);
  static bool IsAccessToURLWhiteListed(const SecurityOrigin* active_origin,
                                       const KURL&);

  static void AddOriginTrustworthyWhiteList(const SecurityOrigin&);
  static bool IsOriginWhiteListedTrustworthy(const SecurityOrigin&);
  static bool IsUrlWhiteListedTrustworthy(const KURL&);

  static bool ReferrerPolicyFromString(const String& policy,
                                       ReferrerPolicyLegacyKeywordsSupport,
                                       ReferrerPolicy* result);

  static bool ReferrerPolicyFromHeaderValue(const String& header_value,
                                            ReferrerPolicyLegacyKeywordsSupport,
                                            ReferrerPolicy* result);
};

}  // namespace blink

#endif  // SecurityPolicy_h
