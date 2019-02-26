// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"

#include "base/logging.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// static
IsolatedWorldCSP& IsolatedWorldCSP::Get() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(IsolatedWorldCSP, g_isolated_world_csp, ());
  return g_isolated_world_csp;
}

void IsolatedWorldCSP::SetContentSecurityPolicy(int world_id,
                                                const String& policy) {
  DCHECK(IsMainThread());
  DCHECK(DOMWrapperWorld::IsIsolatedWorldId(world_id));

  if (policy.IsEmpty())
    csp_map_.erase(world_id);
  else
    csp_map_.Set(world_id, true);
}

bool IsolatedWorldCSP::HasContentSecurityPolicy(int world_id) const {
  DCHECK(IsMainThread());
  DCHECK(DOMWrapperWorld::IsIsolatedWorldId(world_id));

  auto it = csp_map_.find(world_id);
  return it != csp_map_.end() ? it->value : false;
}

IsolatedWorldCSP::IsolatedWorldCSP() = default;

}  // namespace blink
