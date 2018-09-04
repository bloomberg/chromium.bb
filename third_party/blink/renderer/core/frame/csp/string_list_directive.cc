// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/string_list_directive.h"

#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

StringListDirective::StringListDirective(const String& name,
                                         const String& value,
                                         ContentSecurityPolicy* policy)
    : CSPDirective(name, value, policy) {
  // TODO(orsibatiz): After policy naming rules are estabilished, do a more
  // complex parsing according to that.
  value.Split(' ', false, list_);
}

bool StringListDirective::Allows(const String& string_piece) {
  if (list_.IsEmpty())
    return true;
  for (String p : list_) {
    if (p == string_piece) {
      return true;
    }
  }
  return false;
}

void StringListDirective::Trace(blink::Visitor* visitor) {
  CSPDirective::Trace(visitor);
}

}  // namespace blink
