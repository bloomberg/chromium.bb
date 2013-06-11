// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBCORKSCREW_OVERRIDES_ELF_H_
#define THIRD_PARTY_LIBCORKSCREW_OVERRIDES_ELF_H_

// This override header is needed because the NDK <elf.h> doesn't define
// PT_GNU_EH_FRAME yet. It can be removed when the NDK is rolled to a newer
// version that does.
#include_next <elf.h>
#if !defined(PT_GNU_EH_FRAME)
#define PT_GNU_EH_FRAME 0x6474e550
#else
#warning This header is not useful anymore.
#endif

#endif  // THIRD_PARTY_LIBCORKSCREW_OVERRIDES_ELF_H_
