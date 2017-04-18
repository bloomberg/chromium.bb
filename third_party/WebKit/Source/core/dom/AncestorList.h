// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AncestorList_h
#define AncestorList_h

#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KURLHash.h"
#include "platform/wtf/HashSet.h"

namespace blink {

// Maps to "ancestor list" concept referenced in multiple module script
// algorithms.
// Example:
// https://html.spec.whatwg.org/#internal-module-script-graph-fetching-procedure
using AncestorList = HashSet<KURL>;

}  // namespace blink

#endif
