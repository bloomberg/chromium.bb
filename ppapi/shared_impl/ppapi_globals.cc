// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_globals.h"

#include "base/lazy_instance.h"  // For testing purposes only.
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread_local.h"  // For testing purposes only.

namespace ppapi {

namespace {
// Thread-local globals for testing. See SetPpapiGlobalsOnThreadForTest for more
// information.
base::LazyInstance<base::ThreadLocalPointer<PpapiGlobals> >::Leaky
    tls_ppapi_globals_for_test = LAZY_INSTANCE_INITIALIZER;
}  // namespace

PpapiGlobals* PpapiGlobals::ppapi_globals_ = NULL;

PpapiGlobals::PpapiGlobals() {
  DCHECK(!ppapi_globals_);
  ppapi_globals_ = this;
}

PpapiGlobals::PpapiGlobals(ForTest) {
  DCHECK(!ppapi_globals_);
}

PpapiGlobals::~PpapiGlobals() {
  DCHECK(ppapi_globals_ == this || !ppapi_globals_);
  ppapi_globals_ = NULL;
}

// static
void PpapiGlobals::SetPpapiGlobalsOnThreadForTest(PpapiGlobals* ptr) {
  // If we're using a per-thread PpapiGlobals, we should not have a global one.
  // If we allowed it, it would always over-ride the "test" versions.
  DCHECK(!ppapi_globals_);
  tls_ppapi_globals_for_test.Pointer()->Set(ptr);
}

base::MessageLoopProxy* PpapiGlobals::GetMainThreadMessageLoop() {
  CR_DEFINE_STATIC_LOCAL(scoped_refptr<base::MessageLoopProxy>, proxy,
      (base::MessageLoopProxy::current()));
  return proxy.get();
}

bool PpapiGlobals::IsHostGlobals() const {
  return false;
}

bool PpapiGlobals::IsPluginGlobals() const {
  return false;
}

// static
PpapiGlobals* PpapiGlobals::GetThreadLocalPointer() {
  return tls_ppapi_globals_for_test.Pointer()->Get();
}

}  // namespace ppapi
