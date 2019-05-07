// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager.h"

#include <memory>

#include <drm_fourcc.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using testing::_;

namespace ui {

namespace {

constexpr gfx::Size kDefaultSize(1024, 768);

}  // namespace

class WaylandBufferManagerTest : public WaylandTest {
 public:
  WaylandBufferManagerTest() = default;
  ~WaylandBufferManagerTest() override = default;

 protected:
  base::File MakeTempFile() {
    base::FilePath temp_path;
    EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
    return base::File(temp_path, base::File::FLAG_READ |
                                     base::File::FLAG_WRITE |
                                     base::File::FLAG_CREATE_ALWAYS);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManagerTest);
};

TEST_P(WaylandBufferManagerTest, ValidateDataFromGpu) {
  struct InputData {
    bool has_file = false;
    gfx::Size size;
    uint32_t planes_count = 0;
    std::vector<uint32_t> strides;
    std::vector<uint32_t> offsets;
    std::vector<uint64_t> modifiers;
    uint32_t format = 0;
    uint32_t buffer_id = 0;
  };

  constexpr uint32_t kExistingBufferId = 1;
  constexpr uint32_t kNonExistingBufferId = 2;

  WaylandBufferManager* manager = connection_->buffer_manager();
  ASSERT_TRUE(manager);

  // Create a buffer so it gets registered with the given ID.
  // This must be the only buffer that is asked to be created.
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  const gfx::AcceleratedWidget widget = window_->GetWidget();
  manager->CreateBuffer(widget, MakeTempFile(), kDefaultSize, {1}, {2}, {3},
                        DRM_FORMAT_R8, 1, kExistingBufferId);
  Sync();

  const InputData kBadInputs[] = {
      // All zeros.
      {},
      // Valid file but zeros everywhereelse.
      {true},
      // Valid file, invalid size, zeros elsewhere.
      {true, {kDefaultSize.width(), 0}},
      {true, {0, kDefaultSize.height()}},
      // Valid file and size but zeros in other fields.
      {true, kDefaultSize},
      // Vectors have different lengths.
      {true, kDefaultSize, 1, {1}, {2, 3}, {4, 5, 6}},
      // Vectors have same lengths but strides have a zero.
      {true, kDefaultSize, 1, {0}, {2}, {6}},
      // Vectors are valid but buffer format is not.
      {true, kDefaultSize, 1, {1}, {2}, {6}},
      // Everything is correct but the buffer ID is zero.
      {true, kDefaultSize, 1, {1}, {2}, {6}, DRM_FORMAT_R8},
      // Everything is correct but the buffer ID .
      {true, kDefaultSize, 1, {1}, {2}, {6}, DRM_FORMAT_R8, kExistingBufferId},
  };

  for (const auto& bad : kBadInputs) {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(0);
    base::File dummy;
    EXPECT_FALSE(manager->CreateBuffer(
        widget, bad.has_file ? MakeTempFile() : std::move(dummy), bad.size,
        bad.strides, bad.offsets, bad.modifiers, bad.format, bad.planes_count,
        bad.buffer_id));
    EXPECT_FALSE(manager->error_message().empty());
  }

  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  EXPECT_TRUE(manager->CreateBuffer(widget, MakeTempFile(), kDefaultSize, {1},
                                    {2}, {3}, DRM_FORMAT_R8, 1,
                                    kNonExistingBufferId));

  EXPECT_TRUE(manager->DestroyBuffer(widget, kNonExistingBufferId));
  EXPECT_TRUE(manager->DestroyBuffer(widget, kExistingBufferId));
}

TEST_P(WaylandBufferManagerTest, CreateAndDestroyBuffer) {
  WaylandBufferManager* manager = connection_->buffer_manager();
  ASSERT_TRUE(manager);

  const uint32_t kBufferId1 = 1;
  const uint32_t kBufferId2 = 2;

  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(2);

  const gfx::AcceleratedWidget widget = window_->GetWidget();

  EXPECT_TRUE(manager->CreateBuffer(widget, MakeTempFile(), kDefaultSize, {1},
                                    {2}, {3}, DRM_FORMAT_R8, 1, kBufferId1));
  EXPECT_FALSE(manager->CreateBuffer(widget, MakeTempFile(), kDefaultSize, {1},
                                     {2}, {3}, DRM_FORMAT_R8, 1, kBufferId1));
  EXPECT_FALSE(manager->DestroyBuffer(widget, kBufferId2));
  EXPECT_TRUE(manager->CreateBuffer(widget, MakeTempFile(), kDefaultSize, {1},
                                    {2}, {3}, DRM_FORMAT_R8, 1, kBufferId2));

  EXPECT_TRUE(manager->DestroyBuffer(widget, kBufferId1));
  EXPECT_FALSE(manager->DestroyBuffer(widget, kBufferId1));
  EXPECT_TRUE(manager->DestroyBuffer(widget, kBufferId2));
  EXPECT_FALSE(manager->DestroyBuffer(widget, kBufferId2));
}

INSTANTIATE_TEST_SUITE_P(XdgVersionV5Test,
                         WaylandBufferManagerTest,
                         ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandBufferManagerTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
