// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_pointer.h"

namespace wl {

const struct wl_pointer_interface kTestPointerImpl = {
    nullptr,           // set_cursor
    &DestroyResource,  // release
};

TestPointer::TestPointer(wl_resource* resource) : ServerObject(resource) {}

TestPointer::~TestPointer() = default;

}  // namespace wl
