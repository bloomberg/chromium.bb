// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AXObject_h
#define AXObject_h

#include "core/CoreExport.h"

namespace blink {

// TODO(sashab): Add pure virtual methods to this class to remove dependencies
// on AXObjectImpl from outside of modules/.
class CORE_EXPORT AXObject {};

}  // namespace blink

#endif  // AXObject_h
