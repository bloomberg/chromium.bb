// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SETTINGS_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SETTINGS_OBJECT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/weborigin/referrer_policy.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// This is an implementation of the "settings object" concept defined in the
// HTML spec:
// "An environment settings object, containing various settings that are shared
// with other scripts in the same context."
// https://html.spec.whatwg.org/multipage/webappapis.html#settings-object
class CORE_EXPORT SettingsObject final
    : public GarbageCollectedFinalized<SettingsObject> {
 public:
  static SettingsObject* Create(const SecurityOrigin*, ReferrerPolicy);

  virtual ~SettingsObject() = default;

  // "An origin used in security checks."
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-settings-object-origin
  const SecurityOrigin* GetSecurityOrigin() { return security_origin_.get(); }

  // "The default referrer policy for fetches performed using this environment
  // settings object as a request client."
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-settings-object-referrer-policy
  ReferrerPolicy GetReferrerPolicy() const { return referrer_policy_; }

  virtual void Trace(blink::Visitor*) {}

 private:
  SettingsObject(const SecurityOrigin*, ReferrerPolicy);

  const scoped_refptr<const SecurityOrigin> security_origin_;
  ReferrerPolicy referrer_policy_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SETTINGS_OBJECT_H_
