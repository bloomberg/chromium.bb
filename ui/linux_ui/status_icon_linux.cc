// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux_ui/status_icon_linux.h"

StatusIconLinux::Delegate::~Delegate() {
}

StatusIconLinux::StatusIconLinux() : delegate_(NULL) {
}

StatusIconLinux::~StatusIconLinux() {
}
