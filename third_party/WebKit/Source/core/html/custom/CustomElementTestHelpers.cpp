// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CustomElementTestHelpers.h"

namespace blink {

CustomElementDefinition* TestCustomElementDefinitionBuilder::Build(
    const CustomElementDescriptor& descriptor,
    CustomElementDefinition::Id) {
  return new TestCustomElementDefinition(descriptor);
}

}  // namespace blink
