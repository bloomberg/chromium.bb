// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_buffer_manager.h"

#include <memory>

#include <drm_fourcc.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/wayland_test.h"

using testing::_;

namespace ui {

namespace {

constexpr uint32_t kWidth = 1024;
constexpr uint32_t kHeight = 768;

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
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t planes_count = 0;
    std::vector<uint32_t> strides;
    std::vector<uint32_t> offsets;
    std::vector<uint64_t> modifiers;
    uint32_t format = 0;
    uint32_t buffer_id = 0;
  };

  constexpr uint32_t kExistingBufferId = 1;
  constexpr uint32_t kNonExistingBufferId = 2;

  // Create a buffer through the connection's interface so it gets
  // registered with the given ID.
  // This must be the only buffer that is asked to be created.
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);

  connection_->CreateZwpLinuxDmabuf(MakeTempFile(), kWidth, kHeight, {1}, {2},
                                    DRM_FORMAT_R8, {3}, 1, kExistingBufferId);
  Sync();

  const InputData kBadInputs[] = {
      // All zeros.
      {},
      // Valid file but zeros everywhereelse.
      {true},
      // Valid file, invalid size, zeros elsewhere.
      {true, kWidth},
      {true, 0, kHeight},
      // Valid file and size but zeros in other fields.
      {true, kWidth, kHeight},
      // Vectors have different lengths.
      {true, kWidth, kHeight, 1, {1}, {2, 3}, {4, 5, 6}},
      // Vectors have same lengths but strides have a zero.
      {true, kWidth, kHeight, 1, {0}, {2}, {6}},
      // Vectors are valid but buffer format is not.
      {true, kWidth, kHeight, 1, {1}, {2}, {6}},
      // Everything is correct but the buffer ID is zero.
      {true, kWidth, kHeight, 1, {1}, {2}, {6}, DRM_FORMAT_R8},
      // Everything is correct but the buffer ID .
      {true,
       kWidth,
       kHeight,
       1,
       {1},
       {2},
       {6},
       DRM_FORMAT_R8,
       kExistingBufferId},
  };

  WaylandBufferManager* manager = connection_->buffer_manager_for_tests();
  ASSERT_TRUE(manager);

  auto temp_file = MakeTempFile();
  for (const auto& bad : kBadInputs) {
    base::File dummy;
    EXPECT_FALSE(manager->ValidateDataFromGpu(
        bad.has_file ? temp_file : dummy, bad.width, bad.height, bad.strides,
        bad.offsets, bad.format, bad.modifiers, bad.planes_count,
        bad.buffer_id));
    EXPECT_FALSE(manager->error_message().empty());
  }

  EXPECT_TRUE(manager->ValidateDataFromGpu(temp_file, kWidth, kHeight, {1}, {2},
                                           DRM_FORMAT_R8, {3}, 1,
                                           kNonExistingBufferId));

  connection_->DestroyZwpLinuxDmabuf(kExistingBufferId);
}

TEST_P(WaylandBufferManagerTest, CreateAndDestroyBuffer) {
  WaylandBufferManager* manager = connection_->buffer_manager_for_tests();
  ASSERT_TRUE(manager);

  const uint32_t kBufferId1 = 1;
  const uint32_t kBufferId2 = 2;

  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(2);

  EXPECT_TRUE(manager->CreateBuffer(MakeTempFile(), kWidth, kHeight, {1}, {2},
                                    DRM_FORMAT_R8, {3}, 1, kBufferId1));
  EXPECT_FALSE(manager->CreateBuffer(MakeTempFile(), kWidth, kHeight, {1}, {2},
                                     DRM_FORMAT_R8, {3}, 1, kBufferId1));
  EXPECT_FALSE(manager->DestroyBuffer(kBufferId2));
  EXPECT_TRUE(manager->CreateBuffer(MakeTempFile(), kWidth, kHeight, {1}, {2},
                                    DRM_FORMAT_R8, {3}, 1, kBufferId2));

  EXPECT_TRUE(manager->DestroyBuffer(kBufferId1));
  EXPECT_FALSE(manager->DestroyBuffer(kBufferId1));
  EXPECT_TRUE(manager->DestroyBuffer(kBufferId2));
  EXPECT_FALSE(manager->DestroyBuffer(kBufferId2));
}

INSTANTIATE_TEST_SUITE_P(XdgVersionV5Test,
                         WaylandBufferManagerTest,
                         ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandBufferManagerTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
