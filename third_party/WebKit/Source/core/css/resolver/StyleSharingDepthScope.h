// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleSharingDepthScope_h
#define StyleSharingDepthScope_h

#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Node.h"

namespace blink {

// Maintains the style sharing depth in the style resolver.
class StyleSharingDepthScope final {
  STACK_ALLOCATED();

 public:
  explicit StyleSharingDepthScope(Node& parent)
      : resolver_(parent.GetDocument().GetStyleResolver()) {
    resolver_->IncreaseStyleSharingDepth();
  }

  ~StyleSharingDepthScope() { resolver_->DecreaseStyleSharingDepth(); }

 private:
  Member<StyleResolver> resolver_;
};

}  // namespace blink

#endif  // StyleSharingDepthScope_h
