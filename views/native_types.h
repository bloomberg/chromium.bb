// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_NATIVE_TYPES_H_
#define VIEWS_NATIVE_TYPES_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

namespace views {

#if defined(OS_WIN)
typedef MSG NativeEvent;
#endif
#if defined(OS_LINUX)
typedef union _GdkEvent GdkEvent;
typedef GdkEvent* NativeEvent;
#endif
#if defined(TOUCH_UI)
typedef union _XEvent XEvent;
typedef XEvent* NativeEvent;
#endif

}  // namespace views

#endif  // VIEWS_NATIVE_TYPES_H_

