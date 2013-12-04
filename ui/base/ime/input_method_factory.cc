// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_factory.h"

#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/mock_input_method.h"

#if defined(OS_CHROMEOS) && defined(USE_X11)
#include "ui/base/ime/input_method_ibus.h"
#elif defined(OS_WIN)
#include "base/win/metro.h"
#include "ui/base/ime/input_method_imm32.h"
#include "ui/base/ime/input_method_tsf.h"
#include "ui/base/ime/remote_input_method_win.h"
#elif defined(USE_AURA) && defined(USE_X11)
#include "ui/base/ime/input_method_linux_x11.h"
#else
#include "ui/base/ime/input_method_minimal.h"
#endif

namespace ui {
namespace {

bool g_input_method_set_for_testing = false;
InputMethod* g_shared_input_method = NULL;

#if defined(OS_WIN)
// Returns a new instance of input method object for Windows.
scoped_ptr<InputMethod> CreateInputMethodWinInternal(
    internal::InputMethodDelegate* delegate,
    gfx::AcceleratedWidget widget) {
  if (base::win::IsTSFAwareRequired())
    return scoped_ptr<InputMethod>(new InputMethodTSF(delegate, widget));
  if (IsRemoteInputMethodWinRequired(widget))
    return CreateRemoteInputMethodWin(delegate);
  return scoped_ptr<InputMethod>(new InputMethodIMM32(delegate, widget));
}
#endif

}  // namespace

scoped_ptr<InputMethod> CreateInputMethod(
    internal::InputMethodDelegate* delegate,
    gfx::AcceleratedWidget widget) {
  if (g_input_method_set_for_testing)
    return scoped_ptr<InputMethod>(new MockInputMethod(delegate));
#if defined(OS_CHROMEOS) && defined(USE_X11)
  return scoped_ptr<InputMethod>(new InputMethodIBus(delegate));
#elif defined(OS_WIN)
  return CreateInputMethodWinInternal(delegate, widget);
#elif defined(USE_AURA) && defined(USE_X11)
  return scoped_ptr<InputMethod>(new InputMethodLinuxX11(delegate));
#else
  return scoped_ptr<InputMethod>(new InputMethodMinimal(delegate));
#endif
}

void SetUpInputMethodFactoryForTesting() {
  g_input_method_set_for_testing = true;
}

InputMethod* GetSharedInputMethod() {
#if defined(OS_WIN)
  if (!g_shared_input_method)
    g_shared_input_method = CreateInputMethod(NULL, NULL).release();
#else
  NOTREACHED();
#endif
  return g_shared_input_method;
}

namespace internal {

void DestroySharedInputMethod() {
  delete g_shared_input_method;
  g_shared_input_method = NULL;
}

}  // namespace internal
}  // namespace ui
