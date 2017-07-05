// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CustomElementReaction.h"

#include "core/html/custom/CustomElementDefinition.h"

namespace blink {

CustomElementReaction::CustomElementReaction(
    CustomElementDefinition* definition)
    : definition_(definition) {}

DEFINE_TRACE(CustomElementReaction) {
  visitor->Trace(definition_);
}

}  // namespace blink
