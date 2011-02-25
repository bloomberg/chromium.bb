/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_INCLUDE_SYS_NACL_NAME_SERVICE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_INCLUDE_SYS_NACL_NAME_SERVICE_H_

#define NACL_NAME_SERVICE_CONNECTION_MAX  8

#define NACL_NAME_SERVICE_SUCCESS                 0
#define NACL_NAME_SERVICE_NAME_NOT_FOUND          1
#define NACL_NAME_SERVICE_DUPLICATE_NAME          2
#define NACL_NAME_SERVICE_INSUFFICIENT_RESOURCES  3

#define NACL_NAME_SERVICE_INSERT          "insert:sh:i"
#define NACL_NAME_SERVICE_DELETE          "delete:s:i"
#define NACL_NAME_SERVICE_MAKE_IMMUTABLE  "make_immutable::"
#define NACL_NAME_SERVICE_LOOKUP          "lookup:s:ih"

#endif
/* NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_INCLUDE_SYS_NACL_NAME_SERVICE_H_ */
