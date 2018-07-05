// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_FETCH_CLIENT_SETTINGS_OBJECT_SNAPSHOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_FETCH_CLIENT_SETTINGS_OBJECT_SNAPSHOT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/referrer_policy.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class ExecutionContext;

// This class is needed to copy a FetchClientSettingsObjectSnapshot across
// threads, because it has some members which cannot be transferred across
// threads (AtomicString for example).
// There are some rules / restrictions:
//   - This struct cannot contain an object that cannot be transferred across
//     threads (e.g., AtomicString)
//   - Non-simple members need explicit copying (e.g., String::IsolatedCopy,
//     KURL::Copy) rather than the copy constructor or the assignment operator.
struct CrossThreadFetchClientSettingsObjectData {
  WTF_MAKE_NONCOPYABLE(CrossThreadFetchClientSettingsObjectData);
  USING_FAST_MALLOC(CrossThreadFetchClientSettingsObjectData);

 public:
  CrossThreadFetchClientSettingsObjectData(
      KURL base_url,
      scoped_refptr<const SecurityOrigin> security_origin,
      ReferrerPolicy referrer_policy,
      String outgoing_referrer)
      : base_url(std::move(base_url)),
        security_origin(std::move(security_origin)),
        referrer_policy(referrer_policy),
        outgoing_referrer(std::move(outgoing_referrer)) {}

  KURL base_url;
  scoped_refptr<const SecurityOrigin> security_origin;
  ReferrerPolicy referrer_policy;
  String outgoing_referrer;
};

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
class CORE_EXPORT FetchClientSettingsObjectSnapshot final
    : public GarbageCollectedFinalized<FetchClientSettingsObjectSnapshot> {
 public:
  explicit FetchClientSettingsObjectSnapshot(ExecutionContext&);
  explicit FetchClientSettingsObjectSnapshot(
      std::unique_ptr<CrossThreadFetchClientSettingsObjectData>);
  FetchClientSettingsObjectSnapshot(
      const KURL& base_url,
      const scoped_refptr<const SecurityOrigin> security_origin,
      ReferrerPolicy referrer_policy,
      const String& outgoing_referrer);

  virtual ~FetchClientSettingsObjectSnapshot() = default;

  // "A URL used by APIs called by scripts that use this environment settings
  // object to parse URLs."
  // https://html.spec.whatwg.org/multipage/webappapis.html#api-base-url
  const KURL& BaseURL() const { return base_url_; }

  // "An origin used in security checks."
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-settings-object-origin
  const SecurityOrigin* GetSecurityOrigin() const {
    return security_origin_.get();
  }

  // "The default referrer policy for fetches performed using this environment
  // settings object as a request client."
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-settings-object-referrer-policy
  ReferrerPolicy GetReferrerPolicy() const { return referrer_policy_; }

  // "referrerURL" used in the "Determine request's Referrer" algorithm:
  // https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
  const String& GetOutgoingReferrer() const { return outgoing_referrer_; }

  // Gets a copy of the data suitable for passing to another thread.
  std::unique_ptr<CrossThreadFetchClientSettingsObjectData> CopyData() const {
    return std::make_unique<CrossThreadFetchClientSettingsObjectData>(
        base_url_.Copy(), security_origin_->IsolatedCopy(), referrer_policy_,
        outgoing_referrer_.IsolatedCopy());
  }

  void Trace(Visitor*) {}

 private:
  const KURL base_url_;
  const scoped_refptr<const SecurityOrigin> security_origin_;
  const ReferrerPolicy referrer_policy_;
  const String outgoing_referrer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_FETCH_CLIENT_SETTINGS_OBJECT_SNAPSHOT_H_
