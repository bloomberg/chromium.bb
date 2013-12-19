// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_FACTORY_H_
#define UI_BASE_IME_INPUT_METHOD_FACTORY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
namespace internal {
class InputMethodDelegate;
}  // namespace internal

class InputMethod;

// Creates a new instance of InputMethod and returns it.
UI_EXPORT scoped_ptr<InputMethod> CreateInputMethod(
    internal::InputMethodDelegate* delegate,
    gfx::AcceleratedWidget widget);

// Makes CreateInputMethod return a MockInputMethod.
UI_EXPORT void SetUpInputMethodFactoryForTesting();

#if defined(OS_WIN)
// Returns a shared input method object for the platform. Caller must not
// delete the object. Currently supported only on Windows. This method is
// for non-Aura environment, where only one input method object is created for
// the browser process.
UI_EXPORT InputMethod* GetSharedInputMethod();

namespace internal {
// Destroys the shared input method object returned by GetSharedInputMethod().
// This function must be called only from input_method_initializer.cc.
void DestroySharedInputMethod();
}  // namespace internal
#endif

}  // namespace ui;

#endif  // UI_BASE_IME_INPUT_METHOD_FACTORY_H_
