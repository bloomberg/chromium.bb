// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/weborigin/Suborigin.h"

#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/text/ParsingUtilities.h"

namespace blink {

static unsigned g_empty_policy =
    static_cast<unsigned>(Suborigin::SuboriginPolicyOptions::kNone);

Suborigin::Suborigin() : options_mask_(g_empty_policy) {}

Suborigin::Suborigin(const Suborigin* other)
    : name_(other->name_.IsolatedCopy()), options_mask_(other->options_mask_) {}

void Suborigin::SetTo(const Suborigin& other) {
  name_ = other.name_;
  options_mask_ = other.options_mask_;
}

void Suborigin::AddPolicyOption(SuboriginPolicyOptions option) {
  options_mask_ |= static_cast<unsigned>(option);
}

bool Suborigin::PolicyContains(SuboriginPolicyOptions option) const {
  return options_mask_ & static_cast<unsigned>(option);
}

void Suborigin::Clear() {
  name_ = String();
  options_mask_ = g_empty_policy;
}

}  // namespace blink
