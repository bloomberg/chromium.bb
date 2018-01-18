// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AtRuleDescriptorSerializer_h
#define AtRuleDescriptorSerializer_h

#include "core/css/parser/AtRuleDescriptorValueSet.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class AtRuleDescriptorValueSet;

class AtRuleDescriptorSerializer {
  STATIC_ONLY(AtRuleDescriptorSerializer);

 public:
  static String SerializeAtRuleDescriptors(const AtRuleDescriptorValueSet&);
};

}  // namespace blink

#endif  // AtRuleDescriptorSerializer_h
