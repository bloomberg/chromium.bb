// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zwp_text_input_manager.h"

#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"

namespace wl {

namespace {

constexpr uint32_t kTextInputManagerVersion = 1;

void CreateTextInput(struct wl_client* client,
                     struct wl_resource* resource,
                     uint32_t id) {
  auto* im = static_cast<TestZwpTextInputManagerV1*>(
      wl_resource_get_user_data(resource));
  wl_resource* text_resource =
      wl_resource_create(client, &zwp_text_input_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!text_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  SetImplementation(text_resource, &kMockZwpTextInputV1Impl,
                    std::make_unique<MockZwpTextInput>(text_resource));
  im->set_text_input(GetUserDataAs<MockZwpTextInput>(text_resource));
}

}  // namespace

const struct zwp_text_input_manager_v1_interface
    kTestZwpTextInputManagerV1Impl = {
        &CreateTextInput,  // create_text_input
};

TestZwpTextInputManagerV1::TestZwpTextInputManagerV1()
    : GlobalObject(&zwp_text_input_manager_v1_interface,
                   &kTestZwpTextInputManagerV1Impl,
                   kTextInputManagerVersion) {}

TestZwpTextInputManagerV1::~TestZwpTextInputManagerV1() {}

}  // namespace wl
