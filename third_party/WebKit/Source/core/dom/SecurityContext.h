/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
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
 *
 */

#ifndef SecurityContext_h
#define SecurityContext_h

#include "core/CoreExport.h"
#include "core/dom/SandboxFlags.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/Suborigin.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebFeaturePolicy.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/platform/WebURLRequest.h"

#include <memory>

namespace blink {

class SecurityOrigin;
class ContentSecurityPolicy;

class CORE_EXPORT SecurityContext : public GarbageCollectedMixin {
  WTF_MAKE_NONCOPYABLE(SecurityContext);

 public:
  virtual void Trace(blink::Visitor*);

  using InsecureNavigationsSet = HashSet<unsigned, WTF::AlreadyHashed>;

  SecurityOrigin* GetSecurityOrigin() const { return security_origin_.get(); }
  ContentSecurityPolicy* GetContentSecurityPolicy() const {
    return content_security_policy_.Get();
  }

  // Explicitly override the security origin for this security context.
  // Note: It is dangerous to change the security origin of a script context
  //       that already contains content.
  void SetSecurityOrigin(scoped_refptr<SecurityOrigin>);
  virtual void DidUpdateSecurityOrigin() = 0;

  SandboxFlags GetSandboxFlags() const { return sandbox_flags_; }
  bool IsSandboxed(SandboxFlags mask) const { return sandbox_flags_ & mask; }
  virtual void EnforceSandboxFlags(SandboxFlags mask);

  void SetAddressSpace(WebAddressSpace space) { address_space_ = space; }
  WebAddressSpace AddressSpace() const { return address_space_; }
  String addressSpaceForBindings() const;

  void SetRequireTrustedTypes() { require_safe_types_ = true; }
  bool RequireTrustedTypes() const { return require_safe_types_; }

  void AddInsecureNavigationUpgrade(unsigned hashed_host) {
    insecure_navigations_to_upgrade_.insert(hashed_host);
  }
  InsecureNavigationsSet* InsecureNavigationsToUpgrade() {
    return &insecure_navigations_to_upgrade_;
  }

  virtual void SetInsecureRequestPolicy(WebInsecureRequestPolicy policy) {
    insecure_request_policy_ = policy;
  }
  WebInsecureRequestPolicy GetInsecureRequestPolicy() const {
    return insecure_request_policy_;
  }

  void EnforceSuborigin(const Suborigin&);

  WebFeaturePolicy* GetFeaturePolicy() const { return feature_policy_.get(); }
  void InitializeFeaturePolicy(const WebParsedFeaturePolicy& parsed_header,
                               const WebParsedFeaturePolicy& container_policy,
                               const WebFeaturePolicy* parent_feature_policy);
  void UpdateFeaturePolicyOrigin();

  void ApplySandboxFlags(SandboxFlags mask);

 protected:
  SecurityContext();
  virtual ~SecurityContext();

  void SetContentSecurityPolicy(ContentSecurityPolicy*);

 private:
  scoped_refptr<SecurityOrigin> security_origin_;
  Member<ContentSecurityPolicy> content_security_policy_;
  std::unique_ptr<WebFeaturePolicy> feature_policy_;

  SandboxFlags sandbox_flags_;

  WebAddressSpace address_space_;
  WebInsecureRequestPolicy insecure_request_policy_;
  InsecureNavigationsSet insecure_navigations_to_upgrade_;
  bool require_safe_types_;
};

}  // namespace blink

#endif  // SecurityContext_h
