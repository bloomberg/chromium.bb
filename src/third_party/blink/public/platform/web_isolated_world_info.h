// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_ISOLATED_WORLD_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_ISOLATED_WORLD_INFO_H_

namespace blink {

struct WebIsolatedWorldInfo {
  // Associates an isolated world with a security origin. XMLHttpRequest
  // instances used in that world will be considered to come from that origin,
  // not the frame's.
  //
  // Currently the origin shouldn't be aliased, because IsolatedCopy() is
  // taken before associating it to an isolated world and aliased relationship,
  // if any, is broken. crbug.com/779730
  // Note: If this is null, the security origin for the isolated world is
  // cleared.
  WebSecurityOrigin security_origin;

  // Associates a content security policy with an isolated world. This policy
  // should be used when evaluating script in the isolated world, and should
  // also replace a protected resource's CSP when evaluating resources
  // injected into the DOM.
  //
  // TODO(crbug.com/896041): Setting this simply bypasses the protected
  // resource's CSP. It doesn't yet restrict the isolated world to the provided
  // policy.
  // Note: If this is null, the content security policy for the isolated world
  // is cleared. Else if this is specified, |security_origin| must also be
  // specified.
  WebString content_security_policy;

  // Associates an isolated world with human-readable name which is useful for
  // extension debugging.
  WebString human_readable_name;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_ISOLATED_WORLD_INFO_H_
