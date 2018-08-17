// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_TYPE_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_TYPE_POLICY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_options.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class TrustedHTML;
class TrustedScript;
class TrustedScriptURL;
class TrustedURL;

class CORE_EXPORT TrustedTypePolicy final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TrustedTypePolicy* Create(const String& policy_name,
                                   const TrustedTypePolicyOptions&);

  TrustedHTML* createHTML(ScriptState*, const String&, ExceptionState&);
  TrustedScript* createScript(ScriptState*, const String&, ExceptionState&);
  TrustedScriptURL* createScriptURL(ScriptState*,
                                    const String&,
                                    ExceptionState&);
  TrustedURL* createURL(ScriptState*, const String&, ExceptionState&);

  String name() const;

  void Trace(blink::Visitor*) override;

 private:
  TrustedTypePolicy(const String& policy_name, const TrustedTypePolicyOptions&);

  String name_;
  TrustedTypePolicyOptions policy_options_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_TYPE_POLICY_H_
