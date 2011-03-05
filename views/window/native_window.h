// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WINDOW_H_
#define VIEWS_WIDGET_NATIVE_WINDOW_H_
#pragma once

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWindow interface
//
//  An interface implemented by an object that encapsulates a native window.
//
class NativeWindow {
 public:
  virtual ~NativeWindow() {}
};

}  // namespace views

#endif  // VIEWS_WIDGET_NATIVE_WINDOW_H_
