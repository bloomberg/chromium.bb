// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SETTINGS_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SETTINGS_OBJECT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/weborigin/referrer_policy.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class ExecutionContext;

// This is an implementation of the "settings object" concept defined in the
// HTML spec:
// "An environment settings object, containing various settings that are shared
// with other scripts in the same context."
// https://html.spec.whatwg.org/multipage/webappapis.html#settings-object
//
// Note that this class takes a snapshot of the given ExecutionContext's states
// so that it can be passed to another thread, while ExecutionContext is not
// cross-thread-transferable. If you need a fresh snapshot, you have to create
// an instance of this class again.
//
// TODO(nhiroki): Strictly speaking, this class doesn't match the "settings
// object" concept in the spec. The spec requires a reference to the "settings
// object", but instead this class provides a snapshot to avoid cross thread
// references. To reduce confusion, this class should be renamed.
class CORE_EXPORT SettingsObject final {
 public:
  explicit SettingsObject(ExecutionContext&);
  SettingsObject(const scoped_refptr<const SecurityOrigin> security_origin,
                 ReferrerPolicy referrer_policy);

  virtual ~SettingsObject() = default;

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
  SettingsObject IsolatedCopy() const {
    return SettingsObject(security_origin_->IsolatedCopy(), referrer_policy_);
  }

 private:
  const scoped_refptr<const SecurityOrigin> security_origin_;
  const ReferrerPolicy referrer_policy_;
};

template <>
struct CrossThreadCopier<SettingsObject> {
  static SettingsObject Copy(const SettingsObject& settings_object) {
    return settings_object.IsolatedCopy();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SETTINGS_OBJECT_H_
