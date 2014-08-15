// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/single_window_input_injector.h"

namespace remoting {

scoped_ptr<InputInjector> SingleWindowInputInjector::CreateForWindow(
    webrtc::WindowId window_id,
    scoped_ptr<InputInjector> input_injector) {
  return scoped_ptr<InputInjector>();
}

} // namespace remoting
