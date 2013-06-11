// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBCORKSCREW_OVERRIDES_CUTILS_ATOMIC_H_
#define THIRD_PARTY_LIBCORKSCREW_OVERRIDES_CUTILS_ATOMIC_H_

#define android_atomic_acquire_cas(old_value, new_value, ptr) \
    __sync_val_compare_and_swap((ptr), (old_value), (new_value))
#define android_atomic_release_store(value, ptr) \
    do { __sync_synchronize(); *(ptr) = value; } while (0)
#define android_atomic_acquire_load(ptr) \
    __sync_fetch_and_add(ptr, 0)

#endif  // THIRD_PARTY_LIBCORKSCREW_OVERRIDES_CUTILS_ATOMIC_H_
