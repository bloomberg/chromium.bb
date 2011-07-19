// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

namespace remoting {

const char ContinueWindow::kTitle[] = "Remoting";
const char ContinueWindow::kMessage[] =
    "You are currently sharing this machine with another user. "
    "Please confirm that you want to continue sharing.";
const char ContinueWindow::kDefaultButtonText[] = "Continue";
const char ContinueWindow::kCancelButtonText[] = "Stop";

}
