// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSCALL_H__
#define SYSCALL_H__

#ifdef __cplusplus
extern "C" {
#endif

void syscallWrapper() asm("playground$syscallWrapper")
#if defined(__x86_64__)
                      __attribute__((visibility("internal")))
#endif
;

#ifdef __cplusplus
}
#endif

#endif // SYSCALL_H__
