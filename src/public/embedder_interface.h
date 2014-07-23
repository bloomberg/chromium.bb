/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_PUBLIC_EMBEDDER_INTERFACE_H_
#define NATIVE_CLIENT_SRC_PUBLIC_EMBEDDER_INTERFACE_H_

/* A set of functions the embedder should implement for interfacing with NaCl.
 * If NULL, NaCl will fall back on an internal SRPC-based implementation.
 * TODO(teravest): Remove SRPC fallback.
 */
struct NaClEmbedderInterface {
  /* Log a fatal message that caused the NaCl module to terminate. */
  void (*LogFatalMessage)(const char* data, size_t bytes);
};

#endif /* NATIVE_CLIENT_SRC_PUBLIC_EMBEDDER_INTERFACE_H_ */
