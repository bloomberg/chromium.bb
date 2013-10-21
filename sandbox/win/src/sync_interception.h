// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

#ifndef SANDBOX_SRC_SYNC_INTERCEPTION_H__
#define SANDBOX_SRC_SYNC_INTERCEPTION_H__

namespace sandbox {

extern "C" {

typedef HANDLE (WINAPI *CreateEventWFunction) (
    LPSECURITY_ATTRIBUTES lpEventAttributes,
    BOOL bManualReset,
    BOOL bInitialState,
    LPCWSTR lpName);

typedef HANDLE (WINAPI *CreateEventAFunction) (
    LPSECURITY_ATTRIBUTES lpEventAttributes,
    BOOL bManualReset,
    BOOL bInitialState,
    LPCSTR lpName);

typedef HANDLE (WINAPI *OpenEventWFunction) (
    DWORD dwDesiredAccess,
    BOOL bInitialState,
    LPCWSTR lpName);

typedef HANDLE (WINAPI *OpenEventAFunction) (
    DWORD dwDesiredAccess,
    BOOL bInheritHandle,
    LPCSTR lpName);

// Interceptors for CreateEventW/A
SANDBOX_INTERCEPT HANDLE WINAPI TargetCreateEventW(
    CreateEventWFunction orig_CreateEvent,
    LPSECURITY_ATTRIBUTES security_attributes,
    BOOL manual_reset,
    BOOL initial_state,
    LPCWSTR name);

SANDBOX_INTERCEPT HANDLE WINAPI TargetCreateEventA(
    CreateEventAFunction orig_CreateEvent,
    LPSECURITY_ATTRIBUTES security_attributes,
    BOOL manual_reset,
    BOOL initial_state,
    LPCSTR name);

// Interceptors for OpenEventW/A
SANDBOX_INTERCEPT HANDLE WINAPI TargetOpenEventW(
    OpenEventWFunction orig_OpenEvent,
    DWORD desired_access,
    BOOL inherit_handle,
    LPCWSTR name);

SANDBOX_INTERCEPT HANDLE WINAPI TargetOpenEventA(
    OpenEventAFunction orig_OpenEvent,
    DWORD desired_access,
    BOOL inherit_handle,
    LPCSTR name);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_SRC_SYNC_INTERCEPTION_H__
