// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_factory.h"

#include "base/memory/singleton.h"
#include "ui/base/ime/mock_input_method.h"

#if defined(OS_CHROMEOS) && defined(USE_X11)
#include "ui/base/ime/input_method_ibus.h"
#elif defined(OS_WIN)
#include "base/win/metro.h"
#include "ui/base/ime/input_method_imm32.h"
#include "ui/base/ime/input_method_tsf.h"
#include "ui/base/ime/remote_input_method_win.h"
#elif defined(USE_AURA) && defined(USE_X11)
#include "ui/base/ime/input_method_auralinux.h"
#else
#include "ui/base/ime/input_method_minimal.h"
#endif

namespace {

ui::InputMethodFactory* g_input_method_factory = NULL;

#if defined(OS_WIN)
ui::InputMethod* g_shared_input_method = NULL;
#endif

}  // namespace

namespace ui {

// static
InputMethodFactory* InputMethodFactory::GetInstance() {
  if (!g_input_method_factory)
    SetInstance(DefaultInputMethodFactory::GetInstance());

  return g_input_method_factory;
}

// static
void InputMethodFactory::SetInstance(InputMethodFactory* instance) {
  CHECK(!g_input_method_factory);
  CHECK(instance);

  g_input_method_factory = instance;
}

// static
void InputMethodFactory::ClearInstance() {
  // It's a client's duty to delete the object.
  g_input_method_factory = NULL;
}

// DefaultInputMethodFactory

// static
DefaultInputMethodFactory* DefaultInputMethodFactory::GetInstance() {
  return Singleton<DefaultInputMethodFactory>::get();
}

scoped_ptr<InputMethod> DefaultInputMethodFactory::CreateInputMethod(
    internal::InputMethodDelegate* delegate,
    gfx::AcceleratedWidget widget) {
#if defined(OS_CHROMEOS) && defined(USE_X11)
  return scoped_ptr<InputMethod>(new InputMethodIBus(delegate));
#elif defined(OS_WIN)
  if (base::win::IsTSFAwareRequired())
    return scoped_ptr<InputMethod>(new InputMethodTSF(delegate, widget));
  if (IsRemoteInputMethodWinRequired(widget))
    return CreateRemoteInputMethodWin(delegate);
  return scoped_ptr<InputMethod>(new InputMethodIMM32(delegate, widget));
#elif defined(USE_AURA) && defined(USE_X11)
  return scoped_ptr<InputMethod>(new InputMethodAuraLinux(delegate));
#else
  return scoped_ptr<InputMethod>(new InputMethodMinimal(delegate));
#endif
}

// MockInputMethodFactory

// static
MockInputMethodFactory* MockInputMethodFactory::GetInstance() {
  return Singleton<MockInputMethodFactory>::get();
}

scoped_ptr<InputMethod> MockInputMethodFactory::CreateInputMethod(
    internal::InputMethodDelegate* delegate,
    gfx::AcceleratedWidget /* widget */) {
  return scoped_ptr<InputMethod>(new MockInputMethod(delegate));
}

// Shorthands

scoped_ptr<InputMethod> CreateInputMethod(
    internal::InputMethodDelegate* delegate,
    gfx::AcceleratedWidget widget) {
  return InputMethodFactory::GetInstance()->CreateInputMethod(delegate, widget);
}

void SetUpInputMethodFactoryForTesting() {
  InputMethodFactory::SetInstance(MockInputMethodFactory::GetInstance());
}

#if defined(OS_WIN)
InputMethod* GetSharedInputMethod() {
  if (!g_shared_input_method)
    g_shared_input_method = CreateInputMethod(NULL, NULL).release();
  return g_shared_input_method;
}

namespace internal {

void DestroySharedInputMethod() {
  delete g_shared_input_method;
  g_shared_input_method = NULL;
}

}  // namespace internal
#endif

}  // namespace ui
