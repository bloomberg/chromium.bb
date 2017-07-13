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

#ifndef WorkerThreadStartupData_h
#define WorkerThreadStartupData_h

#include <memory>
#include "bindings/core/v8/WorkerV8Settings.h"
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

// A WorkerThreadStartupData carries parameters for starting a worker thread and
// a global scope.
// TODO(nhiroki): Separate this class into WorkerThreadStartupData used for
// initializing WorkerBackingThread, and GlobalScopeStartupData used for
// creating a global scope (https://crbug.com/710364)
class CORE_EXPORT WorkerThreadStartupData final {
  WTF_MAKE_NONCOPYABLE(WorkerThreadStartupData);
  USING_FAST_MALLOC(WorkerThreadStartupData);

 public:
  static std::unique_ptr<WorkerThreadStartupData> Create(
      const KURL& script_url,
      const String& user_agent,
      const String& source_code,
      std::unique_ptr<Vector<char>> cached_meta_data,
      WorkerThreadStartMode start_mode,
      const Vector<CSPHeaderAndType>* content_security_policy_headers,
      const String& referrer_policy,
      const SecurityOrigin* starter_origin,
      WorkerClients* worker_clients,
      WebAddressSpace address_space,
      const Vector<String>* origin_trial_tokens,
      std::unique_ptr<WorkerSettings> worker_settings,
      WorkerV8Settings worker_v8_settings) {
    return WTF::WrapUnique(new WorkerThreadStartupData(
        script_url, user_agent, source_code, std::move(cached_meta_data),
        start_mode, content_security_policy_headers, referrer_policy,
        starter_origin, worker_clients, address_space, origin_trial_tokens,
        std::move(worker_settings), worker_v8_settings));
  }

  ~WorkerThreadStartupData();

  KURL script_url_;
  String user_agent_;
  String source_code_;
  std::unique_ptr<Vector<char>> cached_meta_data_;
  WorkerThreadStartMode start_mode_;
  std::unique_ptr<Vector<CSPHeaderAndType>> content_security_policy_headers_;
  String referrer_policy_;
  std::unique_ptr<Vector<String>> origin_trial_tokens_;

  // The SecurityOrigin of the Document creating a Worker may have
  // been configured with extra policy privileges when it was created
  // (e.g., enforce path-based file:// origins.)
  // To ensure that these are transferred to the origin of a new worker
  // global scope, supply the Document's SecurityOrigin as the
  // 'starter origin'.
  //
  // See SecurityOrigin::transferPrivilegesFrom() for details on what
  // privileges are transferred.
  std::unique_ptr<SecurityOrigin::PrivilegeData> starter_origin_privilege_data_;

  // This object is created and initialized on the thread creating
  // a new worker context, but ownership of it and this WorkerThreadStartupData
  // structure is passed along to the new worker thread, where it is finalized.
  //
  // Hence, CrossThreadPersistent<> is required to allow finalization
  // to happen on a thread different than the thread creating the
  // persistent reference. If the worker thread creation context
  // supplies no extra 'clients', m_workerClients can be left as empty/null.
  CrossThreadPersistent<WorkerClients> worker_clients_;

  WebAddressSpace address_space_;

  std::unique_ptr<WorkerSettings> worker_settings_;

  WorkerV8Settings worker_v8_settings_;

 private:
  WorkerThreadStartupData(
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
      WorkerV8Settings);
};

}  // namespace blink

#endif  // WorkerThreadStartupData_h
