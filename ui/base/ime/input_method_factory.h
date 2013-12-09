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

template <typename T> struct DefaultSingletonTraits;

namespace ui {
namespace internal {
class InputMethodDelegate;
}  // namespace internal

class InputMethod;

class UI_EXPORT InputMethodFactory {
 public:
  // Returns the current active factory.
  // If no factory was set, sets the DefaultInputMethodFactory by default.  Once
  // a factory was set, you cannot change the factory, and always the same
  // factory is returned.
  static InputMethodFactory* GetInstance();

  // Sets an InputMethodFactory to be used.
  // This function must be called at most once.  |instance| is not owned by this
  // class or marked automatically as a leaky object.  It's a caller's duty to
  // destroy the object or mark it as leaky.
  static void SetInstance(InputMethodFactory* instance);

  virtual ~InputMethodFactory() {}

  // Creates and returns an input method implementation.
  virtual scoped_ptr<InputMethod> CreateInputMethod(
      internal::InputMethodDelegate* delegate,
      gfx::AcceleratedWidget widget) = 0;

 private:
  static void ClearInstance();

  friend UI_EXPORT void ShutdownInputMethod();
  friend UI_EXPORT void ShutdownInputMethodForTesting();
};

class DefaultInputMethodFactory : public InputMethodFactory {
 public:
  // For Singleton
  static DefaultInputMethodFactory* GetInstance();

  // Overridden from InputMethodFactory.
  virtual scoped_ptr<InputMethod> CreateInputMethod(
      internal::InputMethodDelegate* delegate,
      gfx::AcceleratedWidget widget) OVERRIDE;

 private:
  DefaultInputMethodFactory() {}

  friend struct DefaultSingletonTraits<DefaultInputMethodFactory>;

  DISALLOW_COPY_AND_ASSIGN(DefaultInputMethodFactory);
};

class MockInputMethodFactory : public InputMethodFactory {
 public:
  // For Singleton
  static MockInputMethodFactory* GetInstance();

  // Overridden from InputMethodFactory.
  virtual scoped_ptr<InputMethod> CreateInputMethod(
      internal::InputMethodDelegate* delegate,
      gfx::AcceleratedWidget widget) OVERRIDE;

 private:
  MockInputMethodFactory() {}

  friend struct DefaultSingletonTraits<MockInputMethodFactory>;

  DISALLOW_COPY_AND_ASSIGN(MockInputMethodFactory);
};

// Shorthand for
// InputMethodFactory::GetInstance()->CreateInputMethod(delegate, widget).
UI_EXPORT scoped_ptr<InputMethod> CreateInputMethod(
    internal::InputMethodDelegate* delegate,
    gfx::AcceleratedWidget widget);

// Shorthand for InputMethodFactory::SetInstance(new MockInputMethodFactory()).
// TODO(yukishiino): Retires this shorthand, and makes ui::InitializeInputMethod
// and ui::InitializeInputMethodForTesting set the appropriate factory.
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
