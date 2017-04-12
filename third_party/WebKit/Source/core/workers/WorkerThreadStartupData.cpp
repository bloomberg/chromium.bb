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

#include "core/workers/WorkerThreadStartupData.h"

#include <memory>
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

WorkerThreadStartupData::WorkerThreadStartupData(
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
    WorkerV8Settings worker_v8_settings)
    : script_url_(script_url.Copy()),
      user_agent_(user_agent.IsolatedCopy()),
      source_code_(source_code.IsolatedCopy()),
      cached_meta_data_(std::move(cached_meta_data)),
      start_mode_(start_mode),
      referrer_policy_(referrer_policy.IsolatedCopy()),
      starter_origin_privilege_data_(
          starter_origin ? starter_origin->CreatePrivilegeData() : nullptr),
      worker_clients_(worker_clients),
      address_space_(address_space),
      worker_settings_(std::move(worker_settings)),
      worker_v8_settings_(worker_v8_settings) {
  content_security_policy_headers_ =
      WTF::MakeUnique<Vector<CSPHeaderAndType>>();
  if (content_security_policy_headers) {
    for (const auto& header : *content_security_policy_headers) {
      CSPHeaderAndType copied_header(header.first.IsolatedCopy(),
                                     header.second);
      content_security_policy_headers_->push_back(copied_header);
    }
  }

  origin_trial_tokens_ = std::unique_ptr<Vector<String>>(new Vector<String>());
  if (origin_trial_tokens) {
    for (const String& token : *origin_trial_tokens)
      origin_trial_tokens_->push_back(token.IsolatedCopy());
  }
}

WorkerThreadStartupData::~WorkerThreadStartupData() {}

}  // namespace blink
