// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "require_trusted_types_for_directive.h"

namespace blink {

RequireTrustedTypesForDirective::RequireTrustedTypesForDirective(
    const String& name,
    const String& value,
    ContentSecurityPolicy* policy)
    : CSPDirective(name, value, policy),
      require_trusted_types_for_script_(false) {
  Vector<String> list;
  value.SimplifyWhiteSpace().Split(' ', false, list);

  for (const String& v : list) {
    // The only value in the sink group is 'script'.
    // https://w3c.github.io/webappsec-trusted-types/dist/spec/#trusted-types-sink-group
    if (v == "'script'") {
      require_trusted_types_for_script_ = true;
      break;
    }
  }
}

bool RequireTrustedTypesForDirective::require() const {
  return require_trusted_types_for_script_;
}

void RequireTrustedTypesForDirective::Trace(Visitor* visitor) {
  CSPDirective::Trace(visitor);
}

}  // namespace blink
