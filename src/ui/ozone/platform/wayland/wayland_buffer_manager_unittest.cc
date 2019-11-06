// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"

#include <drm_fourcc.h>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/mock_callback.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using testing::_;

namespace ui {

namespace {

using MockTerminateGpuCallback =
    base::MockCallback<base::OnceCallback<void(std::string)>>;

constexpr gfx::Size kDefaultSize(1024, 768);

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

}  // namespace

class WaylandBufferManagerTest : public WaylandTest {
 public:
  WaylandBufferManagerTest() = default;
  ~WaylandBufferManagerTest() override = default;

  void SetUp() override {
    WaylandTest::SetUp();

    manager_host_ = connection_->buffer_manager_host();
    EXPECT_TRUE(manager_host_);

    // Use the helper methods below, which automatically set the termination
    // callback and bind the interface again if the manager failed.
    manager_host_->SetTerminateGpuCallback(callback_.Get());
    auto interface_ptr = manager_host_->BindInterface();
    buffer_manager_gpu_->SetWaylandBufferManagerHost(std::move(interface_ptr));
  }

 protected:
  base::ScopedFD MakeFD() {
    base::FilePath temp_path;
    EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
    auto file =
        base::File(temp_path, base::File::FLAG_READ | base::File::FLAG_WRITE |
                                  base::File::FLAG_CREATE_ALWAYS);
    return base::ScopedFD(file.TakePlatformFile());
  }

  // Sets the terminate gpu callback expectation, calls OnChannelDestroyed,
  // sets the same callback again and re-establishes mojo connection again
  // for convenience.
  void SetTerminateCallbackExpectationAndDestroyChannel(
      MockTerminateGpuCallback* callback,
      bool fail) {
    if (!fail) {
      // To avoid warning messages as "Expected to be never called, but has 0
      // WillOnce()s", split the expecations based on the expected call times.
      EXPECT_CALL(*callback, Run(_)).Times(0);
    } else {
      EXPECT_CALL(*callback, Run(_))
          .Times(1)
          .WillRepeatedly(::testing::Invoke([this, callback](std::string) {
            manager_host_->OnChannelDestroyed();
            manager_host_->SetTerminateGpuCallback(callback->Get());

            auto interface_ptr = manager_host_->BindInterface();
            buffer_manager_gpu_->SetWaylandBufferManagerHost(
                std::move(interface_ptr));
          }));
    }
  }

  void CreateDmabufBasedBufferAndSetTerminateExpecation(
      bool fail,
      gfx::AcceleratedWidget widget,
      uint32_t buffer_id,
      base::ScopedFD fd = base::ScopedFD(),
      const gfx::Size& size = kDefaultSize,
      const std::vector<uint32_t>& strides = {1},
      const std::vector<uint32_t>& offsets = {2},
      const std::vector<uint64_t>& modifiers = {3},
      uint32_t format = DRM_FORMAT_R8,
      uint32_t planes_count = 1) {
    if (!fd.is_valid())
      fd = MakeFD();

    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, fail);
    buffer_manager_gpu_->CreateDmabufBasedBuffer(
        widget, std::move(fd), kDefaultSize, strides, offsets, modifiers,
        format, planes_count, buffer_id);

    Sync();
  }

  void CreateShmBasedBufferAndSetTerminateExpecation(
      bool fail,
      gfx::AcceleratedWidget widget,
      uint32_t buffer_id,
      const gfx::Size& size = kDefaultSize,
      size_t length = 0) {
    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, fail);

    if (!length)
      length = size.width() * size.height() * 4;
    buffer_manager_gpu_->CreateShmBasedBuffer(widget, MakeFD(), length, size,
                                              buffer_id);

    Sync();
  }

  void DestroyBufferAndSetTerminateExpectation(gfx::AcceleratedWidget widget,
                                               uint32_t buffer_id,
                                               bool fail) {
    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, fail);

    buffer_manager_gpu_->DestroyBuffer(widget, buffer_id);

    Sync();
  }

  MockTerminateGpuCallback callback_;
  WaylandBufferManagerHost* manager_host_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManagerTest);
};

TEST_P(WaylandBufferManagerTest, CreateDmabufBasedBuffers) {
  constexpr uint32_t kDmabufBufferId = 1;

  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  const gfx::AcceleratedWidget widget = window_->GetWidget();

  CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                   kDmabufBufferId);
  DestroyBufferAndSetTerminateExpectation(widget, kDmabufBufferId,
                                          false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, CreateShmBasedBuffers) {
  constexpr uint32_t kShmBufferId = 1;

  const gfx::AcceleratedWidget widget = window_->GetWidget();

  CreateShmBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                kShmBufferId);
  // The state is reset now and there are no buffers to destroy.
  DestroyBufferAndSetTerminateExpectation(widget, kShmBufferId, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, ValidateDataFromGpu) {
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
  };

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  for (const auto& bad : kBadInputs) {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(0);
    base::ScopedFD dummy;
    CreateDmabufBasedBufferAndSetTerminateExpecation(
        true /*fail*/, widget, bad.buffer_id,
        bad.has_file ? MakeFD() : std::move(dummy), bad.size, bad.strides,
        bad.offsets, bad.modifiers, bad.format, bad.planes_count);
  }

  constexpr uint32_t kBufferId = 1;

  // Create a buffer so it gets registered with the given ID.
  // This must be the only buffer that is asked to be created.
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                   kBufferId);

  // It must be impossible to create a buffer with the same id.
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(0);
  CreateDmabufBasedBufferAndSetTerminateExpecation(true /*fail*/, widget,
                                                   kBufferId);

  // Create the buffer again and try to destroy it.
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                   kBufferId);

  // The destruction of the previously created buffer must be ok.
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId, false /*fail*/);

  // Destroying non-existing buffer triggers the termination callback.
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId, true /*fail*/);
}

TEST_P(WaylandBufferManagerTest, CreateAndDestroyBuffer) {
  const uint32_t kBufferId1 = 1;
  const uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();

  // This section tests that it is impossible to create buffers with the same
  // id.
  {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(2);
    CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                     kBufferId1);
    CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                     kBufferId2);

    // Can't create buffer with existing id.
    CreateDmabufBasedBufferAndSetTerminateExpecation(true /*fail*/, widget,
                                                     kBufferId2);
    // Can't destroy buffer with non-existing id (the manager cleared the state
    // after the previous failure).
    DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, true /*fail*/);
  }

  // This section tests that it is impossible to destroy buffers with
  // non-existing ids (for example, if the have already been destroyed).
  {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
    CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                     kBufferId2);
    DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, false /*fail*/);
    // Can't destroy the same buffer twice (non-existing id).
    DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, true /*fail*/);
  }
}

INSTANTIATE_TEST_SUITE_P(XdgVersionV5Test,
                         WaylandBufferManagerTest,
                         ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandBufferManagerTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
