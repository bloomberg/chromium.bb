// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaListDirective_h
#define MediaListDirective_h

#include "base/macros.h"
#include "core/frame/csp/CSPDirective.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ContentSecurityPolicy;

class CORE_EXPORT MediaListDirective final : public CSPDirective {
 public:
  MediaListDirective(const String& name,
                     const String& value,
                     ContentSecurityPolicy*);
  bool Allows(const String& type) const;

  // The algorothm is described more extensively here:
  // https://w3c.github.io/webappsec-csp/embedded/#subsume-policy.
  bool Subsumes(const HeapVector<Member<MediaListDirective>>& other) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaListDirectiveTest, GetIntersect);
  FRIEND_TEST_ALL_PREFIXES(MediaListDirectiveTest, Subsumes);

  void Parse(const UChar* begin, const UChar* end);

  // The algorothm is described more extensively here:
  // https://w3c.github.io/webappsec-csp/embedded/#subsume-policy.
  HashSet<String> GetIntersect(const HashSet<String>& other) const;

  HashSet<String> plugin_types_;

  DISALLOW_COPY_AND_ASSIGN(MediaListDirective);
};

}  // namespace blink

#endif
