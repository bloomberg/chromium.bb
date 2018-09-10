// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

class AXObjectCache;

AXContext::AXContext(Document& document) {
  AXObjectCache* existing_cache = document.ExistingAXObjectCache();
  if (existing_cache) {
    ax_object_cache_ = existing_cache;
    return;
  }

  ax_object_cache_ = AXObjectCache::Create(document);
  document.SetAXObjectCache(ax_object_cache_);
}

AXContext::~AXContext() {}

AXObjectCache& AXContext::GetAXObjectCache() {
  return *ax_object_cache_.Get();
}

}  // namespace blink
