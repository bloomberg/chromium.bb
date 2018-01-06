// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GlobalScopeCreationParams_h
#define GlobalScopeCreationParams_h

#include <memory>
#include "base/macros.h"
#include "bindings/core/v8/V8CacheOptions.h"
#include "common/net/ip_address_space.mojom-blink.h"
#include "core/CoreExport.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerSettings.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/PtrUtil.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom-blink.h"

namespace blink {

class WorkerClients;

// GlobalScopeCreationParams contains parameters for initializing
// WorkerGlobalScope or WorkletGlobalScope.
struct CORE_EXPORT GlobalScopeCreationParams final {
  USING_FAST_MALLOC(GlobalScopeCreationParams);

 public:
  GlobalScopeCreationParams(
      const KURL& script_url,
      const String& user_agent,
      const Vector<CSPHeaderAndType>* content_security_policy_parsed_headers,
      ReferrerPolicy referrer_policy,
      const SecurityOrigin*,
      WorkerClients*,
      mojom::IPAddressSpace,
      const Vector<String>* origin_trial_tokens,
      std::unique_ptr<WorkerSettings>,
      V8CacheOptions,
      service_manager::mojom::blink::InterfaceProviderPtrInfo = {});

  ~GlobalScopeCreationParams() = default;

  KURL script_url;
  String user_agent;

  // |content_security_policy_parsed_headers| and
  // |content_security_policy_raw_headers| are mutually exclusive.
  // |content_security_policy_parsed_headers| is an empty vector
  // when |content_security_policy_raw_headers| is set.
  std::unique_ptr<Vector<CSPHeaderAndType>>
      content_security_policy_parsed_headers;
  WTF::Optional<ContentSecurityPolicyResponseHeaders>
      content_security_policy_raw_headers;

  ReferrerPolicy referrer_policy;
  std::unique_ptr<Vector<String>> origin_trial_tokens;

  // The SecurityOrigin of the Document creating a Worker/Worklet.
  //
  // For Workers, the origin may have been configured with extra policy
  // privileges when it was created (e.g., enforce path-based file:// origins.)
  // To ensure that these are transferred to the origin of a new worker global
  // scope, supply the Document's SecurityOrigin as the 'starter origin'. See
  // SecurityOrigin::TransferPrivilegesFrom() for details on what privileges are
  // transferred.
  //
  // For Worklets, the origin is used for fetching module scripts. Worklet
  // scripts need to be fetched as sub-resources of the Document, and a module
  // script loader uses Document's SecurityOrigin for security checks.
  scoped_refptr<const SecurityOrigin> starter_origin;

  // This object is created and initialized on the thread creating
  // a new worker context, but ownership of it and this
  // GlobalScopeCreationParams structure is passed along to the new worker
  // thread, where it is finalized.
  //
  // Hence, CrossThreadPersistent<> is required to allow finalization
  // to happen on a thread different than the thread creating the
  // persistent reference. If the worker thread creation context
  // supplies no extra 'clients', m_workerClients can be left as empty/null.
  CrossThreadPersistent<WorkerClients> worker_clients;

  mojom::IPAddressSpace address_space;

  std::unique_ptr<WorkerSettings> worker_settings;

  V8CacheOptions v8_cache_options;

  service_manager::mojom::blink::InterfaceProviderPtrInfo interface_provider;

  DISALLOW_COPY_AND_ASSIGN(GlobalScopeCreationParams);
};

}  // namespace blink

#endif  // GlobalScopeCreationParams_h
