// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"

#include <utility>

#include "base/logging.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

class IsolatedWorldCSPDelegate final
    : public GarbageCollectedFinalized<IsolatedWorldCSPDelegate>,
      public ContentSecurityPolicyDelegate {
  USING_GARBAGE_COLLECTED_MIXIN(IsolatedWorldCSPDelegate);

 public:
  IsolatedWorldCSPDelegate(Document& document,
                           scoped_refptr<SecurityOrigin> security_origin,
                           bool apply_policy)
      : document_(&document),
        security_origin_(std::move(security_origin)),
        apply_policy_(apply_policy) {}

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(document_);
    ContentSecurityPolicyDelegate::Trace(visitor);
  }

  const SecurityOrigin* GetSecurityOrigin() override {
    return security_origin_.get();
  }

  const KURL& Url() const override {
    // This is used to populate violation data's violation url. See
    // https://w3c.github.io/webappsec-csp/#violation-url.
    // TODO(crbug.com/916885): Figure out if we want to support violation
    // reporting for isolated world CSPs.
    DEFINE_STATIC_LOCAL(KURL, g_empty_url, ());
    return g_empty_url;
  }

  // Isolated world CSPs don't support these directives: "sandbox",
  // "treat-as-public-address", "trusted-types" and "upgrade-insecure-requests".
  // These directives depend on ExecutionContext for their implementation and
  // since isolated worlds don't have their own ExecutionContext, these are not
  // supported.
  void SetSandboxFlags(SandboxFlags) override {}
  void SetAddressSpace(mojom::IPAddressSpace) override {}
  void SetRequireTrustedTypes() override {}
  void AddInsecureRequestPolicy(WebInsecureRequestPolicy) override {}

  // TODO(crbug.com/916885): Figure out if we want to support violation
  // reporting for isolated world CSPs.
  std::unique_ptr<SourceLocation> GetSourceLocation() override {
    return nullptr;
  }
  base::Optional<uint16_t> GetStatusCode() override { return base::nullopt; }
  String GetDocumentReferrer() override { return g_empty_string; }
  void DispatchViolationEvent(const SecurityPolicyViolationEventInit&,
                              Element*) override {
    DCHECK(apply_policy_);
  }
  void PostViolationReport(const SecurityPolicyViolationEventInit&,
                           const String& stringified_report,
                           bool is_frame_ancestors_violation,
                           const Vector<String>& report_endpoints,
                           bool use_reporting_api) override {
    DCHECK(apply_policy_);
  }

  void Count(WebFeature feature) override {
    // Log the features used by isolated world CSPs on the underlying document.
    UseCounter::Count(document_, feature);
  }

  void AddConsoleMessage(ConsoleMessage* console_message) override {
    document_->AddConsoleMessage(console_message);
  }

  void DisableEval(const String& error_message) override {
    // TODO(crbug.com/896041): Implement this.
    NOTIMPLEMENTED();
  }

  void ReportBlockedScriptExecutionToInspector(
      const String& directive_text) override {
    // TODO(crbug.com/896041): Figure out if this needs to be implemented.
    NOTIMPLEMENTED();
  }

  void DidAddContentSecurityPolicies(
      const blink::WebVector<WebContentSecurityPolicy>&) override {}

 private:
  const Member<Document> document_;
  const scoped_refptr<SecurityOrigin> security_origin_;

  // Whether the 'IsolatedWorldCSP' feature is enabled, and we are applying the
  // CSP provided by the isolated world.
  const bool apply_policy_;
};

}  // namespace

// static
IsolatedWorldCSP& IsolatedWorldCSP::Get() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(IsolatedWorldCSP, g_isolated_world_csp, ());
  return g_isolated_world_csp;
}

void IsolatedWorldCSP::SetContentSecurityPolicy(int world_id,
                                                const String& policy) {
  DCHECK(IsMainThread());
  DCHECK(DOMWrapperWorld::IsIsolatedWorldId(world_id));

  // TODO(crbug.com/896041): We should allow clients to use an empty
  // ContentSecurityPolicy. Introduce a ClearContentSecurityPolicy method if
  // needed.
  if (policy.IsEmpty())
    csp_map_.erase(world_id);
  else
    csp_map_.Set(world_id, policy);
}

bool IsolatedWorldCSP::HasContentSecurityPolicy(int world_id) const {
  DCHECK(IsMainThread());
  DCHECK(DOMWrapperWorld::IsIsolatedWorldId(world_id));

  auto it = csp_map_.find(world_id);
  return it != csp_map_.end();
}

ContentSecurityPolicy* IsolatedWorldCSP::CreateIsolatedWorldCSP(
    Document& document,
    int world_id) {
  DCHECK(IsMainThread());
  DCHECK(DOMWrapperWorld::IsIsolatedWorldId(world_id));

  auto it = csp_map_.find(world_id);
  if (it == csp_map_.end())
    return nullptr;

  const String& policy = it->value;
  const bool apply_policy = RuntimeEnabledFeatures::IsolatedWorldCSPEnabled();

  ContentSecurityPolicy* csp = ContentSecurityPolicy::Create();

  // TODO(crbug.com/896041): Plumb the correct security origin.
  scoped_refptr<SecurityOrigin> csp_security_origin =
      SecurityOrigin::CreateUniqueOpaque();
  IsolatedWorldCSPDelegate* delegate =
      MakeGarbageCollected<IsolatedWorldCSPDelegate>(
          document, std::move(csp_security_origin), apply_policy);
  csp->BindToDelegate(*delegate);

  if (apply_policy) {
    csp->AddPolicyFromHeaderValue(policy,
                                  kContentSecurityPolicyHeaderTypeEnforce,
                                  kContentSecurityPolicyHeaderSourceHTTP);
  }

  return csp;
}

IsolatedWorldCSP::IsolatedWorldCSP() = default;

}  // namespace blink
