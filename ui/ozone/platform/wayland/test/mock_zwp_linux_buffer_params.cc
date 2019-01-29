// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_zwp_linux_buffer_params.h"

#include "ui/ozone/platform/wayland/test/mock_buffer.h"

namespace wl {

namespace {

void Add(wl_client* client,
         wl_resource* resource,
         int32_t fd,
         uint32_t plane_idx,
         uint32_t offset,
         uint32_t stride,
         uint32_t modifier_hi,
         uint32_t modifier_lo) {
  auto* buffer_params = GetUserDataAs<MockZwpLinuxBufferParamsV1>(resource);

  buffer_params->fds_.emplace_back(fd);
  buffer_params->Add(client, resource, fd, plane_idx, offset, stride,
                     modifier_hi, modifier_lo);
}

void Create(wl_client* client,
            wl_resource* resource,
            int32_t width,
            int32_t height,
            uint32_t format,
            uint32_t flags) {
  auto* buffer_params = GetUserDataAs<MockZwpLinuxBufferParamsV1>(resource);

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, 0);

  SetImplementation(buffer_resource, &kMockWlBufferImpl,
                    std::make_unique<MockBuffer>(
                        buffer_resource, std::move(buffer_params->fds_)));

  zwp_linux_buffer_params_v1_send_created(resource, buffer_resource);

  GetUserDataAs<MockZwpLinuxBufferParamsV1>(resource)->Create(
      client, resource, width, height, format, flags);
}

void CreateImmed(wl_client* client,
                 wl_resource* resource,
                 uint32_t buffer_id,
                 int32_t width,
                 int32_t height,
                 uint32_t format,
                 uint32_t flags) {
  GetUserDataAs<MockZwpLinuxBufferParamsV1>(resource)->CreateImmed(
      client, resource, buffer_id, width, height, format, flags);
}

}  // namespace

const struct zwp_linux_buffer_params_v1_interface
    kMockZwpLinuxBufferParamsV1Impl = {&DestroyResource, &Add, &Create,
                                       &CreateImmed};

MockZwpLinuxBufferParamsV1::MockZwpLinuxBufferParamsV1(wl_resource* resource)
    : ServerObject(resource) {}

MockZwpLinuxBufferParamsV1::~MockZwpLinuxBufferParamsV1() {}

}  // namespace wl
