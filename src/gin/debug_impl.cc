// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/debug_impl.h"

namespace gin {

namespace {
v8::FunctionEntryHook g_entry_hook = NULL;
v8::JitCodeEventHandler g_jit_code_event_handler = NULL;
#if defined(OS_WIN)
Debug::CodeRangeCreatedCallback g_code_range_created_callback = NULL;
Debug::CodeRangeDeletedCallback g_code_range_deleted_callback = NULL;
Debug::DebugBreakCallback g_debug_break_callback = NULL;
Debug::DebugResumeCallback g_debug_resume_callback = NULL;
#endif
}  // namespace

// static
void Debug::SetFunctionEntryHook(v8::FunctionEntryHook entry_hook) {
  g_entry_hook = entry_hook;
}

// static
void Debug::SetJitCodeEventHandler(v8::JitCodeEventHandler event_handler) {
  g_jit_code_event_handler = event_handler;
}

#if defined(OS_WIN)
// static
void Debug::SetCodeRangeCreatedCallback(CodeRangeCreatedCallback callback) {
  g_code_range_created_callback = callback;
}

// static
void Debug::SetCodeRangeDeletedCallback(CodeRangeDeletedCallback callback) {
  g_code_range_deleted_callback = callback;
}

// static
void Debug::SetDebugBreakCallback(DebugBreakCallback callback) {
  g_debug_break_callback = callback;
}

// static
void Debug::SetDebugResumeCallback(DebugResumeCallback callback) {
  g_debug_resume_callback = callback;
}
#endif

// static
v8::FunctionEntryHook DebugImpl::GetFunctionEntryHook() {
  return g_entry_hook;
}

// static
v8::JitCodeEventHandler DebugImpl::GetJitCodeEventHandler() {
  return g_jit_code_event_handler;
}

#if defined(OS_WIN)
// static
Debug::CodeRangeCreatedCallback DebugImpl::GetCodeRangeCreatedCallback() {
  return g_code_range_created_callback;
}

// static
Debug::CodeRangeDeletedCallback DebugImpl::GetCodeRangeDeletedCallback() {
  return g_code_range_deleted_callback;
}

// static
Debug::DebugBreakCallback Debug::GetDebugBreakCallback() {
  return g_debug_break_callback;
}

// static
Debug::DebugResumeCallback Debug::GetDebugResumeCallback() {
  return g_debug_resume_callback;
}
#endif

}  // namespace gin
