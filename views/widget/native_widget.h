// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WIDGET_H_
#define VIEWS_WIDGET_NATIVE_WIDGET_H_
#pragma once

namespace views {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// NativeWidget interface
//
//  An interface implemented by an object that encapsulates rendering, event
//  handling and widget management provided by an underlying native toolkit.
//
class NativeWidget {
 public:
  virtual ~NativeWidget() {}
};

}  // namespace internal
}  // namespace views

#endif  // VIEWS_WIDGET_NATIVE_WIDGET_H_
