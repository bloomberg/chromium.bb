// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_plugin_thread_delegate.h"

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/module.h"

namespace remoting {

PepperPluginThreadDelegate::PepperPluginThreadDelegate()
    : core_(pp::Module::Get()->core()) {
}

PepperPluginThreadDelegate::~PepperPluginThreadDelegate() { }

bool PepperPluginThreadDelegate::RunOnPluginThread(
    base::TimeDelta delay, void(CDECL function)(void*), void* data) {
  // It is safe to cast |function| to PP_CompletionCallback_Func,
  // which is defined as void(*)(void*, int). The callee will just
  // ignore the last argument. The only case when it may be unsafe is
  // with VS when default calling convention is set to __stdcall, but
  // this code will not typecheck in that case.
  core_->CallOnMainThread(
      delay.InMilliseconds(), pp::CompletionCallback(
          reinterpret_cast<PP_CompletionCallback_Func>(function), data), 0);
  return true;
}

}  // namespace remoting
