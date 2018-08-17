// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_TYPE_POLICY_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_TYPE_POLICY_FACTORY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_options.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class LocalFrame;
class ScriptPromise;

class CORE_EXPORT TrustedTypePolicyFactory final : public ScriptWrappable,
                                                   public DOMWindowClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(TrustedTypePolicyFactory);

 public:
  static TrustedTypePolicyFactory* Create(LocalFrame* frame) {
    return new TrustedTypePolicyFactory(frame);
  }

  static ScriptPromise createPolicy(ScriptState*,
                                    const String&,
                                    const TrustedTypePolicyOptions&);

  void Trace(blink::Visitor*) override;

 private:
  explicit TrustedTypePolicyFactory(LocalFrame*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_TYPE_POLICY_FACTORY_H_
