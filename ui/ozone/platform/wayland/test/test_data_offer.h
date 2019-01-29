// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_OFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_OFFER_H_

#include <string>

#include <wayland-server-protocol-core.h>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct wl_data_offer_interface kTestDataOfferImpl;

class TestDataOffer : public ServerObject {
 public:
  explicit TestDataOffer(wl_resource* resource);
  ~TestDataOffer() override;

  void Receive(const std::string& mime_type, base::ScopedFD fd);
  void OnOffer(const std::string& mime_type);

 private:
  // TODO(adunaev): get rid of this in favor of using a task runner.
  base::Thread io_thread_;
  base::WeakPtrFactory<TestDataOffer> write_data_weak_ptr_factory_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_OFFER_H_
