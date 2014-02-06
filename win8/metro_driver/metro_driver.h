// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_METRO_DRIVER_H_
#define WIN8_METRO_DRIVER_METRO_DRIVER_H_

#include "stdafx.h"

class ChromeAppViewFactory
    : public mswr::RuntimeClass<winapp::Core::IFrameworkViewSource> {
 public:
  ChromeAppViewFactory(winapp::Core::ICoreApplication* icore_app,
                       LPTHREAD_START_ROUTINE host_main,
                       void* host_context);
  IFACEMETHOD(CreateView)(winapp::Core::IFrameworkView** view);
};

#endif  // WIN8_METRO_DRIVER_METRO_DRIVER_H_