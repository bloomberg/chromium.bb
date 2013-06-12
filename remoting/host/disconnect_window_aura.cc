// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

// TODO(lambroslambrou): Implement this for Aura - see http://crbug.com/247213.
#if defined(OS_LINUX)
#include "remoting/host/disconnect_window_gtk.cc"
#else
#error Not implemented.
#endif
