// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_PROPERTY_H_
#define UI_AURA_WINDOW_PROPERTY_H_
#pragma once

#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

// This header should be included by code that defines WindowProperties. It
// should not be included by code that only gets and sets WindowProperties.
//
// To define a new WindowProperty:
//
//   #include "ui/aura/window_property.h"
//
//   namespace {
//   const WindowProperty<MyType> kMyProp = {my_default_value};
//   }
//
//   FOO_EXPORT const WindowProperty<MyType>* const kMyKey = &kMyProp;
//
//   // outside all namespaces:
//   DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(FOO_EXPORT, MyType)
//
// If the property is not exported, use DECLARE_WINDOW_PROPERTY_TYPE(MyType),
// which is a shorthand for DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(, MyType).

namespace aura {
namespace {

// No single new-style cast works for every conversion to/from intptr_t, so we
// need this helper class. A third specialization is needed for bool because
// MSVC warning C4800 (forcing value to bool) is not suppressed by an explicit
// cast (!).
template<typename T>
class WindowPropertyCaster {
 public:
  static intptr_t ToIntptrT(T x) { return static_cast<intptr_t>(x); }
  static T FromIntptrT(intptr_t x) { return static_cast<T>(x); }
};
template<typename T>
class WindowPropertyCaster<T*> {
 public:
  static intptr_t ToIntptrT(T* x) { return reinterpret_cast<intptr_t>(x); }
  static T* FromIntptrT(intptr_t x) { return reinterpret_cast<T*>(x); }
};
template<>
class WindowPropertyCaster<bool> {
 public:
  static intptr_t ToIntptrT(bool x) { return static_cast<intptr_t>(x); }
  static bool FromIntptrT(intptr_t x) { return x != 0; }
};

}  // namespace

template<typename T>
struct WindowProperty {
  T default_value;
};

template<typename T>
void Window::SetProperty(const WindowProperty<T>* property, T value) {
  SetPropertyInternal(
      property,
      WindowPropertyCaster<T>::ToIntptrT(value),
      WindowPropertyCaster<T>::ToIntptrT(property->default_value));
}

template<typename T>
T Window::GetProperty(const WindowProperty<T>* property) const {
  return WindowPropertyCaster<T>::FromIntptrT(GetPropertyInternal(
      property, WindowPropertyCaster<T>::ToIntptrT(property->default_value)));
}

template<typename T>
void Window::ClearProperty(const WindowProperty<T>* property) {
  intptr_t default_value =
      WindowPropertyCaster<T>::ToIntptrT(property->default_value);
  SetPropertyInternal(property, default_value, default_value);
}

}  // namespace aura

// Macros to instantiate the property getter/setter template functions.
#define DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(EXPORT, T)  \
    template EXPORT void aura::Window::SetProperty(         \
        const aura::WindowProperty<T >*, T);                \
    template EXPORT T aura::Window::GetProperty(            \
        const aura::WindowProperty<T >*) const;             \
    template EXPORT void aura::Window::ClearProperty(       \
        const aura::WindowProperty<T >*);
#define DECLARE_WINDOW_PROPERTY_TYPE(T)  \
    DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(, T)

#endif  // UI_AURA_WINDOW_PROPERTY_H_
