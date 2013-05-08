/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* These weak stubs are provided for bitcode linking,
 * so that they are defined symbols.
 */

#define STUB(sym)   void sym() { }

STUB(longjmp)
STUB(setjmp)

