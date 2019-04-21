/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_STUB_THREAD_CREATOR_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_STUB_THREAD_CREATOR_H_

#include "native_client/src/untrusted/irt/irt.h"
#include "ppapi/nacl_irt/public/irt_ppapi.h"

#ifdef __cplusplus
extern "C" {
#endif

void __nacl_register_thread_creator(const struct nacl_irt_ppapihook *hooks);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
