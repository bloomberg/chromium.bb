// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_FETCH_CLIENT_SETTINGS_OBJECT_SNAPSHOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_FETCH_CLIENT_SETTINGS_OBJECT_SNAPSHOT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/weborigin/referrer_policy.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class ExecutionContext;

// This is a partial implementation of the "settings object" concept defined in
// the HTML spec:
// https://html.spec.whatwg.org/multipage/webappapis.html#settings-object
//
// This is also a partial implementation of the "fetch client settings object"
// used in module script fetch. Actually, it's used with ResourceFetcher and
// FetchContext to compensate "fetch client settings object" that are not
// included in this class.
// https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-worker-script-tree
//
// This takes a partial snapshot of the execution context's states so that an
// instance of this class can be passed to another thread without cross-thread
// synchronization. Don't keep this object persistently, instead create a new
// instance per each "fetch a module script graph" algorithm:
// https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-script-tree
// https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-worker-script-tree
class CORE_EXPORT FetchClientSettingsObjectSnapshot final {
 public:
  explicit FetchClientSettingsObjectSnapshot(ExecutionContext&);
  FetchClientSettingsObjectSnapshot(
      const scoped_refptr<const SecurityOrigin> security_origin,
      ReferrerPolicy referrer_policy);

  virtual ~FetchClientSettingsObjectSnapshot() = default;

  // "An origin used in security checks."
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-settings-object-origin
  const SecurityOrigin* GetSecurityOrigin() const {
    return security_origin_.get();
  }

  // "The default referrer policy for fetches performed using this environment
  // settings object as a request client."
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-settings-object-referrer-policy
  ReferrerPolicy GetReferrerPolicy() const { return referrer_policy_; }

  // Makes an copy of this instance. This is typically used for cross-thread
  // communication in CrossThreadCopier.
  FetchClientSettingsObjectSnapshot IsolatedCopy() const {
    return FetchClientSettingsObjectSnapshot(security_origin_->IsolatedCopy(),
                                             referrer_policy_);
  }

 private:
  const scoped_refptr<const SecurityOrigin> security_origin_;
  const ReferrerPolicy referrer_policy_;
};

template <>
struct CrossThreadCopier<FetchClientSettingsObjectSnapshot> {
  static FetchClientSettingsObjectSnapshot Copy(
      const FetchClientSettingsObjectSnapshot& settings_object) {
    return settings_object.IsolatedCopy();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_FETCH_CLIENT_SETTINGS_OBJECT_SNAPSHOT_H_
