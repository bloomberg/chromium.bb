// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_SOURCE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include <wayland-server-protocol-core.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace base {
class SequencedTaskRunner;
}

namespace wl {

extern const struct wl_data_source_interface kTestDataSourceImpl;

class TestDataSource : public ServerObject {
 public:
  explicit TestDataSource(wl_resource* resource);
  ~TestDataSource() override;

  void Offer(const std::string& mime_type);

  using ReadDataCallback =
      base::OnceCallback<void(const std::vector<uint8_t>&)>;
  void ReadData(ReadDataCallback);

  void OnCancelled();

 private:
  void DataReadCb(ReadDataCallback callback, const std::vector<uint8_t>& data);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<TestDataSource> read_data_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestDataSource);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_SOURCE_H_
