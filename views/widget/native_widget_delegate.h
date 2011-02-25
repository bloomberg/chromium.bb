// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WIDGET_DELEGATE_H_
#define VIEWS_WIDGET_NATIVE_WIDGET_DELEGATE_H_
#pragma once

namespace views {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetDelegate
//
//  An interface implemented by the object that handles events sent by a
//  NativeWidget implementation.
//
class NativeWidgetDelegate {
 public:
  virtual ~NativeWidgetDelegate() {}
};

}  // namespace internal
}  // namespace views

#endif  // VIEWS_WIDGET_NATIVE_WIDGET_DELEGATE_H_
