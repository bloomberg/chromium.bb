// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementDefinition.h"

namespace blink {

CustomElementDefinition::CustomElementDefinition(
    const CustomElementDescriptor& descriptor)
    : m_descriptor(descriptor)
{
}

CustomElementDefinition::~CustomElementDefinition()
{
}

} // namespace blink
