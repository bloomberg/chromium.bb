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
// the HTML spec.
// "An environment settings object, containing various settings that are shared
// with other scripts in the same context."
// https://html.spec.whatwg.org/multipage/webappapis.html#settings-object
//
// This is used as the "fetch client settings object" on script fetch. For
// example:
// "To fetch a module worker script graph given a url, a fetch client settings
// object, a destination, a credentials mode, and a module map settings object,
// run these steps."
// https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-worker-script-tree
//
// In the HTML spec, the "settings object" concept is defined as master data of
// the execution context's states, and each spec algorithm takes a reference to
// the master data. On the other hand, this class takes a partial snapshot of
// the execution context's states so that an instance of this class can be
// passed to another thread without cross-thread synchronization. This means the
// instance can be out of sync with the master data. If you need a fresh data,
// you have to create a new instance again.
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
