// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorProxiedPropertySet_h
#define CompositorProxiedPropertySet_h

#include <memory>
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

// Keeps track of the number of proxies bound to each property.
class CompositorProxiedPropertySet final {
  WTF_MAKE_NONCOPYABLE(CompositorProxiedPropertySet);
  USING_FAST_MALLOC(CompositorProxiedPropertySet);

 public:
  static std::unique_ptr<CompositorProxiedPropertySet> Create();
  virtual ~CompositorProxiedPropertySet();

  bool IsEmpty() const;
  void Increment(uint32_t mutable_properties);
  void Decrement(uint32_t mutable_properties);
  uint32_t ProxiedProperties() const;

 private:
  CompositorProxiedPropertySet();

  unsigned short counts_[CompositorMutableProperty::kNumProperties];
};

}  // namespace blink

#endif  // CompositorProxiedPropertySet_h
