// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides forward declarations for private XPC symbols.

#ifndef SANDBOX_MAC_XPC_H_
#define SANDBOX_MAC_XPC_H_

#include <AvailabilityMacros.h>
#include <mach/mach.h>
#include <bsm/libbsm.h>
#include <xpc/xpc.h>

#include "sandbox/sandbox_export.h"

extern "C" {
// Declare private types.
typedef struct _xpc_pipe_s* xpc_pipe_t;

// Signatures for private XPC functions.
// Dictionary manipulation.
void xpc_dictionary_set_mach_send(xpc_object_t dictionary,
                                  const char* name,
                                  mach_port_t port);
void xpc_dictionary_get_audit_token(xpc_object_t dictionary,
                                    audit_token_t* token);

// Raw object getters.
mach_port_t xpc_mach_send_get_right(xpc_object_t value);

// Pipe methods.
xpc_pipe_t xpc_pipe_create_from_port(mach_port_t port, int flags);
int xpc_pipe_receive(mach_port_t port, xpc_object_t* message);
int xpc_pipe_routine(xpc_pipe_t pipe,
                     xpc_object_t request,
                     xpc_object_t* reply);
int xpc_pipe_routine_reply(xpc_object_t reply);
int xpc_pipe_simpleroutine(xpc_pipe_t pipe, xpc_object_t message);
int xpc_pipe_routine_forward(xpc_pipe_t forward_to, xpc_object_t request);
}  // extern "C"

#endif  // SANDBOX_MAC_XPC_H_
