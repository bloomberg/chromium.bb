/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HeapTestUtilities_h
#define HeapTestUtilities_h

namespace blink {

void PreciselyCollectGarbage();
void ConservativelyCollectGarbage();
void ClearOutOldGarbage();

}  // namespace blink

#endif  // HeapTestUtilities_h
