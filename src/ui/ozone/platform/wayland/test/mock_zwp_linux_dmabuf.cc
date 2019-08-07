// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_zwp_linux_dmabuf.h"

#include <linux-dmabuf-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/mock_buffer.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_linux_buffer_params.h"

namespace wl {

namespace {

constexpr uint32_t kLinuxDmabufVersion = 1;

void CreateParams(wl_client* client, wl_resource* resource, uint32_t id) {
  CreateResourceWithImpl<::testing::NiceMock<MockZwpLinuxBufferParamsV1>>(
      client, &zwp_linux_buffer_params_v1_interface,
      wl_resource_get_version(resource), &kMockZwpLinuxBufferParamsV1Impl, id);

  GetUserDataAs<MockZwpLinuxDmabufV1>(resource)->CreateParams(client, resource,
                                                              id);
}

}  // namespace

const struct zwp_linux_dmabuf_v1_interface kMockZwpLinuxDmabufV1Impl = {
    &DestroyResource,  // destroy
    &CreateParams,     // create_params
};

MockZwpLinuxDmabufV1::MockZwpLinuxDmabufV1()
    : GlobalObject(&zwp_linux_dmabuf_v1_interface,
                   &kMockZwpLinuxDmabufV1Impl,
                   kLinuxDmabufVersion) {}

MockZwpLinuxDmabufV1::~MockZwpLinuxDmabufV1() {}

}  // namespace wl
