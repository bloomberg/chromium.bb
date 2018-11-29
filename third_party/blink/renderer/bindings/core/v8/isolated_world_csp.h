// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ISOLATED_WORLD_CSP_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ISOLATED_WORLD_CSP_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// A singleton storing content security policy for each isolated world.
class CORE_EXPORT IsolatedWorldCSP {
 public:
  static IsolatedWorldCSP& Get();

  // Associated an isolated world with a Content Security Policy. Resources
  // embedded into the main world's DOM from script executed in an isolated
  // world should be restricted based on the isolated world's DOM, not the
  // main world's.
  //
  // FIXME: Right now, resource injection simply bypasses the main world's
  // DOM. More work is necessary to allow the isolated world's policy to be
  // applied correctly.
  void SetContentSecurityPolicy(int world_id, const String& policy);
  bool HasContentSecurityPolicy(int world_id) const;

 private:
  IsolatedWorldCSP();

  // Map from the isolated world |world_id| to a bool denoting if it has a CSP
  // defined.
  HashMap<int, bool> csp_map_;

  DISALLOW_COPY_AND_ASSIGN(IsolatedWorldCSP);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ISOLATED_WORLD_CSP_H_
