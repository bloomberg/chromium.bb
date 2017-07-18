// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GlobalScopeCreationParams_h
#define GlobalScopeCreationParams_h

#include <memory>
#include "bindings/core/v8/V8CacheOptions.h"
#include "core/CoreExport.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerSettings.h"
#include "core/workers/WorkerThread.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebAddressSpace.h"

namespace blink {

class WorkerClients;

// GlobalScopeCreationParams contains parameters for initializing
// WorkerGlobalScope or WorkletGlobalScope.
struct CORE_EXPORT GlobalScopeCreationParams final {
  WTF_MAKE_NONCOPYABLE(GlobalScopeCreationParams);
  USING_FAST_MALLOC(GlobalScopeCreationParams);

 public:
  GlobalScopeCreationParams(
      const KURL& script_url,
      const String& user_agent,
      const String& source_code,
      std::unique_ptr<Vector<char>> cached_meta_data,
      WorkerThreadStartMode,
      const Vector<CSPHeaderAndType>* content_security_policy_headers,
      const String& referrer_policy,
      const SecurityOrigin*,
      WorkerClients*,
      WebAddressSpace,
      const Vector<String>* origin_trial_tokens,
      std::unique_ptr<WorkerSettings>,
      V8CacheOptions);

  ~GlobalScopeCreationParams() = default;

  KURL script_url;
  String user_agent;
  String source_code;
  std::unique_ptr<Vector<char>> cached_meta_data;
  WorkerThreadStartMode start_mode;
  std::unique_ptr<Vector<CSPHeaderAndType>> content_security_policy_headers;
  String referrer_policy;
  std::unique_ptr<Vector<String>> origin_trial_tokens;

  // The SecurityOrigin of the Document creating a Worker may have
  // been configured with extra policy privileges when it was created
  // (e.g., enforce path-based file:// origins.)
  // To ensure that these are transferred to the origin of a new worker
  // global scope, supply the Document's SecurityOrigin as the
  // 'starter origin'.
  //
  // See SecurityOrigin::transferPrivilegesFrom() for details on what
  // privileges are transferred.
  std::unique_ptr<SecurityOrigin::PrivilegeData> starter_origin_privilege_data;

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

  WebAddressSpace address_space;

  std::unique_ptr<WorkerSettings> worker_settings;

  V8CacheOptions v8_cache_options;
};

}  // namespace blink

#endif  // GlobalScopeCreationParams_h
