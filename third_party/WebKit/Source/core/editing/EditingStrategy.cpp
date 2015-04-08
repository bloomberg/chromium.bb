// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/EditingStrategy.h"

#include "core/editing/htmlediting.h"

namespace blink {

bool EditingStrategy::editingIgnoresContent(const Node* node)
{
    return ::blink::editingIgnoresContent(node);
}

int EditingStrategy::lastOffsetForEditing(const Node* node)
{
    return ::blink::lastOffsetForEditing(node);
}

} // namespace blink
