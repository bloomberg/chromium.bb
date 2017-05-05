// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8PagePopupControllerBinding_h
#define V8PagePopupControllerBinding_h

#include "platform/bindings/WrapperTypeInfo.h"
#include "platform/wtf/Allocator.h"
#include "v8/include/v8.h"

namespace blink {

// FIXME: This class was introduced in order to just support
// window.pagePopupController which is used by internal implementation of form
// popups.  Form popups should be implemented in another way, and this class
// should be removed.
class V8PagePopupControllerBinding {
  STATIC_ONLY(V8PagePopupControllerBinding);

 public:
  // Installs 'pagePopupController' attribute into 'window' object.
  static void InstallPagePopupController(v8::Local<v8::Context>,
                                         v8::Local<v8::Object> window_wrapper);
};

}  // namespace blink

#endif  // V8PagePopupControllerBinding_h
